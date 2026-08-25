#include "hodion/engine_c.h"

#include <cstring>

#include "hodion/engine.h"
#include "hodion/utf.h"

struct hodion_engine {
  hodion::Engine impl;
};

namespace {

size_t write_out(const std::u32string& text, char* out, size_t cap) {
  const std::string u8 = hodion::utf::to_utf8(text);
  if (out && cap > 0) {
    const size_t n = u8.size() < cap - 1 ? u8.size() : cap - 1;
    std::memcpy(out, u8.data(), n);
    out[n] = '\0';
  }
  return u8.size();
}

int result_out(const hodion::Engine::Result& r, char* out, size_t cap) {
  write_out(r.text, out, cap);
  return static_cast<int>(r.action);
}

}  // namespace

hodion_engine* hodion_engine_create(void) { return new hodion_engine{}; }

void hodion_engine_destroy(hodion_engine* e) { delete e; }

void hodion_engine_set_method(hodion_engine* e, int method) {
  hodion::Config cfg = e->impl.config();
  cfg.method = method == HODION_METHOD_VNI ? hodion::InputMethod::Vni
                                           : hodion::InputMethod::Telex;
  e->impl.set_config(cfg);
}

void hodion_engine_set_tone_style(hodion_engine* e, int style) {
  hodion::Config cfg = e->impl.config();
  cfg.tone_style = style == HODION_TONE_MODERN ? hodion::ToneStyle::Modern
                                               : hodion::ToneStyle::Traditional;
  e->impl.set_config(cfg);
}

void hodion_engine_set_flag(hodion_engine* e, int flag, int value) {
  hodion::Config cfg = e->impl.config();
  const bool v = value != 0;
  switch (flag) {
    case HODION_FLAG_FREE_MARKING: cfg.free_marking = v; break;
    case HODION_FLAG_SPELL_CHECK: cfg.spell_check = v; break;
    case HODION_FLAG_RESTORE_NON_VN: cfg.restore_non_vn = v; break;
    case HODION_FLAG_W_SHORTHAND: cfg.w_shorthand = v; break;
    case HODION_FLAG_TELEX_BRACKETS: cfg.telex_brackets = v; break;
    default: return;
  }
  e->impl.set_config(cfg);
}

int hodion_engine_key(hodion_engine* e, uint32_t codepoint, char* out,
                      size_t out_cap) {
  return result_out(e->impl.process_char(static_cast<char32_t>(codepoint)),
                    out, out_cap);
}

int hodion_engine_backspace(hodion_engine* e, char* out, size_t out_cap) {
  return result_out(e->impl.process_backspace(), out, out_cap);
}

size_t hodion_engine_composition(const hodion_engine* e, char* out,
                                 size_t out_cap) {
  return write_out(e->impl.composition(), out, out_cap);
}

int hodion_engine_composing(const hodion_engine* e) {
  return e->impl.composing() ? 1 : 0;
}

void hodion_engine_reset(hodion_engine* e) { e->impl.reset(); }
