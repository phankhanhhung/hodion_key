#include "composer.h"

#include <algorithm>

namespace hodion::detail {

namespace {

char32_t ascii_lower(char32_t c) {
  return (c >= U'A' && c <= U'Z') ? c + 32 : c;
}

bool is_ascii_upper(char32_t c) { return c >= U'A' && c <= U'Z'; }

void append_literal(ComposeState& st, char32_t key) {
  VChar vc;
  vc.base = ascii_lower(key);
  vc.upper = is_ascii_upper(key);
  st.chars.push_back(vc);
}

// Đoạn nguyên âm cuối cùng của buffer (bỏ qua phụ âm cuối nếu có).
// Trả về [begin, end] theo chỉ số, hoặc begin > end nếu không có nguyên âm.
struct Run {
  int begin = 0;
  int end = -1;
  bool valid() const { return begin <= end; }
};

Run last_vowel_run(const std::vector<VChar>& chars) {
  Run r;
  int i = static_cast<int>(chars.size()) - 1;
  while (i >= 0 && !is_vowel_base(chars[i].base)) --i;
  if (i < 0) return r;
  r.end = i;
  while (i >= 0 && is_vowel_base(chars[i].base)) --i;
  r.begin = i + 1;
  return r;
}

// ---- Luật chung cho hai kiểu gõ ------------------------------------------

// Dấu thanh: cùng phím gõ lần hai → bỏ thanh và chèn phím dưới dạng chữ thường.
void apply_tone_key(ComposeState& st, char32_t key, Tone tone) {
  if (!st.has_vowel()) {
    append_literal(st, key);
    return;
  }
  if (st.tone == tone) {
    st.tone = Tone::None;
    append_literal(st, key);
  } else {
    st.tone = tone;
  }
}

// Mũ â/ê/ô: chỉ nhận khi ký tự vừa gõ trùng nguyên âm (aa, ee, oo);
// gõ lần ba thì hủy (aaa → aa).
bool apply_circumflex(ComposeState& st, char32_t key) {
  const char32_t lc = ascii_lower(key);
  if (st.chars.empty()) return false;
  VChar& last = st.chars.back();
  if (last.base != lc) return false;
  if (last.mark == Mark::None) {
    last.mark = Mark::Circumflex;
    return true;
  }
  if (last.mark == Mark::Circumflex) {
    last.mark = Mark::None;
    append_literal(st, key);
    return true;
  }
  return false;
}

// Móc/trăng (phím w của Telex, số 7/8 của VNI).
//   - uo liền nhau trong vần cuối → ươ (và hủy ngược lại);
//   - ngược lại tìm nguyên âm phù hợp gần cuối nhất: a→ă, o→ơ, u→ư.
// `allow_a`: Telex w áp cho cả a/o/u; VNI tách riêng 7 (o,u) và 8 (a).
bool apply_horn_breve(ComposeState& st, char32_t key, bool allow_a,
                      bool allow_ou) {
  const Run run = last_vowel_run(st.chars);
  if (!run.valid()) return false;

  if (allow_ou) {
    // Cặp "uo" đứng cạnh nhau → hóa/hủy đồng thời.
    for (int i = run.end; i > run.begin; --i) {
      VChar& u = st.chars[i - 1];
      VChar& o = st.chars[i];
      if (u.base != U'u' || o.base != U'o') continue;
      if (u.mark == Mark::None && o.mark == Mark::None) {
        u.mark = Mark::Horn;
        o.mark = Mark::Horn;
        return true;
      }
      if (u.mark == Mark::Horn && o.mark == Mark::Horn) {
        u.mark = Mark::None;
        o.mark = Mark::None;
        append_literal(st, key);
        return true;
      }
      break;
    }
  }

  for (int i = run.end; i >= run.begin; --i) {
    VChar& vc = st.chars[i];
    Mark want;
    if (vc.base == U'a' && allow_a) {
      want = Mark::Breve;
    } else if ((vc.base == U'o' || vc.base == U'u') && allow_ou) {
      want = Mark::Horn;
    } else {
      continue;
    }
    if (vc.mark == Mark::None) {
      vc.mark = want;
      return true;
    }
    if (vc.mark == want) {
      vc.mark = Mark::None;
      append_literal(st, key);
      return true;
    }
    // Nguyên âm đã mang dấu khác (vd â) — thử nguyên âm đứng trước.
  }
  return false;
}

// đ: gạch chữ d đầu từ; gõ lần nữa thì hủy (ddd → dd).
bool apply_stroke(ComposeState& st, char32_t key, bool allow_delayed) {
  if (st.chars.empty()) return false;
  VChar& first = st.chars.front();
  if (first.base != U'd') return false;
  const bool adjacent = st.chars.size() == 1;
  if (!adjacent && !allow_delayed) return false;
  if (first.mark == Mark::None) {
    first.mark = Mark::Stroke;
    return true;
  }
  if (first.mark == Mark::Stroke) {
    first.mark = Mark::None;
    append_literal(st, key);
    return true;
  }
  return false;
}

// ---- Telex ----------------------------------------------------------------

void telex_key(ComposeState& st, char32_t key, const Config& cfg) {
  const char32_t lc = ascii_lower(key);

  switch (lc) {
    case U's': apply_tone_key(st, key, Tone::Acute); return;
    case U'f': apply_tone_key(st, key, Tone::Grave); return;
    case U'r': apply_tone_key(st, key, Tone::Hook); return;
    case U'x': apply_tone_key(st, key, Tone::Tilde); return;
    case U'j': apply_tone_key(st, key, Tone::Dot); return;
    case U'z':
      if (st.tone != Tone::None) {
        st.tone = Tone::None;
      } else {
        append_literal(st, key);
      }
      return;
    case U'a':
    case U'e':
    case U'o':
      if (apply_circumflex(st, key)) return;
      append_literal(st, key);
      return;
    case U'w': {
      // ww → hủy ư sinh từ w đơn, trả lại 'w' thô.
      if (!st.chars.empty() && st.chars.back().from_w) {
        st.chars.pop_back();
        append_literal(st, key);
        return;
      }
      if (apply_horn_breve(st, key, /*allow_a=*/true, /*allow_ou=*/true)) return;
      if (cfg.w_shorthand) {
        VChar vc;
        vc.base = U'u';
        vc.mark = Mark::Horn;
        vc.upper = is_ascii_upper(key);
        vc.from_w = true;
        st.chars.push_back(vc);
        return;
      }
      append_literal(st, key);
      return;
    }
    case U'd':
      if (apply_stroke(st, key, cfg.delayed_d)) return;
      append_literal(st, key);
      return;
    default:
      append_literal(st, key);
      return;
  }
}

// ---- VNI ------------------------------------------------------------------

void vni_key(ComposeState& st, char32_t key) {
  if (key < U'0' || key > U'9') {
    append_literal(st, key);
    return;
  }
  switch (key) {
    case U'1': apply_tone_key(st, key, Tone::Acute); return;
    case U'2': apply_tone_key(st, key, Tone::Grave); return;
    case U'3': apply_tone_key(st, key, Tone::Hook); return;
    case U'4': apply_tone_key(st, key, Tone::Tilde); return;
    case U'5': apply_tone_key(st, key, Tone::Dot); return;
    case U'0':
      if (st.tone != Tone::None) {
        st.tone = Tone::None;
      } else {
        append_literal(st, key);
      }
      return;
    case U'6': {
      // Mũ cho a/e/o gần cuối nhất.
      const Run run = last_vowel_run(st.chars);
      if (run.valid()) {
        for (int i = run.end; i >= run.begin; --i) {
          VChar& vc = st.chars[i];
          if (vc.base != U'a' && vc.base != U'e' && vc.base != U'o') continue;
          if (vc.mark == Mark::None) {
            vc.mark = Mark::Circumflex;
            return;
          }
          if (vc.mark == Mark::Circumflex) {
            vc.mark = Mark::None;
            append_literal(st, key);
            return;
          }
        }
      }
      append_literal(st, key);
      return;
    }
    case U'7':
      if (apply_horn_breve(st, key, /*allow_a=*/false, /*allow_ou=*/true)) return;
      append_literal(st, key);
      return;
    case U'8':
      if (apply_horn_breve(st, key, /*allow_a=*/true, /*allow_ou=*/false)) return;
      append_literal(st, key);
      return;
    case U'9':
      if (apply_stroke(st, key, /*allow_delayed=*/true)) return;
      append_literal(st, key);
      return;
    default:
      append_literal(st, key);
      return;
  }
}

// ---- Đặt dấu thanh --------------------------------------------------------

// Trả về chỉ số ký tự nhận dấu thanh, hoặc -1.
int tone_position(const std::vector<VChar>& chars, ToneStyle style) {
  const int n = static_cast<int>(chars.size());

  int first_v = 0;
  while (first_v < n && !is_vowel_base(chars[first_v].base)) ++first_v;
  if (first_v >= n) return -1;

  // "qu" và "gi": u/i thuộc phụ âm đầu khi còn nguyên âm khác theo sau.
  const bool next_is_vowel =
      first_v + 1 < n && is_vowel_base(chars[first_v + 1].base);
  if (next_is_vowel && first_v > 0) {
    const char32_t prev = chars[first_v - 1].base;
    const char32_t cur = chars[first_v].base;
    if ((cur == U'u' && prev == U'q' && chars[first_v].mark == Mark::None) ||
        (cur == U'i' && prev == U'g' && chars[first_v].mark == Mark::None)) {
      ++first_v;
    }
  }

  int end = first_v;
  while (end + 1 < n && is_vowel_base(chars[end + 1].base)) ++end;
  const int len = end - first_v + 1;

  // Ưu tiên nguyên âm mang dấu phụ (ê, ơ, â…); nếu cả cặp ươ thì lấy ơ.
  int last_marked = -1;
  for (int i = first_v; i <= end; ++i) {
    const Mark m = chars[i].mark;
    if (m == Mark::Breve || m == Mark::Circumflex || m == Mark::Horn) {
      last_marked = i;
    }
  }
  if (last_marked >= 0) return last_marked;

  if (len == 1) return first_v;

  const bool has_coda = end < n - 1;
  if (len == 2) {
    if (has_coda) return first_v + 1;
    const char32_t v1 = chars[first_v].base;
    const char32_t v2 = chars[first_v + 1].base;
    const bool glide_pair = (v1 == U'o' && (v2 == U'a' || v2 == U'e')) ||
                            (v1 == U'u' && v2 == U'y');
    if (glide_pair && style == ToneStyle::Modern) return first_v + 1;
    return first_v;
  }
  return first_v + 1;  // 3 nguyên âm → nguyên âm giữa (oai, uyu, oeo…)
}

}  // namespace

bool ComposeState::has_vowel() const {
  return std::any_of(chars.begin(), chars.end(),
                     [](const VChar& c) { return is_vowel_base(c.base); });
}

void apply_key(ComposeState& st, char32_t key, const Config& cfg) {
  if (cfg.method == InputMethod::Telex) {
    telex_key(st, key, cfg);
  } else {
    vni_key(st, key);
  }
}

std::u32string render(const ComposeState& st, ToneStyle style) {
  const int pos =
      st.tone == Tone::None ? -1 : tone_position(st.chars, style);

  std::u32string out;
  out.reserve(st.chars.size());
  for (int i = 0; i < static_cast<int>(st.chars.size()); ++i) {
    const VChar& c = st.chars[i];
    const Tone t = (i == pos) ? st.tone : Tone::None;
    out.push_back(composed_char(c.base, c.mark, t, c.upper));
  }
  return out;
}

}  // namespace hodion::detail
