// Sinh lại dấu cho chữ đã gõ xong (reconversion).
//
// Hai tính chất quan trọng hơn mọi ca cụ thể:
//   ĐỦ  — mọi âm tiết engine gõ ra được đều phải có mặt trong danh sách
//         biến thể của chính nó. Thiếu là người dùng không chọn được thứ
//         họ gõ ra được bằng tay.
//   ĐÚNG — mọi biến thể trả về phải cùng chuỗi chữ cái gốc, và bản thân
//         nó phải sinh lại đúng cùng một họ.
#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "hodion/reconvert.h"
#include "test_util.h"

using hodion::Config;
using hodion::Engine;

namespace {

std::string u8(const std::u32string& s) { return hodion::utf::to_utf8(s); }
std::u32string u32(const std::string& s) { return hodion::utf::from_utf8(s); }

std::vector<std::string> variants(const std::string& word,
                                  Config cfg = Config{}) {
  std::vector<std::string> out;
  for (const std::u32string& v : hodion::syllable_variants(u32(word), cfg)) {
    out.push_back(u8(v));
  }
  return out;
}

std::string joined(const std::string& word, Config cfg = Config{}) {
  std::string s;
  for (const std::string& v : variants(word, cfg)) {
    if (!s.empty()) s += " ";
    s += v;
  }
  return s;
}

bool has(const std::vector<std::string>& v, const std::string& what) {
  return std::find(v.begin(), v.end(), what) != v.end();
}

// Bộ sinh cố định — chạy lại luôn ra cùng kết quả.
class Rng {
 public:
  explicit Rng(uint32_t seed) : state_(seed) {}
  uint32_t below(uint32_t n) {
    state_ = state_ * 1664525u + 1013904223u;
    return n ? (state_ >> 8) % n : 0;
  }

 private:
  uint32_t state_;
};

}  // namespace

