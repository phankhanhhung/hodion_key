// Test cho bảng từ tiếng Anh.
//
// Test quan trọng nhất ở đây không phải "tra có ra không" mà là TÍNH CHẤT
// AN TOÀN: không từ nào trong bảng, ở bất kỳ cấu hình Telex nào, được phép
// ghép ra một âm tiết tiếng Việt hợp lệ. Nếu tính chất đó vỡ thì bộ gõ sẽ
// im lặng ghi đè lên chữ tiếng Việt đúng của người dùng — hỏng tệ nhất
// trong các kiểu hỏng. Bảng do script sinh ra, nên phải kiểm lại ở đây
// trên TỪNG từ chứ không tin script.
#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

#include "hodion/engine.h"
#include "hodion/english_words.h"
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

  std::printf("wordlist_test: %d từ, %d checks, %d failures\n",
              static_cast<int>(words.size()), g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
