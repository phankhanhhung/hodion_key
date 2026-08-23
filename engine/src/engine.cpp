#include "hodion/engine.h"

#include "composer.h"

namespace hodion {

namespace {

bool is_ascii_letter(char32_t c) {
  const char32_t lc = (c >= U'A' && c <= U'Z') ? c + 32 : c;
  return lc >= U'a' && lc <= U'z';
}

bool is_ascii_digit(char32_t c) { return c >= U'0' && c <= U'9'; }

}  // namespace

void Engine::set_config(const Config& cfg) {
  cfg_ = cfg;
  reset();
}

std::u32string Engine::composition() const {
  detail::ComposeState st;
  for (const char32_t key : raw_) detail::apply_key(st, key, cfg_);
  return detail::render(st, cfg_.tone_style);
}

std::u32string Engine::raw() const {
  return std::u32string(raw_.begin(), raw_.end());
}

std::u32string Engine::commit() {
  std::u32string text = composition();
  reset();
  return text;
}

Engine::Result Engine::composing_result() const {
  Result r;
  r.action = Result::Action::Composing;
  r.text = composition();
  return r;
}

Engine::Result Engine::process_char(char32_t ch) {
  if (raw_.empty()) {
    // Chỉ chữ cái ASCII mới mở một từ mới; mọi thứ khác đi thẳng tới app.
    if (!is_ascii_letter(ch)) return Result{};
    raw_.push_back(ch);
    return composing_result();
  }

  const bool continues =
      is_ascii_letter(ch) ||
      (cfg_.method == InputMethod::Vni && is_ascii_digit(ch));

  if (continues && raw_.size() < kMaxRaw) {
    raw_.push_back(ch);
    return composing_result();
  }

  // Ký tự ngắt từ (hoặc buffer quá dài): chốt từ đang gõ kèm ký tự đó.
  Result r;
  r.action = Result::Action::Commit;
  r.text = composition();
  r.text.push_back(ch);
  reset();
  return r;
}

Engine::Result Engine::process_backspace() {
  if (raw_.empty()) return Result{};
  raw_.pop_back();
  return composing_result();
}

}  // namespace hodion
