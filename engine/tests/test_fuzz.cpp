// Fuzz có hạt giống cố định: gõ ngẫu nhiên, xen Backspace giữa chừng, đổi
// cấu hình ngẫu nhiên. Mỗi thao tác kiểm tra lại bất biến — cả bất biến
// nội bộ (Engine::self_check) lẫn bất biến quan sát được từ ngoài.
//
// Khi một bất biến vỡ, test in ra đúng chuỗi thao tác đã dẫn tới lỗi để
// tái hiện lại được ngay.
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "test_util.h"
#include "vnlexi.h"  // white-box: bảng ký tự để kiểm tra chuỗi ra

using hodion::Config;
using hodion::Engine;

namespace {

// Bộ sinh số giả ngẫu nhiên cố định — chạy lại luôn ra cùng kết quả.
class Rng {
 public:
  explicit Rng(uint32_t seed) : state_(seed) {}
  uint32_t next() {
    state_ = state_ * 1664525u + 1013904223u;
    return state_ >> 8;
  }
  uint32_t below(uint32_t n) { return n ? next() % n : 0; }
  bool chance(uint32_t percent) { return below(100) < percent; }

 private:
  uint32_t state_;
};

// Mọi codepoint engine được phép sinh ra: ASCII in được + chữ tiếng Việt
// dựng sẵn. Bất cứ thứ gì khác là dấu hiệu bảng ký tự bị tra sai.
bool IsAllowedOutputChar(char32_t c) {
  if (c >= 0x20 && c < 0x7F) return true;

  using hodion::detail::Mark;
  static const char32_t kBases[] = {U'a', U'e', U'i', U'o', U'u', U'y', U'd'};
  static const Mark kMarks[] = {Mark::None, Mark::Breve, Mark::Circumflex,
                                Mark::Horn, Mark::Stroke};
  for (char32_t base : kBases) {
    for (Mark mark : kMarks) {
      for (uint8_t tone = 0; tone <= 5; ++tone) {
        for (int upper = 0; upper < 2; ++upper) {
          if (hodion::detail::composed_char(base, mark, tone, upper != 0) == c) {
            return true;
          }
        }
      }
    }
  }
  return false;
}

// Chuyển một ký tự tiếng Việt sang chữ hoa bằng chính bảng ký tự của engine.
char32_t ToUpperVn(char32_t c) {
  if (c >= U'a' && c <= U'z') return c - 32;

  using hodion::detail::Mark;
  static const char32_t kBases[] = {U'a', U'e', U'i', U'o', U'u', U'y', U'd'};
  static const Mark kMarks[] = {Mark::None, Mark::Breve, Mark::Circumflex,
                                Mark::Horn, Mark::Stroke};
  for (char32_t base : kBases) {
    for (Mark mark : kMarks) {
      for (uint8_t tone = 0; tone <= 5; ++tone) {
        if (hodion::detail::composed_char(base, mark, tone, false) == c) {
          return hodion::detail::composed_char(base, mark, tone, true);
        }
      }
    }
  }
  return c;
}

// Số vòng fuzz: mặc định vừa đủ nhanh cho CI, tăng bằng HODION_FUZZ_ROUNDS
// khi muốn chạy soak lâu.
int FuzzRounds(int fallback) {
  const char* env = std::getenv("HODION_FUZZ_ROUNDS");
  if (!env) return fallback;
  const int n = std::atoi(env);
  return n > 0 ? n : fallback;
}

struct Op {
  enum Kind { Key, Backspace } kind;
  char32_t ch;
};

std::string DescribeOps(const std::vector<Op>& ops, const Config& cfg) {
  std::string s = cfg.method == hodion::InputMethod::Telex ? "[telex" : "[vni";
  s += cfg.tone_style == hodion::ToneStyle::Modern ? ",modern" : ",cu";
  if (!cfg.free_marking) s += ",!free";
  if (!cfg.spell_check) s += ",!spell";
  if (cfg.restore_non_vn) s += ",restore";
  if (!cfg.w_shorthand) s += ",!w";
  if (!cfg.telex_brackets) s += ",!brackets";
  s += "] ";
  for (const Op& op : ops) {
    if (op.kind == Op::Backspace) {
      s += "<BS>";
    } else {
      s += static_cast<char>(op.ch);
    }
  }
  return s;
}

// Báo lỗi một lần cho mỗi loại bất biến để log không bị ngập.
struct Reporter {
  int reported = 0;
  void fail(const char* what, const std::vector<Op>& ops, const Config& cfg) {
    ++g_failures;
    if (reported++ < 8) {
      std::printf("FUZZ FAIL (%s)\n  thao tác: %s\n", what,
                  DescribeOps(ops, cfg).c_str());
    }
  }
};

Config RandomConfig(Rng& rng) {
  Config cfg;
  cfg.method = rng.chance(50) ? hodion::InputMethod::Telex
                              : hodion::InputMethod::Vni;
  cfg.tone_style = rng.chance(50) ? hodion::ToneStyle::Traditional
                                  : hodion::ToneStyle::Modern;
  cfg.free_marking = rng.chance(80);
  cfg.spell_check = rng.chance(80);
  cfg.restore_non_vn = rng.chance(40);
  cfg.w_shorthand = rng.chance(85);
  cfg.telex_brackets = rng.chance(85);
  return cfg;
}

// Sinh chuỗi phím có hình dạng một âm tiết thật (phụ âm đầu + vần + âm cuối
// + dấu). Fuzz thuần ngẫu nhiên phần lớn tạo ra từ hỏng ngay từ ký tự thứ
// hai nên không chạm được vào các luật thú vị; trộn thêm dạng này thì các
// đường đi thật (đặt/di chuyển thanh, họ vần uo, âm cuối…) mới chạy.
void AppendSyllableKeys(Rng& rng, const Config& cfg,
                        std::vector<char32_t>& out) {
  static const char* kOnsets[] = {"",   "b",  "c",  "ch", "d",  "g",  "gh",
                                  "gi", "h",  "k",  "kh", "l",  "m",  "n",
                                  "ng", "ngh", "nh", "ph", "qu", "r",  "s",
                                  "t",  "th", "tr", "v",  "x"};
  static const char* kTelexVowels[] = {
      "a",   "aa",  "aw",   "e",   "ee",  "i",   "o",    "oo",  "ow",
      "u",   "uw",  "w",    "y",   "ai",  "ao",  "au",   "ay",  "ia",
      "ie",  "iee", "iu",   "oa",  "oe",  "oi",  "ua",   "ue",  "uo",
      "uow", "uoi", "uy",   "uye", "uyee", "ieu", "ieeu", "oai", "oay",
      "uay", "uou", "uoiw", "wo",  "wou"};
  static const char* kVniVowels[] = {
      "a",   "a6",  "a8",  "e",   "e6",  "i",    "o",    "o6",  "o7",
      "u",   "u7",  "y",   "ai",  "ao",  "au",   "ay",   "ia",  "ie",
      "ie6", "iu",  "oa",  "oe",  "oi",  "ua",   "ue",   "uo",  "uo7",
      "uy",  "uye", "uye6", "ieu", "ie6u", "oai", "oay",  "uou"};
  static const char* kCodas[] = {"", "", "c", "ch", "m", "n", "ng", "nh",
                                 "p", "t"};
  static const char* kTelexTones[] = {"", "", "s", "f", "r", "x", "j", "z"};
  static const char* kVniTones[] = {"", "", "1", "2", "3", "4", "5", "0"};

  const bool telex = cfg.method == hodion::InputMethod::Telex;
  std::string s;

  // đ gõ bằng dd (Telex) hoặc d9 (VNI).
  if (rng.chance(12)) {
    s += telex ? "dd" : "d9";
  } else {
    s += kOnsets[rng.below(sizeof(kOnsets) / sizeof(kOnsets[0]))];
  }

  const char* vowel =
      telex ? kTelexVowels[rng.below(sizeof(kTelexVowels) / sizeof(char*))]
            : kVniVowels[rng.below(sizeof(kVniVowels) / sizeof(char*))];
  const char* coda = kCodas[rng.below(sizeof(kCodas) / sizeof(char*))];
  const char* tone =
      telex ? kTelexTones[rng.below(sizeof(kTelexTones) / sizeof(char*))]
            : kVniTones[rng.below(sizeof(kVniTones) / sizeof(char*))];

  // Một nửa số lần gõ dấu ngay sau vần, nửa còn lại gõ ở cuối từ (gõ dấu
  // tự do) — hai đường đi khác nhau trong engine.
  if (rng.chance(50)) {
    s += vowel;
    s += tone;
    s += coda;
  } else {
    s += vowel;
    s += coda;
    s += tone;
  }

  for (char c : s) {
    char32_t ch = static_cast<char32_t>(c);
    if (ch >= U'a' && ch <= U'z' && rng.chance(12)) ch -= 32;  // thỉnh thoảng hoa
    out.push_back(ch);
  }
  if (rng.chance(35)) out.push_back(U' ');  // đôi khi chốt từ
}

char32_t RandomKey(Rng& rng, const Config& cfg) {
  // Nghiêng về chữ cái và phím dấu để chuỗi sinh ra giống chữ thật hơn.
  const uint32_t bucket = rng.below(100);
  if (bucket < 45) {
    static const char kVowels[] = "aeiouyAEIOUY";
    return static_cast<char32_t>(kVowels[rng.below(12)]);
  }
  if (bucket < 75) {
    static const char kConsonants[] = "bcdghklmnpqrstvxBCDGHKLMNPQRSTVX";
    return static_cast<char32_t>(kConsonants[rng.below(32)]);
  }
  if (bucket < 92) {
    if (cfg.method == hodion::InputMethod::Telex) {
      static const char kMarks[] = "sfrxjzwadeoSFRXJZWADEO";
      return static_cast<char32_t>(kMarks[rng.below(22)]);
    }
    static const char kDigits[] = "0123456789";
    return static_cast<char32_t>(kDigits[rng.below(10)]);
  }
  static const char kSymbols[] = "[]{}.,! -_1&'\"()";
  return static_cast<char32_t>(kSymbols[rng.below(16)]);
}

}  // namespace

