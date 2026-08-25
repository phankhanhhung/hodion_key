// Test cho bảng từ tiếng Anh.
//
// Test quan trọng nhất ở đây không phải "tra có ra không" mà là TÍNH CHẤT
// AN TOÀN: không từ nào trong bảng, ở bất kỳ cấu hình Telex nào, được phép
// ghép ra một âm tiết tiếng Việt hợp lệ. Nếu tính chất đó vỡ thì bộ gõ sẽ
// im lặng ghi đè lên chữ tiếng Việt đúng của người dùng — hỏng tệ nhất
// trong các kiểu hỏng. Bảng do script sinh ra, nên phải kiểm lại ở đây
// trên TỪNG từ chứ không tin script.
#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <utility>
#include <string>
#include <vector>

#include "hodion/engine.h"
#include "hodion/english_words.h"
#include "hodion/ngram.h"
#include "hodion/predict.h"
#include "hodion/reconvert.h"
#include "hodion/utf.h"
#include "hodion/viet_words.h"

namespace {

#include "english_telex.inc"

int g_failures = 0;
int g_checks = 0;

#define EXPECT_EQ(actual, expected)                                     \
  do {                                                                  \
    ++g_checks;                                                         \
    const auto a_ = (actual);                                           \
    const auto e_ = (expected);                                         \
    if (a_ != e_) {                                                     \
      ++g_failures;                                                     \
      std::printf("FAIL %s:%d\n  cần:  %s\n  được: %s\n", __FILE__,     \
                  __LINE__, std::string(e_).c_str(),                    \
                  std::string(a_).c_str());                             \
    }                                                                   \
  } while (0)

#define EXPECT_TRUE(cond)                                               \
  do {                                                                  \
    ++g_checks;                                                         \
    if (!(cond)) {                                                      \
      ++g_failures;                                                     \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);       \
    }                                                                   \
  } while (0)

void Check(bool cond, const char* what) {
  ++g_checks;
  if (!cond) {
    ++g_failures;
    if (g_failures < 20) std::printf("FAIL: %s\n", what);
  }
}

std::vector<std::string> AllWords() {
  std::vector<std::string> out;
  for (size_t i = 0; i < sizeof(kBlocks) / sizeof(kBlocks[0]); ++i) {
    const size_t len = kMinLen + i;
    for (unsigned k = 0; k < kBlocks[i].count; ++k) {
      out.emplace_back(kBlocks[i].data + k * len, len);
    }
  }
  return out;
}

std::u32string Wide(const std::string& s) {
  return std::u32string(s.begin(), s.end());
}

std::u32string U(const char* utf8) { return hodion::utf::from_utf8(utf8); }

// --- Dựng một file mô hình nhỏ ngay trong test ---------------------------
//
// Tự dựng chứ không đọc file có sẵn: bộ nạp phân tích dữ liệu từ đĩa, tức
// là nó phải chịu được cả file hỏng lẫn file bị sửa. Muốn kiểm chuyện đó
// thì phải dựng được đủ mọi kiểu dữ liệu vào.
void PutU32(std::string* out, uint32_t v) {
  for (int i = 0; i < 4; ++i) out->push_back(static_cast<char>(v >> (8 * i)));
}
void PutU64(std::string* out, uint64_t v) {
  for (int i = 0; i < 8; ++i) out->push_back(static_cast<char>(v >> (8 * i)));
}
void PutF32(std::string* out, float v) {
  uint32_t bits;
  std::memcpy(&bits, &v, 4);
  PutU32(out, bits);
}

struct MiniModel {
  std::vector<std::string> vocab;              // phải xếp tăng dần
  std::vector<float> unigram;
  std::vector<std::pair<uint32_t, float>> bigram;
  std::vector<std::pair<uint64_t, float>> trigram;

  std::string Serialize(const char* magic = "HKNG", uint32_t version = 1) const {
    std::string out(magic, 4);
    PutU32(&out, version);
    PutU32(&out, static_cast<uint32_t>(vocab.size()));
    for (const std::string& w : vocab) {
      out.push_back(static_cast<char>(w.size()));
      out += w;
    }
    for (float f : unigram) PutF32(&out, f);
    PutU32(&out, static_cast<uint32_t>(bigram.size()));
    for (const auto& e : bigram) PutU32(&out, e.first);
    for (const auto& e : bigram) PutF32(&out, e.second);
    PutU32(&out, static_cast<uint32_t>(trigram.size()));
    for (const auto& e : trigram) PutU64(&out, e.first);
    for (const auto& e : trigram) PutF32(&out, e.second);
    return out;
  }
};

