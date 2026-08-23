#include "hodion/engine.h"

#include <vector>

#include "vnlexi.h"
#include "word.h"

namespace hodion {

using detail::HookKind;
using detail::KeyOutcome;
using detail::Word;

namespace {

char32_t ascii_lower(char32_t c) {
  return (c >= U'A' && c <= U'Z') ? c + 32 : c;
}

bool is_ascii_letter(char32_t c) {
  const char32_t lc = ascii_lower(c);
  return lc >= U'a' && lc <= U'z';
}

// Ký tự ngắt từ (theo UniKey; ~ ` ^ và chữ số KHÔNG ngắt từ — chúng là
// ký tự "ngoại lai" nằm trong từ).
bool is_break_char(char32_t c) {
  if (c <= U' ') return true;  // khoảng trắng + ký tự điều khiển
  switch (c) {
    case U',': case U';': case U':': case U'.': case U'"': case U'\'':
    case U'!': case U'?': case U'<': case U'>': case U'=': case U'+':
    case U'-': case U'*': case U'/': case U'\\': case U'_': case U'@':
    case U'#': case U'$': case U'%': case U'&': case U'(': case U')':
    case U'{': case U'}': case U'[': case U']': case U'|':
      return true;
    default:
      return false;
  }
}

enum class EvKind : uint8_t {
  Normal,
  Tone,
  RoofAny,     // VNI 6
  RoofTarget,  // Telex a/e/o
  HookUO,      // VNI 7 (chỉ ơ/ư)
  Bowl,        // VNI 8 (chỉ ă)
  Dd,
  TelexW,
  MapChar,     // [ ] { } → ơ ư Ơ Ư
};

struct Ev {
  EvKind kind = EvKind::Normal;
  detail::ToneId tone = 0;
  char32_t target = 0;  // RoofTarget: 'a' | 'e' | 'o'
  char32_t sym = 0;     // MapChar
};

Ev classify(char32_t ch, const Config& cfg) {
  Ev ev;
  if (cfg.method == InputMethod::Telex) {
    switch (ascii_lower(ch)) {
      case U'z': ev.kind = EvKind::Tone; ev.tone = 0; return ev;
      case U's': ev.kind = EvKind::Tone; ev.tone = 1; return ev;
      case U'f': ev.kind = EvKind::Tone; ev.tone = 2; return ev;
      case U'r': ev.kind = EvKind::Tone; ev.tone = 3; return ev;
      case U'x': ev.kind = EvKind::Tone; ev.tone = 4; return ev;
      case U'j': ev.kind = EvKind::Tone; ev.tone = 5; return ev;
      case U'a': ev.kind = EvKind::RoofTarget; ev.target = U'a'; return ev;
      case U'e': ev.kind = EvKind::RoofTarget; ev.target = U'e'; return ev;
      case U'o': ev.kind = EvKind::RoofTarget; ev.target = U'o'; return ev;
      case U'w': ev.kind = EvKind::TelexW; return ev;
      case U'd': ev.kind = EvKind::Dd; return ev;
      default: break;
    }
    if (cfg.telex_brackets) {
      switch (ch) {
        case U'[': ev.kind = EvKind::MapChar; ev.sym = U'ơ'; return ev;
        case U']': ev.kind = EvKind::MapChar; ev.sym = U'ư'; return ev;
        case U'{': ev.kind = EvKind::MapChar; ev.sym = U'Ơ'; return ev;
        case U'}': ev.kind = EvKind::MapChar; ev.sym = U'Ư'; return ev;
        default: break;
      }
    }
  } else {  // VNI
    switch (ch) {
      case U'0': ev.kind = EvKind::Tone; ev.tone = 0; return ev;
      case U'1': ev.kind = EvKind::Tone; ev.tone = 1; return ev;
      case U'2': ev.kind = EvKind::Tone; ev.tone = 2; return ev;
      case U'3': ev.kind = EvKind::Tone; ev.tone = 3; return ev;
      case U'4': ev.kind = EvKind::Tone; ev.tone = 4; return ev;
      case U'5': ev.kind = EvKind::Tone; ev.tone = 5; return ev;
      case U'6': ev.kind = EvKind::RoofAny; return ev;
      case U'7': ev.kind = EvKind::HookUO; return ev;
      case U'8': ev.kind = EvKind::Bowl; return ev;
      case U'9': ev.kind = EvKind::Dd; return ev;
      default: break;
    }
  }
  return ev;
}

}  // namespace

struct Engine::Impl {
  Config cfg{};
  Word word;
  std::vector<char32_t> keys;   // phím thô của từ hiện tại
  bool any_converted = false;   // có phím nào gây biến đổi chưa
  // Nhật ký phím còn tả đúng từ đang hiển thị không. Backspace xóa một ký
  // tự, mà một ký tự có thể do nhiều phím tạo ra (vieejt → 6 phím, 4 ký
  // tự), nên sau khi xóa ta không thể dựng lại chuỗi phím đã gõ.
  bool keys_valid = true;

  static constexpr size_t kMaxCells = 40;

  void reset() {
    word.clear();
    keys.clear();
    any_converted = false;
    keys_valid = true;
  }