void run_reconvert_tests() {
  // --- Bỏ dấu ---
  EXPECT_EQ(u8(hodion::strip_diacritics(u32("Việt"))), std::string("Viet"));
  EXPECT_EQ(u8(hodion::strip_diacritics(u32("đường"))), std::string("duong"));
  EXPECT_EQ(u8(hodion::strip_diacritics(u32("ĐƯỜNG"))), std::string("DUONG"));
  EXPECT_EQ(u8(hodion::strip_diacritics(u32("Tiếng Việt!"))),
            std::string("Tieng Viet!"));
  EXPECT_EQ(u8(hodion::strip_diacritics(u32(""))), std::string(""));
  // Ký tự không phải chữ cái giữ nguyên.
  EXPECT_EQ(u8(hodion::strip_diacritics(u32("a1-b"))), std::string("a1-b"));

  // --- Danh sách biến thể ---
  EXPECT_EQ(joined("viet"), std::string("viêt viết việt"));
  EXPECT_EQ(joined("nguyen"),
            std::string("nguyên nguyến nguyền nguyển nguyễn nguyện"));
  EXPECT_EQ(joined("quyen"),
            std::string("quyên quyến quyền quyển quyễn quyện"));

  // Chữ đang có đứng đầu, rồi tới những chữ khác nó ít nhất.
  {
    const std::vector<std::string> v = variants("đường");
    EXPECT_EQ(v.at(0), std::string("đường"));
    // Khác đúng một dấu thanh hoặc một dấu phụ thì phải nằm ngay đầu bảng.
    const std::vector<std::string> head(v.begin(), v.begin() + 7);
    EXPECT_TRUE(has(head, "đưởng"));
    EXPECT_TRUE(has(head, "đương"));
    EXPECT_TRUE(has(head, "dường"));
    EXPECT_TRUE(has(v, "đướng"));
  }

  // Chữ không dấu mà tự nó đã là âm tiết hợp lệ cũng đứng đầu.
  EXPECT_EQ(variants("toi").at(0), std::string("toi"));
  EXPECT_EQ(variants("hoa").at(0), std::string("hoa"));
  // "viet" thì không: engine không coi "iet" là vần hợp lệ.
  EXPECT_TRUE(!has(variants("viet"), "viet"));

  // --- Hoa/thường giữ theo từng vị trí ---
  EXPECT_EQ(joined("Viet"), std::string("Viêt Viết Việt"));
  EXPECT_EQ(joined("VIET"), std::string("VIÊT VIẾT VIỆT"));
  EXPECT_EQ(variants("vIET").at(0), std::string("vIÊT"));
  // Cả hai đầu bảng chữ cái ở dạng hoa lẫn thường đều phải được nhận là
  // chữ của từ ("A" và "Z" là biên của khoảng, dễ lọt).
  EXPECT_TRUE(has(variants("An"), "Ăn"));
  EXPECT_TRUE(has(variants("AN"), "ĂN"));
  EXPECT_TRUE(has(variants("an"), "ăn"));
  // f, j, w, z không thuộc bảng chữ tiếng Việt nên không có biến thể nào.
  // ("z" đặc biệt dễ lọt: nó là phím xoá thanh của Telex, engine không coi
  // "zan" là từ hỏng nên nếu không chặn sẽ đề xuất cả "zán", "zàn".)
  EXPECT_TRUE(variants("Za").empty());
  EXPECT_TRUE(variants("zan").empty());
  EXPECT_TRUE(variants("fan").empty());
  EXPECT_TRUE(variants("jan").empty());
  EXPECT_TRUE(variants("wan").empty());
  EXPECT_TRUE(variants("ZAN").empty());

  // --- Kiểu bỏ dấu đổi chỗ đặt thanh ---
  EXPECT_TRUE(has(variants("hoa"), "hòa"));
  {
    Config modern;
    modern.tone_style = hodion::ToneStyle::Modern;
    EXPECT_TRUE(has(variants("hoa", modern), "hoà"));
    EXPECT_TRUE(!has(variants("hoa", modern), "hòa"));
  }

  // --- Đầu vào không phải một âm tiết ---
  EXPECT_TRUE(variants("").empty());
  EXPECT_TRUE(variants("viet nam").empty());   // có dấu cách
  EXPECT_TRUE(variants("viet1").empty());      // có chữ số
  EXPECT_TRUE(variants("nghiengggg").empty()); // dài hơn mọi âm tiết
  EXPECT_TRUE(variants("xyz").empty());        // không ghép được gì
  EXPECT_TRUE(variants("zzz").empty());

  // --- Giới hạn số kết quả ---
  {
    Config cfg;
    EXPECT_TRUE(hodion::syllable_variants(u32("toi"), cfg, 0).empty());
    EXPECT_TRUE(hodion::syllable_variants(u32("toi"), cfg, 3).size() == 3);
    // Cắt bớt nhưng vẫn giữ chữ đang có ở đầu.
    const auto few = hodion::syllable_variants(u32("tôi"), cfg, 2);
    EXPECT_EQ(u8(few.at(0)), std::string("tôi"));
  }

  // ======================================================================
  // Tính chất ĐỦ: gõ ngẫu nhiên; mọi âm tiết tiếng Việt engine cho ra được
  // đều phải nằm trong danh sách biến thể của chính nó.
  // ======================================================================
  {
    // Không có "dd": engine cố ý bỏ kiểm chính tả cho từ có đ nên nó nhận
    // cả "đoait" — đường gõ đó không dùng làm chuẩn đối chiếu được. Luật đ
    // có test riêng ngay bên dưới.
    static const char* kOnsets[] = {"",   "b",  "c",  "ch", "d",  "g",
                                    "gh", "gi", "h",  "k",  "kh", "l",  "m",
                                    "n",  "ng", "ngh", "nh", "ph", "qu", "r",
                                    "s",  "t",  "th", "tr", "v",  "x"};
    static const char* kVowels[] = {
        "a",  "aa", "aw",  "e",   "ee",  "i",    "o",   "oo",  "ow",
        "u",  "uw", "y",   "ai",  "ao",  "au",   "ay",  "ia",  "ie",
        "iee", "iu", "oa", "oe",  "oi",  "ua",   "ue",  "uo",  "uow",
        "uoi", "uy", "uye", "uyee", "ieu", "ieeu", "oai", "oay", "uou"};
    static const char* kCodas[] = {"", "c", "ch", "m", "n", "ng", "nh", "p",
                                   "t"};
    static const char* kTones[] = {"", "s", "f", "r", "x", "j"};

    Rng rng(0x5EEDu);
    int checked = 0;
    for (int style = 0; style < 2; ++style) {
      Config cfg;
      cfg.tone_style = style == 0 ? hodion::ToneStyle::Traditional
                                  : hodion::ToneStyle::Modern;
      for (int round = 0; round < 6000; ++round) {
        std::string keys = kOnsets[rng.below(26)];
        keys += kVowels[rng.below(36)];
        keys += kCodas[rng.below(9)];
        keys += kTones[rng.below(6)];

        Engine e(cfg);
        for (char c : keys) e.process_char(static_cast<char32_t>(c));
        if (!e.composing_is_vietnamese()) continue;
        const std::u32string text = e.composition();
        if (text.empty() || text.size() > 8) continue;

        ++g_checks;
        ++checked;
        const auto list =
            hodion::syllable_variants(hodion::strip_diacritics(text), cfg);
        if (std::find(list.begin(), list.end(), text) == list.end()) {
          ++g_failures;
          std::printf("FAIL: gõ \"%s\" ra \"%s\" nhưng không có trong biến thể\n",
                      keys.c_str(), u8(text).c_str());
        }
      }
    }
    EXPECT_TRUE(checked > 3000);  // bộ sinh không được rơi hết vào chữ hỏng
  }

  // --- Luật đ: suy ra từ d chứ không lấy từ đường gõ ---
  {
    const std::vector<std::string> v = variants("duong");
    // Mọi biến thể của d đều có bản đ tương ứng.
    EXPECT_TRUE(has(v, "dương") && has(v, "đương"));
    EXPECT_TRUE(has(v, "dường") && has(v, "đường"));
    EXPECT_TRUE(has(v, "duông") && has(v, "đuông"));
    EXPECT_EQ(variants("Duong").at(0), std::string("Duông"));
    EXPECT_TRUE(has(variants("Duong"), "Đường"));

    // Nhưng KHÔNG đề xuất chữ mà engine chỉ nhận vì bỏ kiểm chính tả cho
    // đ: gõ tay "ddoait" ra "đoait", ở đây thì không có.
    EXPECT_TRUE(variants("doait").empty());
    EXPECT_TRUE(variants("đoait").empty());
    EXPECT_TRUE(variants("decr").empty());
    // Chữ có đ ở giữa từ không phải tiếng Việt → không có biến thể nào.
    EXPECT_TRUE(variants("ađ").empty());
  }

  // ======================================================================
  // Tính chất ĐÚNG: mọi biến thể cùng chuỗi chữ cái gốc, và sinh lại từ
  // bất kỳ biến thể nào cũng cho đúng cùng một họ.
  // ======================================================================
  {
    static const char* kWords[] = {"toi",  "duong", "hoa",  "viet", "nguyen",
                                   "bat",  "gia",   "thuo", "an",   "uyen",
                                   "khuya", "nghieng", "qua", "Duong", "TOI"};
    Config cfg;
    for (const char* w : kWords) {
      const std::u32string base = hodion::strip_diacritics(u32(w));
      const auto list = hodion::syllable_variants(u32(w), cfg);
      std::set<std::u32string> family(list.begin(), list.end());

      for (const std::u32string& v : list) {
        ++g_checks;
        if (hodion::strip_diacritics(v) != base || v.size() != base.size()) {
          ++g_failures;
          std::printf("FAIL: biến thể \"%s\" lệch chữ gốc của \"%s\"\n",
                      u8(v).c_str(), w);
          continue;
        }
        const auto again = hodion::syllable_variants(v, cfg);
        if (std::set<std::u32string>(again.begin(), again.end()) != family) {
          ++g_failures;
          std::printf("FAIL: sinh lại từ \"%s\" ra họ khác\n", u8(v).c_str());
        }
      }
    }
  }
}