// Mô hình đồ chơi: "buổi tối" và "tôi qua" đều gặp, "buổi tôi" thì không.
MiniModel ToyModel() {
  MiniModel m;
  // Xếp theo thứ tự u32string (điểm mã), giống bộ nạp mong đợi.
  m.vocab = {"<s>", "buổi", "qua", "tôi", "tối"};
  std::sort(m.vocab.begin(), m.vocab.end(),
            [](const std::string& a, const std::string& b) {
              return hodion::utf::from_utf8(a) < hodion::utf::from_utf8(b);
            });
  m.unigram.assign(m.vocab.size(), -6.0f);
  return m;
}

int ToyId(const MiniModel& m, const char* word) {
  for (size_t i = 0; i < m.vocab.size(); ++i) {
    if (m.vocab[i] == word) return static_cast<int>(i);
  }
  return -1;
}
std::string S(const std::u32string& s) { return hodion::utf::to_utf8(s); }

// Bảng âm tiết thật KHÔNG nằm trong repo (từ điển nguồn là GPL-2), nên test
// dùng một bảng nhỏ tự viết. Nó đủ để kiểm hợp đồng của SyllableList và
// luật "chỉ nhận khi duy nhất" — hai thứ duy nhất thuộc về mã nguồn ở đây.
const char kSampleSyllables[] =
    "# vài âm tiết để test\n"
    "nguyệt\n"
    "việt\n"
    "viết\n"
    "đường\n"
    "dương\n"
    "toàn\n"
    "toán\n"
    "toan\n"
    "thuở\n"
    "khuya\n"
    "\n"
    "nguyệt\r\n";

// Các biến thể Telex làm đổi chữ ghép ra (giống SCAN_VARIANTS của script).
std::vector<hodion::Config> TelexVariants() {
  std::vector<hodion::Config> out;
  for (int no_w = 0; no_w < 2; ++no_w) {
    for (int no_free = 0; no_free < 2; ++no_free) {
      hodion::Config cfg;
      cfg.w_shorthand = no_w == 0;
      cfg.free_marking = no_free == 0;
      out.push_back(cfg);
    }
  }
  return out;
}

}  // namespace