  // Chuỗi chốt từ: khôi phục phím thô nếu bật tùy chọn và từ không phải
  // tiếng Việt hợp lệ (mirror hành vi autoNonVnRestore của UniKey).
  std::u32string commit_text() const {
    if (cfg.restore_non_vn && cfg.spell_check && !word.single_mode() &&
        keys_valid && any_converted && word.is_non_vn(cfg) &&
        word.has_vn_mark()) {
      return std::u32string(keys.begin(), keys.end());
    }
    return word.render();
  }
};

Engine::Engine() : impl_(new Impl) {}
Engine::Engine(const Config& cfg) : impl_(new Impl) { impl_->cfg = cfg; }
Engine::~Engine() = default;
Engine::Engine(Engine&&) noexcept = default;
Engine& Engine::operator=(Engine&&) noexcept = default;

const Config& Engine::config() const { return impl_->cfg; }

void Engine::set_config(const Config& cfg) {
  impl_->cfg = cfg;
  impl_->reset();
}

bool Engine::starts_word(char32_t ch) const {
  if (is_ascii_letter(ch)) return true;
  if (impl_->cfg.method == InputMethod::Telex && impl_->cfg.telex_brackets) {
    return ch == U'[' || ch == U']' || ch == U'{' || ch == U'}';
  }
  return false;
}

bool Engine::composing() const { return !impl_->word.empty(); }

std::u32string Engine::composition() const { return impl_->word.render(); }

std::u32string Engine::raw() const {
  if (!impl_->keys_valid) return impl_->word.render();
  return std::u32string(impl_->keys.begin(), impl_->keys.end());
}

std::u32string Engine::commit() {
  std::u32string text = impl_->commit_text();
  impl_->reset();
  return text;
}

void Engine::reset() { impl_->reset(); }

bool Engine::self_check() const {
  const Impl& im = *impl_;
  if (!im.word.check_invariants()) return false;
  // Nhật ký phím chỉ tồn tại khi còn tin cậy được.
  if (!im.keys_valid && !im.keys.empty()) return false;
  // Không ghép gì thì không còn gì sót lại.
  if (im.word.empty() && !im.keys.empty()) return false;
  return true;
}

Engine::Result Engine::process_char(char32_t ch) {
  Impl& im = *impl_;
  const Config& cfg = im.cfg;

  if (im.word.empty() && !starts_word(ch)) return Result{};

  const Ev ev = classify(ch, cfg);

  // Ký tự ngắt từ (trừ khi là phím MapChar của Telex) → chốt từ kèm ký tự.
  if (is_break_char(ch) && ev.kind != EvKind::MapChar) {
    Result r;
    r.action = Result::Action::Commit;
    r.text = im.commit_text();
    r.text.push_back(ch);
    im.reset();
    return r;
  }

  KeyOutcome out = KeyOutcome::NotApplicable;
  switch (ev.kind) {
    case EvKind::Tone:
      out = im.word.tone(ch, ev.tone, cfg);
      break;
    case EvKind::RoofAny:
      out = im.word.roof(ch, 0, cfg);
      break;
    case EvKind::RoofTarget:
      out = im.word.roof(ch, ev.target, cfg);
      break;
    case EvKind::HookUO:
      out = im.word.hook(ch, HookKind::HornOnly, cfg);
      break;
    case EvKind::Bowl:
      out = im.word.hook(ch, HookKind::BreveOnly, cfg);
      break;
    case EvKind::Dd:
      out = im.word.stroke_d(ch, cfg);
      break;
    case EvKind::TelexW:
      out = im.word.telex_w(ch, cfg);
      break;
    case EvKind::MapChar: {
      out = im.word.map_char(ch, ev.sym, false, cfg);
      if (out == KeyOutcome::Reverted || out == KeyOutcome::NotApplicable) {
        // Phím gốc ([, ], {, }) là ký tự ngắt từ → chốt phần còn lại kèm nó.
        if (out == KeyOutcome::Reverted) im.any_converted = true;
        Result r;
        r.action = Result::Action::Commit;
        r.text = im.commit_text();
        r.text.push_back(ch);
        im.reset();
        return r;
      }
      break;
    }
    case EvKind::Normal:
      break;
  }

  if (out == KeyOutcome::NotApplicable) {
    im.word.append(ch, cfg);
    out = KeyOutcome::Appended;
  }
  im.word.maybe_restart_word(cfg);

  if (im.keys_valid) im.keys.push_back(ch);
  if (out == KeyOutcome::Applied || out == KeyOutcome::Reverted) {
    im.any_converted = true;
  }

  // Chặn từ phình vô hạn (URL, mã hash…): tự chốt như một từ.
  if (im.word.size() >= Impl::kMaxCells) {
    Result r;
    r.action = Result::Action::Commit;
    r.text = im.commit_text();
    im.reset();
    return r;
  }

  Result r;
  r.action = Result::Action::Composing;
  r.text = im.word.render();
  return r;
}

Engine::Result Engine::process_backspace() {
  Impl& im = *impl_;
  if (im.word.empty()) return Result{};

  im.word.backspace(im.cfg);
  // Bỏ nhật ký phím: nó không còn khớp với chữ đang hiển thị. Nếu xóa hết
  // thì coi như bắt đầu lại sạch sẽ và nhật ký dùng được cho từ kế tiếp.
  im.keys.clear();
  im.any_converted = false;
  im.keys_valid = im.word.empty();

  Result r;
  r.action = Result::Action::Composing;
  r.text = im.word.render();
  return r;
}

}  // namespace hodion
