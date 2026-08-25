#include "hodion/predict.h"

#include <vector>

#include <algorithm>
#include <cmath>

#include "hodion/reconvert.h"

namespace hodion {
namespace {

// Số phương án tối đa xét cho mỗi âm tiết. Thực tế nhiều nhất khoảng 20;
// chặn trên để một chuỗi lạ không làm bùng số trạng thái.
constexpr size_t kMaxCandidates = 24;

// Các cách viết có dấu CÓ THẬT của một chuỗi không dấu.
std::vector<std::u32string> RealVariants(const std::u32string& word,
                                         const Config& cfg,
                                         const SyllableSet& known) {
  std::vector<std::u32string> out;
  for (const std::u32string& v :
       syllable_variants(word, cfg, kMaxCandidates * 4, &known)) {
    if (known.contains(v)) out.push_back(v);
    if (out.size() >= kMaxCandidates) break;
  }
  return out;
}

}  // namespace

std::u32string restore_diacritics(const std::u32string& word,
                                  const Config& cfg,
                                  const SyllableSet& known) {
  if (word.empty()) return {};
  // Đã có dấu thì người dùng đã nói rõ họ muốn gì.
  if (strip_diacritics(word) != word) return {};

  const std::vector<std::u32string> variants =
      syllable_variants(word, cfg, 128, &known);

  // Chỉ nhận khi có ĐÚNG MỘT âm tiết có thật. Hai cái trở lên là nhập
  // nhằng, và không có ngữ cảnh thì chọn bừa còn tệ hơn để nguyên.
  std::u32string answer;
  int found = 0;
  for (const std::u32string& v : variants) {
    if (!known.contains(v)) continue;
    if (++found > 1) return {};
    answer = v;
  }
  if (found != 1) return {};
  if (answer == word) return {};  // không có gì để đổi
  return answer;
}

std::u32string restore_in_context(const std::vector<std::u32string>& left,
                                  const std::u32string& word,
                                  const Config& cfg, const SyllableSet& known,
                                  const NgramModel& model, float margin) {
  if (model.empty() || word.empty()) return {};
  if (strip_diacritics(word) != word) return {};

  const std::vector<std::u32string> candidates = RealVariants(word, cfg, known);
  if (candidates.size() < 2) {
    // Không nhập nhằng: để restore_diacritics lo, không cần mô hình.
    return {};
  }

  // Ngữ cảnh: hai âm tiết ngay trước. Thiếu thì coi như đầu câu.
  const size_t n = left.size();
  const int a = n >= 2 ? model.id(left[n - 2]) : model.bos();
  const int b = n >= 1 ? model.id(left[n - 1]) : model.bos();

  float best = 0, second = 0;
  const std::u32string* winner = nullptr;
  for (const std::u32string& c : candidates) {
    const int cid = model.id(c);
    if (cid == NgramModel::kNoWord) continue;
    const float s = model.score(a, b, cid);
    if (!winner || s > best) {
      second = winner ? best : s;
      best = s;
      winner = &c;
    } else if (s > second || !winner) {
      second = s;
    }
  }
  if (!winner) return {};
  if (best - second < margin) return {};  // gần như hoà → không quyết
  if (*winner == word) return {};          // không có gì để đổi
  return *winner;
}

std::vector<std::u32string> rank_candidates(
    const std::vector<std::u32string>& left, const std::u32string& word,
    const Config& cfg, const SyllableSet& known, const NgramModel& model,
    size_t max_results) {
  std::vector<std::u32string> out;
  if (word.empty() || max_results == 0) return out;

  const std::u32string bare = strip_diacritics(word);
  const std::vector<std::u32string> all =
      syllable_variants(bare, cfg, max_results * 2, &known);

  std::vector<std::u32string> real, structural;
  for (const std::u32string& v : all) {
    (known.contains(v) ? real : structural).push_back(v);
  }

  if (!model.empty() && real.size() > 1) {
    const size_t n = left.size();
    const int a = n >= 2 ? model.id(left[n - 2]) : model.bos();
    const int b = n >= 1 ? model.id(left[n - 1]) : model.bos();
    std::stable_sort(real.begin(), real.end(),
                     [&](const std::u32string& x, const std::u32string& y) {
                       return model.score(a, b, model.id(x)) >
                              model.score(a, b, model.id(y));
                     });
  }

  out = std::move(real);
  out.insert(out.end(), structural.begin(), structural.end());

  if (out.size() > max_results) out.resize(max_results);

  // Chuỗi không dấu ban đầu phải nằm trong vòng, người dùng cần quay về
  // được đúng thứ mình gõ. Cắt xong mới xét, vì bản thân nó cũng có thể là
  // một âm tiết thật ("toi") và bị chính giới hạn cắt mất — chỗ này không
  // phải chỉ thiếu-thì-thêm mà là một bất biến của kết quả.
  if (std::find(out.begin(), out.end(), bare) == out.end()) {
    if (out.size() == max_results) {
      out.back() = bare;  // đổi lấy phương án bét bảng
    } else {
      out.push_back(bare);
    }
  }
  return out;
}

std::vector<std::u32string> restore_sentence(
    const std::vector<std::u32string>& words, const Config& cfg,
    const SyllableSet& known, const NgramModel& model) {
  std::vector<std::u32string> result = words;
  if (model.empty() || words.empty()) return result;

  // Phương án cho từng vị trí. Chữ nào đã có dấu, hoặc không có phương án
  // nào có thật, thì giữ nguyên và trở thành một mốc cố định của chuỗi.
  std::vector<std::vector<std::u32string>> options(words.size());
  std::vector<std::vector<int>> ids(words.size());
  for (size_t i = 0; i < words.size(); ++i) {
    if (strip_diacritics(words[i]) == words[i]) {
      options[i] = RealVariants(words[i], cfg, known);
    }
    if (options[i].empty()) options[i].push_back(words[i]);
    for (const std::u32string& o : options[i]) ids[i].push_back(model.id(o));
  }

  // Viterbi trên trạng thái (âm tiết trước đó, âm tiết hiện tại). Ở đây
  // chữ bên phải đã có sẵn nên tối ưu chung cả chuỗi mới có nghĩa —
  // khác hẳn lúc đang gõ.
  struct Cell {
    float score = -1e30f;
    int prev = -1;  // chỉ số phương án ở vị trí trước
  };
  // best[j] = trạng thái kết ở phương án j của vị trí hiện tại, kèm ngữ
  // cảnh là phương án prev của vị trí trước.
  std::vector<std::vector<Cell>> table(words.size());

  for (size_t i = 0; i < words.size(); ++i) {
    table[i].assign(options[i].size(), Cell());
    for (size_t j = 0; j < options[i].size(); ++j) {
      if (i == 0) {
        table[i][j].score = model.score(model.bos(), model.bos(), ids[i][j]);
        continue;
      }
      for (size_t k = 0; k < options[i - 1].size(); ++k) {
        if (table[i - 1][k].score <= -1e29f) continue;
        const int prev2 =
            i >= 2 && table[i - 1][k].prev >= 0
                ? ids[i - 2][static_cast<size_t>(table[i - 1][k].prev)]
                : model.bos();
        const float s = table[i - 1][k].score +
                        model.score(prev2, ids[i - 1][k], ids[i][j]);
        if (s > table[i][j].score) {
          table[i][j].score = s;
          table[i][j].prev = static_cast<int>(k);
        }
      }
    }
  }

  // Lần ngược đường đi tốt nhất.
  size_t last = words.size() - 1;
  size_t bestIndex = 0;
  for (size_t j = 1; j < table[last].size(); ++j) {
    if (table[last][j].score > table[last][bestIndex].score) bestIndex = j;
  }
  for (size_t i = words.size(); i-- > 0;) {
    result[i] = options[i][bestIndex];
    if (i == 0) break;
    const int prev = table[i][bestIndex].prev;
    if (prev < 0) break;
    bestIndex = static_cast<size_t>(prev);
  }
  return result;
}

}  // namespace hodion
