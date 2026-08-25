#include "hodion/reconvert.h"

#include <algorithm>

#include "vnlexi.h"

namespace hodion {

using detail::Mark;
using detail::ToneId;

namespace {

// Âm tiết dài nhất có thể: phụ âm đầu 3 (ngh) + vần 3 + âm cuối 2.
constexpr size_t kMaxSyllable = 8;

// Phím dấu Telex gõ ở CUỐI từ (gõ dấu tự do). Mỗi phím dùng nhiều nhất một
// lần: một nguyên âm chỉ mang được một dấu phụ.
//   a/e/o → mũ (â ê ô),  w → móc (ơ ư) hoặc trăng (ă)
//
// KHÔNG có phím `d` ở đây, cố ý — xem chú thích về đ bên dưới.
constexpr char32_t kMarkKeys[] = {U'a', U'e', U'o', U'w'};
constexpr int kMarkCount = 4;
constexpr char32_t kToneKeys[] = {0, U's', U'f', U'r', U'x', U'j'};

// Cấu hình dùng để SINH biến thể — cố ý không phải cấu hình gõ của người
// dùng. Nó chỉ trả lời "âm tiết nào tồn tại", nên phải bật gõ dấu tự do và
// tắt các tùy chọn có thể chen ký tự lạ vào (w→ư, [ ]→ơ ư).
Config generator_config(const Config& user) {
  Config cfg;
  cfg.tone_style = user.tone_style;  // "hòa" hay "hoà" là lựa chọn của họ
  cfg.method = InputMethod::Telex;
  cfg.free_marking = true;
  cfg.spell_check = true;
  cfg.restore_non_vn = false;
  cfg.w_shorthand = false;
  cfg.telex_brackets = false;
  cfg.english_detect = false;
  return cfg;
}

// đ được suy ra chứ không gõ thử, và có lý do.
//
// Engine cố ý KHÔNG kiểm tra chính tả cho từ có đ — đó là luật của UniKey
// để gõ viết tắt ("đt", "đc") mà không bị chặn. Hệ quả: gõ "ddoait" ra
// "đoait" và engine không kêu gì, dù đó không phải âm tiết tiếng Việt.
// Nếu lấy đường gõ làm nguồn sinh thì danh sách chọn sẽ đầy những chữ như
// vậy.
//
// Nên ta sinh với `d` (được kiểm chính tả đầy đủ) rồi đổi `d` đầu từ thành
// `đ`. Đổi được vì luật ghép phụ âm đầu chỉ phân biệt k, gi và qu — với
// mọi vần, `đ` hợp lệ đúng ở chỗ `d` hợp lệ. Và đ trong tiếng Việt chỉ
// đứng đầu âm tiết nên không cần xét vị trí nào khác.
bool starts_with_d(const std::u32string& s) {
  return !s.empty() && (s[0] == U'd' || s[0] == U'D');
}

std::u32string with_stroke_d(const std::u32string& s) {
  std::u32string out = s;
  out[0] = s[0] == U'D' ? U'Đ' : U'đ';
  return out;
}

// Hạ chữ hoa về chữ thường, kể cả chữ tiếng Việt dựng sẵn.
std::u32string lowered(const std::u32string& s) {
  std::u32string out;
  out.reserve(s.size());
  for (char32_t c : s) {
    char32_t base = 0;
    Mark mark = Mark::None;
    ToneId tone = 0;
    bool upper = false;
    if (detail::decompose_char(c, &base, &mark, &tone, &upper)) {
      out.push_back(detail::composed_char(base, mark, tone, false));
    } else {
      out.push_back(c);
    }
  }
  return out;
}

}  // namespace

std::u32string strip_diacritics(const std::u32string& s) {
  std::u32string out;
  out.reserve(s.size());
  for (char32_t c : s) {
    char32_t base = 0;
    Mark mark = Mark::None;
    ToneId tone = 0;
    bool upper = false;
    if (detail::decompose_char(c, &base, &mark, &tone, &upper)) {
      out.push_back(upper ? base - 32 : base);
    } else {
      out.push_back(c);
    }
  }
  return out;
}

// Cách sinh: KHÔNG duyệt bảng vần, mà gõ thử.
//
// Duyệt bảng cho ra những thứ bảng cho phép nhưng bộ luật gõ không bao giờ
// sinh ra ("gía" bên cạnh "giá", "quýen" bên cạnh "quyến", "dưong" bên cạnh
// "dương"). Một danh sách chọn chứa những chữ người dùng không gõ ra được
// là danh sách sai. Nên ở đây ta lấy chuỗi chữ cái gốc rồi gõ lại nó qua
// CHÍNH engine với mọi tổ hợp phím dấu và phím thanh, và giữ lại những gì
// engine thật sự cho ra. Hợp đồng vì thế đúng theo định nghĩa, không phải
// theo lời hứa.
std::vector<std::u32string> syllable_variants(const std::u32string& word,
                                              const Config& cfg,
                                              size_t max_results,
                                              const SyllableSet* known) {
  std::vector<std::u32string> out;
  if (word.empty() || word.size() > kMaxSyllable || max_results == 0) {
    return out;
  }

  // Âm tiết tiếng Việt chỉ gồm chữ cái của bảng chữ tiếng Việt.
  //
  // is_vn_letter loại f, j, w đúng theo cách UniKey phân loại khi GÕ. Ở đây
  // loại thêm `z`: nó chỉ là phím xoá thanh của Telex chứ không phải chữ
  // cái tiếng Việt, và vì engine không coi "zan" là từ hỏng nên để lọt là
  // danh sách chọn sẽ có cả "zán", "zàn". Không đụng vào is_vn_letter vì
  // hàm đó nằm trên đường gõ — đổi nó là đổi hành vi gõ VNI.
  const std::u32string bare = strip_diacritics(word);
  for (char32_t c : bare) {
    const char32_t lower = (c >= U'A' && c <= U'Z') ? c + 32 : c;
    if (!detail::is_vn_letter(lower) || lower == U'z') return out;
  }

  // Số dấu phụ + thanh khác so với chữ người dùng đang có. Không có mô
  // hình tần suất nên không thể xếp theo "hay gặp"; xếp theo GẦN VỚI CHỮ
  // ĐANG CÓ thì vừa đoán được vừa đúng với việc người ta hay làm nhất —
  // sửa lại đúng một cái dấu. (Xếp theo tần suất là việc của mô hình n-gram
  // trong lộ trình, không phải của tầng này.)
  // Mọi phương án đều dài đúng bằng `word` (chúng là cùng chuỗi chữ cái gốc,
  // chỉ khác dấu), nên một cận là đủ.
  auto distance = [&word](const std::u32string& other) {
    int diff = 0;
    for (size_t i = 0; i < other.size(); ++i) {
      char32_t b1 = 0, b2 = 0;
      Mark m1 = Mark::None, m2 = Mark::None;
      ToneId t1 = 0, t2 = 0;
      bool u1 = false, u2 = false;
      detail::decompose_char(word[i], &b1, &m1, &t1, &u1);
      detail::decompose_char(other[i], &b2, &m2, &t2, &u2);
      if (m1 != m2) ++diff;
      if (t1 != t2) ++diff;
    }
    return diff;
  };

  Engine engine(generator_config(cfg));
  for (int mask = 0; mask < (1 << kMarkCount); ++mask) {
    for (char32_t tone_key : kToneKeys) {
      engine.reset();
      for (char32_t c : bare) engine.process_char(c);
      for (int i = 0; i < kMarkCount; ++i) {
        if (mask & (1 << i)) engine.process_char(kMarkKeys[i]);
      }
      if (tone_key != 0) engine.process_char(tone_key);

      // Phím dấu không áp được sẽ bị nối vào như một chữ cái thường, và
      // chữ hỏng chính tả thì engine ngừng biến đổi — cả hai đều làm chuỗi
      // dài ra. Đó cũng chính là dấu hiệu để loại.
      const std::u32string text = engine.composition();
      if (text.size() != bare.size()) continue;
      if (!engine.composing_is_vietnamese()) continue;
      if (strip_diacritics(text) != bare) continue;

      if (std::find(out.begin(), out.end(), text) == out.end()) {
        out.push_back(text);
      }
    }
  }

  if (starts_with_d(bare)) {
    const size_t plain = out.size();
    for (size_t i = 0; i < plain; ++i) out.push_back(with_stroke_d(out[i]));
  }

  // Ba bậc ưu tiên. Chữ người dùng đang có tự khắc đứng đầu: nó là phương
  // án DUY NHẤT có khoảng cách 0 (khác dấu là khác chuỗi, mà khác chuỗi thì
  // khoảng cách khác 0).
  auto rank = [&distance, known](const std::u32string& s) {
    const int d = distance(s);
    if (d == 0) return std::make_pair(0, 0);            // chính nó
    if (known && known->contains(lowered(s))) return std::make_pair(1, d);
    return std::make_pair(2, d);                        // chỉ hợp lệ cấu trúc
  };
  std::stable_sort(out.begin(), out.end(),
                   [&rank](const std::u32string& a, const std::u32string& b) {
                     return rank(a) < rank(b);
                   });

  if (out.size() > max_results) out.resize(max_results);
  return out;
}

}  // namespace hodion
