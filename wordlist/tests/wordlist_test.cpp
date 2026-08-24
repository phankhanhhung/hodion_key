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

namespace {

#include "english_telex.inc"

int g_failures = 0;
int g_checks = 0;

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

  std::printf("wordlist_test: %d từ, %d checks, %d failures\n",
              static_cast<int>(words.size()), g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