int main() {
  const std::vector<std::string> words = AllWords();
  const std::vector<hodion::Config> variants = TelexVariants();

  Check(!words.empty(), "bảng không rỗng");
  Check(words.size() == hodion::english_words_count(),
        "english_words_count khớp số bản ghi");

  // --- Cấu trúc bảng: mỗi khối xếp tăng dần, đúng độ dài, chỉ a–z ---
  for (size_t i = 0; i < sizeof(kBlocks) / sizeof(kBlocks[0]); ++i) {
    const size_t len = kMinLen + i;
    std::string prev;
    for (unsigned k = 0; k < kBlocks[i].count; ++k) {
      const std::string w(kBlocks[i].data + k * len, len);
      bool lower = true;
      for (char c : w) lower = lower && c >= 'a' && c <= 'z';
      if (!lower) Check(false, ("từ có ký tự lạ: " + w).c_str());
      if (!prev.empty() && !(prev < w)) {
        Check(false, ("khối không xếp tăng dần tại: " + w).c_str());
      }
      prev = w;
    }
  }

  // --- Tính chất an toàn, kiểm trên từng từ ở mọi cấu hình Telex ---
  int unsafe = 0, unchanged = 0;
  for (const std::string& w : words) {
    for (const hodion::Config& cfg : variants) {
      hodion::Engine e(cfg);
      for (char c : w) e.process_char(static_cast<char32_t>(c));
      ++g_checks;
      if (e.composing_is_vietnamese()) {
        if (++unsafe < 10) {
          std::printf("FAIL: \"%s\" ghép ra âm tiết tiếng Việt hợp lệ\n",
                      w.c_str());
        }
        ++g_failures;
      }
      if (e.composition() == Wide(w)) ++unchanged;
    }
  }
  // Từ gõ ra đúng chính nó thì nằm trong bảng cũng vô nghĩa: nó chỉ làm
  // bảng phình ra. Cho phép vài từ như vậy ở cấu hình lệch chuẩn, nhưng
  // không được là số đông.
  Check(unchanged * 10 < static_cast<int>(words.size() * variants.size()),
        "phần lớn bản ghi thật sự cần khôi phục");

  // --- Tra cứu ---
  const hodion::ForeignWords& dict = hodion::english_words();
  bool all_found = true;
  for (const std::string& w : words) all_found &= dict.contains(Wide(w));
  Check(all_found, "tra được mọi từ trong bảng");

  Check(dict.contains(U"meeting"), "meeting có trong bảng");
  Check(dict.contains(U"server"), "server có trong bảng");
  Check(dict.contains(U"password"), "password có trong bảng");

  // Những từ ĐÃ BỊ LOẠI vì trùng âm tiết tiếng Việt thật — có mặt ở đây là
  // lỗi nghiêm trọng, không phải chuyện thẩm mỹ.
  const char* kMustBeAbsent[] = {
      "bans",   // bán
      "cans",   // cán
      "bust",   // bút
      "bits",   // bít
      "test",   // tét
      "best",   // bét
      "cars",   // cá
      "boots",  // bốt
  };
  for (const char* w : kMustBeAbsent) {
    Check(!dict.contains(Wide(w)),
          (std::string("từ va chạm phải bị loại: ") + w).c_str());
  }

  // Từ quá ngắn không bao giờ được xét (chúng vừa là từ Anh vừa là chuỗi
  // gõ hợp lệ của á/í/ỏ…).
  Check(!dict.contains(U"as"), "từ 2 ký tự bị bỏ qua");
  Check(!dict.contains(U"is"), "từ 2 ký tự bị bỏ qua");
  Check(!dict.contains(U"or"), "từ 2 ký tự bị bỏ qua");
  Check(!dict.contains(U""), "chuỗi rỗng bị bỏ qua");

  // Đầu vào sai hợp đồng (hoa, ký tự lạ) trả false chứ không đọc lố bảng.
  Check(!dict.contains(U"MEETING"), "chữ hoa không khớp");
  Check(!dict.contains(U"meet ing"), "có dấu cách thì không khớp");
  Check(!dict.contains(U"meet~ing"), "ký tự trên 'z' thì không khớp");
  Check(!dict.contains(U"meetìng"), "ký tự ngoài ASCII thì không khớp");
  Check(!dict.contains(U"meetingmeetingmeetingmeeting"), "quá dài, không khớp");
  Check(!dict.contains(U"zzzzzzzz"), "từ không có thật");

  // ======================================================================
  // Bảng âm tiết tiếng Việt + đoán dấu
  // ======================================================================
  {
    hodion::SyllableList viet;
    Check(viet.empty(), "chưa nạp thì bảng rỗng");
    Check(!viet.contains(U("việt")), "bảng rỗng thì không tra được gì");
    Check(!viet.load(""), "nội dung rỗng không nạp được");
    Check(!viet.load("# chỉ có chú thích\n\n"), "toàn chú thích cũng vậy");

    Check(viet.load(kSampleSyllables), "nạp được bảng mẫu");
    // Dòng chú thích, dòng trống và mục trùng đều bị bỏ; '\r' cuối dòng
    // (file sinh trên Windows) không được dính vào chữ.
    Check(viet.size() == 10, "bỏ chú thích, dòng trống và mục trùng");
    Check(viet.contains(U("việt")) && viet.contains(U("đường")) &&
              viet.contains(U("nguyệt")),
          "tra được chữ đã nạp");
    Check(!viet.contains(U("duông")) && !viet.contains(U("zzzz")),
          "chữ không nạp thì không có");
    const std::vector<std::u32string> syllables = {
        U("nguyệt"), U("việt"), U("viết"), U("đường"), U("dương"),
        U("toàn"),   U("toán"), U("toan"), U("thuở"),  U("khuya")};

    // --- Đoán dấu: chỉ nhận khi có ĐÚNG MỘT cách viết có thật ---
    hodion::Config cfg;
    EXPECT_EQ(S(hodion::restore_diacritics(U("nguyet"), cfg, viet)),
              std::string("nguyệt"));
    EXPECT_EQ(S(hodion::restore_diacritics(U("thuo"), cfg, viet)),
              std::string("thuở"));
    EXPECT_EQ(S(hodion::restore_diacritics(U("khuya"), cfg, viet)),
              std::string(""));  // đã đúng rồi, không có gì để đổi
    // Nhập nhằng thì im lặng để nguyên — đoán bừa là kiểu hỏng tệ nhất.
    EXPECT_TRUE(hodion::restore_diacritics(U("toan"), cfg, viet).empty());
    EXPECT_TRUE(hodion::restore_diacritics(U("duong"), cfg, viet).empty());
    EXPECT_TRUE(hodion::restore_diacritics(U("viet"), cfg, viet).empty());
    // Đã có dấu, không phải âm tiết, rỗng: không đụng tới.
    EXPECT_TRUE(hodion::restore_diacritics(U("việt"), cfg, viet).empty());
    EXPECT_TRUE(hodion::restore_diacritics(U("nguyệt"), cfg, viet).empty());
    EXPECT_TRUE(hodion::restore_diacritics(U("meeting"), cfg, viet).empty());
    EXPECT_TRUE(hodion::restore_diacritics(U("zzz"), cfg, viet).empty());
    EXPECT_TRUE(hodion::restore_diacritics(U(""), cfg, viet).empty());
    EXPECT_TRUE(hodion::restore_diacritics(U("viet nam"), cfg, viet).empty());

    // --- Tính chất an toàn, kiểm trên từng âm tiết trong bảng ---
    //
    // Đoán ra thứ gì thì thứ đó (a) phải là chữ CÓ THẬT, và (b) phải đúng
    // là chữ vừa gõ đã thêm dấu, không phải một chữ khác. Vi phạm (b) là
    // bộ gõ tự ý thay từ của người dùng — hỏng tệ nhất có thể.
    int restored = 0;
    for (const std::u32string& v : syllables) {
      const std::u32string bare = hodion::strip_diacritics(v);
      const std::u32string guess = hodion::restore_diacritics(bare, cfg, viet);
      ++g_checks;
      if (guess.empty()) continue;
      ++restored;
      if (!viet.contains(guess)) {
        ++g_failures;
        std::printf("FAIL: đoán \"%s\" ra \"%s\" — không có trong bảng\n",
                    S(bare).c_str(), S(guess).c_str());
      } else if (hodion::strip_diacritics(guess) != bare) {
        ++g_failures;
        std::printf("FAIL: đoán \"%s\" ra \"%s\" — khác chữ gốc\n",
                    S(bare).c_str(), S(guess).c_str());
      }
    }
    // Chặn dưới để thấy ngay nếu dữ liệu hụt đi; chặn trên để thấy ngay nếu
    // luật "chỉ nhận khi duy nhất" bị nới lỏng mất.
    Check(restored > 0 && restored < static_cast<int>(syllables.size()),
          "có đoán được, nhưng không phải cái nào cũng đoán");

    // Xếp thứ tự của reconversion: chữ CÓ THẬT phải lên trước chữ chỉ hợp
    // lệ về cấu trúc.
    {
      const auto v = hodion::syllable_variants(U("duong"), cfg, 64, &viet);
      EXPECT_TRUE(viet.contains(v.at(0)));
      size_t first_unknown = v.size();
      for (size_t i = 0; i < v.size(); ++i) {
        if (!viet.contains(v[i])) { first_unknown = i; break; }
      }
      for (size_t i = first_unknown; i < v.size(); ++i) {
        EXPECT_TRUE(!viet.contains(v[i]));
      }
    }
  }

  // ======================================================================
  // Mô hình n-gram: bộ nạp và bộ giải mã
  // ======================================================================
  {
    hodion::NgramModel model;
    Check(model.empty(), "chưa nạp thì mô hình rỗng");
    Check(model.score(0, 0, 0) < -10.0f, "mô hình rỗng cho điểm rất thấp");

    // --- File hỏng: phải từ chối, không được đọc lố ---
    MiniModel toy = ToyModel();
    Check(!model.load(""), "file rỗng");
    Check(!model.load("HK"), "file cụt");
    Check(!model.load(toy.Serialize("XXXX")), "sai magic");
    Check(!model.load(toy.Serialize("HKNG", 99)), "sai phiên bản");
    {
      const std::string full = toy.Serialize();
      for (size_t cut = 1; cut < full.size(); cut += 7) {
        if (model.load(full.substr(0, cut))) {
          Check(false, "nhận một file bị cắt cụt");
          break;
        }
      }
      Check(true, "mọi độ dài cắt cụt đều bị từ chối");
    }
    {
      // Số lượng bịa quá lớn: phải phát hiện trước khi cấp phát.
      std::string bad(4, '\0');
      std::memcpy(&bad[0], "HKNG", 4);
      PutU32(&bad, 1);
      PutU32(&bad, 0xFFFFFFFFu);
      Check(!model.load(bad), "số mục từ vựng bịa quá lớn");
    }
    {
      MiniModel unsorted = toy;
      std::reverse(unsorted.vocab.begin(), unsorted.vocab.end());
      Check(!model.load(unsorted.Serialize()), "từ vựng không xếp tăng dần");
    }

    // --- Nạp được và tra đúng ---
    const int s_bos = ToyId(toy, "<s>");
    const int s_buoi = ToyId(toy, "buổi");
    const int s_toi = ToyId(toy, "tôi");
    const int s_toi2 = ToyId(toy, "tối");
    const int s_qua = ToyId(toy, "qua");
    // "buổi tối" hay gặp; "buổi tôi" không có trong mô hình.
    toy.bigram.push_back({(uint32_t(s_buoi) << 16) | uint32_t(s_toi2), -0.2f});
    toy.bigram.push_back({(uint32_t(s_bos) << 16) | uint32_t(s_toi), -0.5f});
    toy.bigram.push_back({(uint32_t(s_toi) << 16) | uint32_t(s_qua), -0.3f});
    std::sort(toy.bigram.begin(), toy.bigram.end());
    toy.trigram.push_back({(uint64_t(s_bos) << 32) | (uint64_t(s_buoi) << 16) |
                               uint64_t(s_toi2), -0.1f});
    std::sort(toy.trigram.begin(), toy.trigram.end());

    Check(model.load(toy.Serialize()), "nạp được mô hình đồ chơi");
    Check(model.vocab_size() == toy.vocab.size(), "đúng số từ vựng");
    Check(model.bigram_count() == toy.bigram.size(), "đúng số bigram");
    Check(model.trigram_count() == toy.trigram.size(), "đúng số trigram");
    Check(model.id(U("buổi")) == s_buoi, "tra được id");
    Check(model.id(U("khong-co")) == hodion::NgramModel::kNoWord,
          "chữ ngoài từ vựng trả kNoWord");
    Check(model.bos() == s_bos, "tìm được mốc đầu câu");

    // Trigram có sẵn thì dùng thẳng; thiếu thì lùi bậc và bị phạt.
    Check(model.score(s_bos, s_buoi, s_toi2) > -0.15f, "trigram dùng thẳng");
    const float bi = model.score(s_qua, s_buoi, s_toi2);   // chỉ có bigram
    Check(bi < -0.2f && bi > -2.0f, "lùi về bigram thì bị phạt");
    const float uni = model.score(s_qua, s_qua, s_toi2);   // chỉ có unigram
    Check(uni < bi, "lùi hai bậc thì phạt nặng hơn");

    // --- Đoán theo ngữ cảnh trái ---
    hodion::SyllableList toyKnown;
    Check(toyKnown.load("buổi\ntối\ntôi\nqua\n"), "bảng âm tiết đồ chơi");
    hodion::Config cfg;
    // "toi" một mình: mô hình không có bằng chứng nghiêng hẳn → im lặng.
    // Sau "buổi" thì trigram nói rõ là "tối".
    const std::vector<std::u32string> after_buoi = {U("buổi")};
    EXPECT_EQ(S(hodion::restore_in_context(after_buoi, U("toi"), cfg, toyKnown,
                                           model)),
              std::string("tối"));
    // Chữ đã có dấu thì không đụng.
    EXPECT_TRUE(hodion::restore_in_context(after_buoi, U("tối"), cfg, toyKnown,
                                           model)
                    .empty());
    // Không có mô hình thì không đoán gì.
    {
      hodion::NgramModel none;
      EXPECT_TRUE(hodion::restore_in_context(after_buoi, U("toi"), cfg,
                                             toyKnown, none)
                      .empty());
    }

    // --- Xếp hạng phương án cho phím xoay vòng ---
    {
      const std::vector<std::u32string> after_buoi2 = {U("buổi")};
      const auto ranked = hodion::rank_candidates(after_buoi2, U("toi"), cfg,
                                                  toyKnown, model);
      Check(!ranked.empty(), "có phương án để xoay");
      // Ngữ cảnh "buổi" thì "tối" phải đứng đầu — bấm một cái là xong.
      EXPECT_EQ(S(ranked.at(0)), std::string("tối"));
      // Chuỗi không dấu ban đầu LUÔN có mặt: phải quay về được thứ mình gõ.
      Check(std::find(ranked.begin(), ranked.end(), U("toi")) != ranked.end(),
            "chuỗi không dấu nằm trong vòng xoay");
      // Không im lặng như restore_in_context: người dùng đã chủ động bấm.
      Check(ranked.size() >= 2, "luôn có ít nhất hai thứ để xoay qua lại");

      // Giới hạn và ca biên.
      Check(hodion::rank_candidates(after_buoi2, U("toi"), cfg, toyKnown,
                                    model, 2)
                .size() == 2,
            "tôn trọng giới hạn số phương án");
      Check(hodion::rank_candidates(after_buoi2, U("toi"), cfg, toyKnown,
                                    model, 0)
                .empty(),
            "giới hạn 0 thì rỗng");
      // Giới hạn chật KHÔNG được đẩy chuỗi gốc ra khỏi vòng: cắt danh sách
      // sau khi thêm nó vào sẽ vứt đi đúng cái vừa thêm.
      for (size_t limit = 1; limit <= 6; ++limit) {
        const auto few = hodion::rank_candidates(after_buoi2, U("toi"), cfg,
                                                 toyKnown, model, limit);
        Check(few.size() <= limit, "không vượt giới hạn");
        Check(std::find(few.begin(), few.end(), U("toi")) != few.end(),
              "chuỗi gốc còn nguyên dù giới hạn chật");
      }
      Check(hodion::rank_candidates({}, U(""), cfg, toyKnown, model).empty(),
            "chuỗi rỗng thì không có phương án");

      // Không có mô hình vẫn xoay được, chỉ là không xếp theo ngữ cảnh.
      hodion::NgramModel none;
      const auto plain =
          hodion::rank_candidates({}, U("toi"), cfg, toyKnown, none);
      Check(plain.size() >= 2, "không có mô hình vẫn có phương án");
      Check(std::find(plain.begin(), plain.end(), U("toi")) != plain.end(),
            "vẫn quay về được chuỗi không dấu");

      // Chữ đã có dấu cũng xoay được — xoay quanh chính họ của nó.
      const auto from_accented =
          hodion::rank_candidates({}, U("tối"), cfg, toyKnown, model);
      Check(std::find(from_accented.begin(), from_accented.end(), U("tôi")) !=
                from_accented.end(),
            "xoay từ chữ đã có dấu vẫn thấy các anh em của nó");
    }

    // --- Viterbi cả câu ---
    {
      const std::vector<std::u32string> bare = {U("buổi"), U("toi")};
      const auto out =
          hodion::restore_sentence(bare, cfg, toyKnown, model);
      Check(out.size() == bare.size(), "giữ nguyên số phần tử");
      EXPECT_EQ(S(out.at(1)), std::string("tối"));
      // Chuỗi rỗng và chuỗi không đoán được thì trả về y nguyên.
      EXPECT_TRUE(hodion::restore_sentence({}, cfg, toyKnown, model).empty());
      const std::vector<std::u32string> odd = {U("zzz"), U("qqq")};
      const auto same = hodion::restore_sentence(odd, cfg, toyKnown, model);
      Check(same == odd, "chữ không đoán được thì giữ nguyên");
    }
  }

  std::printf("wordlist_test: %d từ, %d checks, %d failures\n",
              static_cast<int>(words.size()), g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