void run_fuzz_tests() {
  Rng rng(0xC0FFEEu);
  Reporter rep;
  // Thống kê để kiểm chứng fuzzer không sinh toàn rác vô hại.
  long stats_rounds = 0, stats_diacritic_rounds = 0;

  const int rounds = FuzzRounds(4000);
  for (int round = 0; round < rounds; ++round) {
    const Config cfg = RandomConfig(rng);
    Engine e(cfg);
    std::vector<Op> ops;
    std::vector<char32_t> pending;  // phần âm tiết còn lại chưa gõ hết
    bool round_made_diacritic = false;
    size_t keys_since_edit = 0;  // số phím đã nhận, chưa dính Backspace
    bool edited = false;         // đã Backspace trong từ hiện tại chưa
    // Mỗi vòng một "tính khí" khác nhau: có vòng gõ là chính, có vòng xóa
    // nhiều hơn gõ (mô phỏng người dùng sửa tới sửa lui).
    const uint32_t bs_rate = 8 + rng.below(40);

    const int len = 1 + static_cast<int>(rng.below(60));
    for (int i = 0; i < len; ++i) {
      ++g_checks;  // mỗi thao tác là một lượt kiểm tra bất biến
      const size_t before_len = e.composition().size();

      // Backspace xen vào bất kỳ đâu trong lúc gõ.
      if (rng.chance(bs_rate)) {
        ops.push_back({Op::Backspace, 0});
        const auto r = e.process_backspace();
        if (!e.self_check()) rep.fail("self_check sau Backspace", ops, cfg);

        if (before_len == 0) {
          if (r.action != Engine::Result::Action::None) {
            rep.fail("Backspace khi rỗng phải trả None", ops, cfg);
          }
        } else {
          // Backspace xóa đúng một ký tự hiển thị, không hơn không kém.
          if (e.composition().size() + 1 != before_len) {
            rep.fail("Backspace không xóa đúng 1 ký tự", ops, cfg);
          }
          if (r.action != Engine::Result::Action::Composing) {
            rep.fail("Backspace khi đang ghép phải trả Composing", ops, cfg);
          }
          edited = true;
          keys_since_edit = 0;
          if (!e.composing()) edited = false;  // xóa sạch → coi như từ mới
        }
        continue;
      }

      if (pending.empty() && rng.chance(60)) {
        AppendSyllableKeys(rng, cfg, pending);  // gõ theo hình dạng âm tiết
      }
      char32_t ch;
      if (!pending.empty()) {
        ch = pending.front();
        pending.erase(pending.begin());
      } else {
        ch = RandomKey(rng, cfg);
      }
      ops.push_back({Op::Key, ch});
      const bool was_composing = e.composing();
      const auto r = e.process_char(ch);

      if (!e.self_check()) rep.fail("self_check sau phím", ops, cfg);

      switch (r.action) {
        case Engine::Result::Action::None:
          if (was_composing) {
            rep.fail("đang ghép mà trả None", ops, cfg);
          }
          if (e.composing()) rep.fail("None mà lại mở composition", ops, cfg);
          break;

        case Engine::Result::Action::Commit:
          if (e.composing()) rep.fail("Commit mà chưa reset", ops, cfg);
          if (!e.raw().empty()) rep.fail("Commit mà raw còn sót", ops, cfg);
          keys_since_edit = 0;
          edited = false;
          break;

        case Engine::Result::Action::Composing: {
          if (!e.composing()) {
            rep.fail("Composing mà không ở trạng thái ghép", ops, cfg);
          }
          if (r.text != e.composition()) {
            rep.fail("text trả về khác composition()", ops, cfg);
          }
          // Một phím thêm nhiều nhất một ký tự hiển thị (có thể không thêm
          // gì khi nó chỉ đánh dấu, hoặc hủy dấu rồi nối chữ).
          const size_t after_len = e.composition().size();
          if (after_len > before_len + 1) {
            rep.fail("một phím sinh quá 1 ký tự", ops, cfg);
          }
          ++keys_since_edit;
          if (!edited && e.raw().size() != keys_since_edit) {
            rep.fail("nhật ký phím lệch số phím đã gõ", ops, cfg);
          }
          break;
        }
      }

      // Chuỗi ra không bao giờ chứa ký tự lạ.
      for (char32_t c : e.composition()) {
        if (c > 127) round_made_diacritic = true;
        if (!IsAllowedOutputChar(c)) {
          rep.fail("codepoint lạ trong chuỗi ra", ops, cfg);
          break;
        }
      }
      // Hàm const không được đổi trạng thái.
      if (e.composition() != e.composition()) {
        rep.fail("composition() không ổn định", ops, cfg);
      }
    }

    if (round_made_diacritic) ++stats_diacritic_rounds;
    ++stats_rounds;

    // Xóa tới cùng: luôn về trạng thái sạch, không kẹt.
    int guard = 0;
    while (e.composing() && guard++ < 200) e.process_backspace();
    if (e.composing()) rep.fail("Backspace không xóa hết được", ops, cfg);
    if (!e.composition().empty()) rep.fail("còn chữ sau khi xóa hết", ops, cfg);
    if (!e.raw().empty()) rep.fail("raw còn sót sau khi xóa hết", ops, cfg);
    if (!e.self_check()) rep.fail("self_check sau khi xóa hết", ops, cfg);
    if (e.process_backspace().action != Engine::Result::Action::None) {
      rep.fail("Backspace thừa phải trả None", ops, cfg);
    }
    ++g_checks;
  }

  if (std::getenv("HODION_FUZZ_STATS")) {
    std::printf("fuzz: %ld/%ld vòng (%.1f%%) sinh được chữ tiếng Việt có dấu\n",
                stats_diacritic_rounds, stats_rounds,
                stats_rounds ? 100.0 * stats_diacritic_rounds / stats_rounds
                             : 0.0);
  }

  // Tính chất biến hình: gõ y hệt nhưng viết hoa thì kết quả phải đúng bằng
  // bản viết hoa của kết quả viết thường (chỉ xét chữ cái, vì Shift của
  // phím ký hiệu cho ra ký tự khác hẳn).
  {
    Rng rngc(0x0CA5E1u);
    const int case_rounds = FuzzRounds(4000) / 2;
    for (int round = 0; round < case_rounds; ++round) {
      const Config cfg = RandomConfig(rngc);
      std::vector<Op> ops;
      Engine lower(cfg);
      Engine upper(cfg);
      const int len = 1 + static_cast<int>(rngc.below(24));
      for (int i = 0; i < len; ++i) {
        ++g_checks;
        if (rngc.chance(15) && lower.composing()) {
          ops.push_back({Op::Backspace, 0});
          lower.process_backspace();
          upper.process_backspace();
        } else {
          char32_t ch = RandomKey(rngc, cfg);
          if (!(ch >= U'a' && ch <= U'z') && !(ch >= U'A' && ch <= U'Z')) {
            ch = U'a' + rngc.below(26);  // chỉ dùng chữ cái cho tính chất này
          }
          const char32_t lo =
              (ch >= U'A' && ch <= U'Z') ? ch + 32 : ch;
          ops.push_back({Op::Key, lo});
          lower.process_char(lo);
          upper.process_char(lo - 32 + 0);  // cùng phím, viết hoa
        }

        std::u32string want;
        for (char32_t c : lower.composition()) want.push_back(ToUpperVn(c));
        if (want != upper.composition()) {
          rep.fail("gõ hoa không ra đúng bản hoa của gõ thường", ops, cfg);
          break;
        }
      }
    }
  }

  // Chốt từ ở mọi thời điểm đều để lại trạng thái sạch.
  Rng rng2(0xBEEF01u);
  for (int round = 0; round < 600; ++round) {
    const Config cfg = RandomConfig(rng2);
    Engine e(cfg);
    const int len = 1 + static_cast<int>(rng2.below(20));
    for (int i = 0; i < len; ++i) {
      e.process_char(RandomKey(rng2, cfg));
      if (rng2.chance(15)) e.process_backspace();
    }
    const std::u32string committed = e.commit();
    EXPECT_TRUE(!e.composing());
    EXPECT_TRUE(e.raw().empty());
    EXPECT_TRUE(e.self_check());
    for (char32_t c : committed) EXPECT_TRUE(IsAllowedOutputChar(c));
  }
}
