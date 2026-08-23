#include "word.h"

#include <algorithm>

namespace hodion::detail {

namespace {

char32_t ascii_lower(char32_t c) {
  return (c >= U'A' && c <= U'Z') ? c + 32 : c;
}

bool is_ascii_upper(char32_t c) { return c >= U'A' && c <= U'Z'; }

bool is_ascii_letter(char32_t c) {
  const char32_t lc = ascii_lower(c);
  return lc >= U'a' && lc <= U'z';
}

// f, j, w không thuộc bảng chữ tiếng Việt — chúng là ký tự "ngoại lai"
// (không mở/nối âm tiết hợp lệ), giống phân loại của UniKey.
bool is_vn_letter(char32_t lower) {
  return lower >= U'a' && lower <= U'z' && lower != U'f' && lower != U'j' &&
         lower != U'w';
}

bool is_stop_coda(int cs) {
  return cs == kCSeq_c || cs == kCSeq_ch || cs == kCSeq_p || cs == kCSeq_t;
}

}  // namespace

// Toàn bộ thao tác nội bộ gom vào WordOps để truy cập cells_.
struct WordOps {
  Word& w;
  const Config& cfg;

  std::vector<Cell>& cells() { return w.cells_; }
  int last_idx() const { return static_cast<int>(w.cells_.size()) - 1; }
  Cell& at(int i) { return w.cells_[i]; }

  // ---- Thông tin cấu trúc hiện tại ----------------------------------------

  int v_end() const { return w.cells_.empty() ? -1 : w.cells_.back().v_end; }

  int cur_vs() const {
    const int ve = v_end();
    return ve < 0 ? kNoSeq : w.cells_[ve].vs;
  }

  int v_start() const {
    const int ve = v_end();
    if (ve < 0) return -1;
    return ve - vseq(w.cells_[ve].vs).len + 1;
  }

  int onset_cs() const {
    if (w.cells_.empty()) return kNoSeq;
    const int oe = w.cells_.back().onset_end;
    return oe < 0 ? kNoSeq : w.cells_[oe].cs;
  }

  int coda_cs() const {
    if (w.cells_.empty()) return kNoSeq;
    const Cell& last = w.cells_.back();
    return (last.form == Form::VC || last.form == Form::CVC) ? last.cs
                                                             : kNoSeq;
  }

  bool terminated() const { return v_end() == last_idx(); }

  // Cập nhật vs của từng ô trong vần theo các prefix của vần mới.
  void set_vseq_cells(int vstart, int new_vs) {
    if (new_vs < 0 || vstart < 0) return;
    const VSeqInfo& info = vseq(new_vs);
    const int len = info.len < 3 ? info.len : 3;
    for (int i = 0; i < len; ++i) {
      VGlyph prefix[3];
      for (int k = 0; k <= i && k < 3; ++k) prefix[k] = info.g[k];
      at(vstart + i).vs = static_cast<int16_t>(vseq_lookup(prefix, i + 1));
      at(vstart + i).base = info.g[i].base;
      at(vstart + i).mark = info.g[i].mark;
    }
  }

  // Di chuyển dấu thanh khi vị trí tính theo vần mới khác vị trí cũ.
  void retone(int vstart, int old_pos, ToneId tone, int new_vs,
              bool term_now) {
    if (tone == 0) return;
    const int new_pos =
        vstart + tone_offset(new_vs, term_now,
                             cfg.tone_style == ToneStyle::Modern);
    if (new_pos != old_pos) {
      at(new_pos).tone = tone;
      at(old_pos).tone = 0;
    }
  }

  int tone_pos(int vs, bool term) const {
    return v_start() + tone_offset(vs, term,
                                   cfg.tone_style == ToneStyle::Modern);
  }

  // ---- Nối ký tự (mirror appendVowel/appendConsonnant/nonVn) --------------

  void push_other(char32_t key) {
    Cell c;
    c.key = key;
    if (is_ascii_letter(key)) {
      c.base = ascii_lower(key);
      c.upper = is_ascii_upper(key);
    }
    c.form = Form::NonVn;
    cells().push_back(c);
    w.glue_class_vn_ = false;
  }

  void push_vowel(char32_t key, VGlyph g, bool upper, bool from_w) {
    Cell c;
    c.key = key;
    c.base = g.base;
    c.mark = g.mark;
    c.upper = upper;
    c.from_w = from_w;
    w.glue_class_vn_ = true;

    if (cells().empty()) {
      c.form = Form::V;
      c.vs = static_cast<int16_t>(vseq_lookup(&g, 1));
      c.v_end = 0;
      cells().push_back(c);
      return;
    }

    const Cell& prev = cells().back();
    const int prev_idx = last_idx();

    switch (prev.form) {
      case Form::NonVn:
      case Form::VC:
      case Form::CVC:
        c.form = Form::NonVn;
        break;

      case Form::V:
      case Form::CV: {
        const int vs = prev.vs;
        const VSeqInfo& info = vseq(vs);
        int new_vs = kNoSeq;
        if (info.len < 3) {
          VGlyph seq[3];
          for (int i = 0; i < info.len; ++i) seq[i] = info.g[i];
          seq[info.len] = g;
          new_vs = vseq_lookup(seq, info.len + 1);
        }
        if (new_vs != kNoSeq && prev.form == Form::CV &&
            !valid_cv(onset_cs(), new_vs)) {
          new_vs = kNoSeq;
        }
        if (new_vs == kNoSeq) {
          c.form = Form::NonVn;
          break;
        }

        const int vstart = prev_idx - info.len + 1;
        const int old_pos = tone_pos(vs, true);
        const ToneId tone = at(old_pos).tone;

        c.form = prev.form;
        c.onset_end = prev.onset_end;
        c.v_end = static_cast<int16_t>(prev_idx + 1);
        c.vs = static_cast<int16_t>(new_vs);
        cells().push_back(c);

        retone(vstart, old_pos, tone, new_vs, true);
        return;
      }

      case Form::C: {
        const int new_vs = vseq_lookup(&g, 1);
        if (!valid_cv(prev.cs, new_vs)) {
          c.form = Form::NonVn;
          break;
        }
        c.form = Form::CV;
        c.onset_end = static_cast<int16_t>(prev_idx);
        c.v_end = static_cast<int16_t>(prev_idx + 1);
        c.vs = static_cast<int16_t>(new_vs);
        const bool gi_tone = prev.cs == kCSeq_gi && at(prev_idx).tone != 0;
        cells().push_back(c);
        if (gi_tone) {
          // gì + a → già: thanh chuyển từ i sang nguyên âm mới.
          cells().back().tone = at(prev_idx).tone;
          at(prev_idx).tone = 0;
        }
        return;
      }
    }
    cells().push_back(c);
  }

  void push_consonant(char32_t key, char32_t lower, bool upper) {
    Cell c;
    c.key = key;
    c.base = lower;
    c.upper = upper;
    w.glue_class_vn_ = true;

    VGlyph g{lower, Mark::None};

    if (cells().empty()) {
      c.form = Form::C;
      c.cs = static_cast<int16_t>(cseq_lookup(&g, 1));
      c.onset_end = 0;
      cells().push_back(c);
      return;
    }

    const Cell& prev = cells().back();
    const int prev_idx = last_idx();

    switch (prev.form) {
      case Form::NonVn:
        c.form = Form::NonVn;
        break;

      case Form::V:
      case Form::CV: {
        const int vs = prev.vs;
        // ưo/uơ tự hoàn thành thành ươ khi có âm cuối hợp lệ (thuơ+ng→thương).
        int adj_vs = vs;
        if (vs == kVSeq_uho || vs == kVSeq_uoh) adj_vs = kVSeq_uhoh;

        const int new_cs = cseq_lookup(&g, 1);
        const int c1 = prev.onset_end >= 0 ? at(prev.onset_end).cs : kNoSeq;
        if (!valid_cvc(c1, adj_vs, new_cs)) {
          c.form = Form::NonVn;
          break;
        }

        const int vstart = prev_idx - vseq(vs).len + 1;
        const int old_pos = tone_pos(vs, true);
        const ToneId tone = at(old_pos).tone;

        if (adj_vs != vs) set_vseq_cells(vstart, adj_vs);

        c.form = prev.form == Form::V ? Form::VC : Form::CVC;
        c.cs = static_cast<int16_t>(new_cs);
        c.onset_end = prev.onset_end;
        c.v_end = prev.v_end;
        cells().push_back(c);

        retone(vstart, old_pos, tone, adj_vs, false);
        return;
      }

      case Form::C: {
        const int prev_cs = prev.cs;
        int new_cs = kNoSeq;
        if (prev_cs != kNoSeq && cseq(prev_cs).len < 3) {
          VGlyph seq[3];
          const CSeqInfo& info = cseq(prev_cs);
          for (int i = 0; i < info.len; ++i) seq[i] = info.g[i];
          seq[info.len] = g;
          new_cs = cseq_lookup(seq, info.len + 1);
        }
        if (new_cs == kNoSeq) {
          c.form = Form::NonVn;
          break;
        }
        c.form = Form::C;
        c.cs = static_cast<int16_t>(new_cs);
        c.onset_end = static_cast<int16_t>(prev_idx + 1);
        break;
      }

      case Form::VC:
      case Form::CVC: {
        const int prev_cs = prev.cs;
        int new_cs = kNoSeq;
        if (prev_cs != kNoSeq && cseq(prev_cs).len < 3) {
          VGlyph seq[3];
          const CSeqInfo& info = cseq(prev_cs);
          for (int i = 0; i < info.len; ++i) seq[i] = info.g[i];
          seq[info.len] = g;
          new_cs = cseq_lookup(seq, info.len + 1);
        }
        if (new_cs != kNoSeq) {
          const int c1 = prev.onset_end >= 0 ? at(prev.onset_end).cs : kNoSeq;
          if (!valid_cvc(c1, at(prev.v_end).vs, new_cs)) new_cs = kNoSeq;
        }
        if (new_cs == kNoSeq) {
          c.form = Form::NonVn;
          break;
        }
        c.form = prev.form;
        c.cs = static_cast<int16_t>(new_cs);
        c.onset_end = prev.onset_end;
        c.v_end = prev.v_end;
        break;
      }
    }
    cells().push_back(c);
  }

  void append(char32_t key) {
    const char32_t lower = ascii_lower(key);
    const bool upper = is_ascii_upper(key);

    if (!is_ascii_letter(key) || !is_vn_letter(lower)) {
      push_other(key);
      return;
    }

    if (is_vowel_letter(lower)) {
      // qu + u, g + i: nguyên âm bị hút vào phụ âm đầu (qu/gi).
      if (!cells().empty() && cells().back().form == Form::C &&
          ((cells().back().cs == kCSeq_q && lower == U'u') ||
           (cells().back().cs == kCSeq_g && lower == U'i'))) {
        push_consonant(key, lower, upper);
        return;
      }
      push_vowel(key, {lower, Mark::None}, upper, false);
      return;
    }
    push_consonant(key, lower, upper);
  }

  // ---- Xóa ô cuối kèm cứu dấu thanh (mirror mapchar-undo) -----------------

  void remove_last_rescue_tone() {
    if (cells().empty()) return;
    const Cell removed = cells().back();
    if (removed.form == Form::V || removed.form == Form::CV) {
      const int vs = removed.vs;
      const int vstart = last_idx() - vseq(vs).len + 1;
      const int old_pos = tone_pos(vs, true);
      const ToneId tone = at(old_pos).tone;
      cells().pop_back();
      if (tone != 0 && !cells().empty() &&
          (cells().back().form == Form::V ||
           cells().back().form == Form::CV)) {
        const int new_vs = cells().back().vs;
        const int new_pos =
            vstart + tone_offset(new_vs, true,
                                 cfg.tone_style == ToneStyle::Modern);
        if (new_pos != old_pos) {
          at(new_pos).tone = tone;
          if (old_pos <= last_idx()) at(old_pos).tone = 0;
        }
      }
      return;
    }
    cells().pop_back();
  }
};

// ---------------------------------------------------------------------------

std::u32string Word::render() const {
  std::u32string out;
  out.reserve(cells_.size());
  for (const Cell& c : cells_) {
    if (c.base != 0) {
      out.push_back(composed_char(c.base, c.mark, c.tone, c.upper));
    } else {
      out.push_back(c.key);
    }
  }
  return out;
}

void Word::append(char32_t key, const Config& cfg) {
  WordOps{*this, cfg}.append(key);
}

// ---- Mũ (a/e/o của Telex, 6 của VNI) --------------------------------------

KeyOutcome Word::roof(char32_t key, char32_t target, const Config& cfg) {
  WordOps ops{*this, cfg};
  if (cells_.empty() || ops.v_end() < 0) return KeyOutcome::NotApplicable;

  const int last = ops.last_idx();
  const int ve = ops.v_end();
  const int vs = ops.cur_vs();
  const VSeqInfo& info = vseq(vs);
  const int vstart = ve - info.len + 1;
  const bool term = ve == last;
  const int old_tone_pos = ops.tone_pos(vs, term);
  const ToneId tone = cells_[old_tone_pos].tone;

  // ưo/ươ/ưoi/ươi + mũ → uô/uôi (mũ thắng móc trên cả cụm).
  const bool double_uo = vs == kVSeq_uho || vs == kVSeq_uhoh ||
                         vs == kVSeq_uhoi || vs == kVSeq_uhohi;

  int new_vs;
  if (double_uo) {
    VGlyph seq[3] = {{U'u', Mark::None}, {U'o', Mark::Circumflex}, {}};
    int len = 2;
    if (info.len == 3) {
      seq[2] = info.g[2];
      len = 3;
    }
    new_vs = vseq_lookup(seq, len);
  } else {
    new_vs = info.with_roof;
  }

  if (new_vs == kNoSeq) {
    if (info.roof_pos < 0) return KeyOutcome::NotApplicable;

    // Đã có mũ → gõ lặp thì hủy mũ và nối phím thành chữ thường.
    const int pos = vstart + info.roof_pos;
    if (target != 0 && cells_[pos].base != target) {
      return KeyOutcome::NotApplicable;
    }
    if (!cfg.free_marking && pos != last) return KeyOutcome::NotApplicable;

    cells_[pos].mark = Mark::None;
    VGlyph seq[3];
    for (int i = 0; i < info.len; ++i) seq[i] = cells_[vstart + i].glyph();
    const int undo_vs = vseq_lookup(seq, info.len);
    if (undo_vs != kNoSeq) ops.set_vseq_cells(vstart, undo_vs);
    if (undo_vs != kNoSeq) ops.retone(vstart, old_tone_pos, tone, undo_vs, term);
    single_mode_ = false;
    ops.append(key);
    return KeyOutcome::Reverted;
  }

  const VSeqInfo& ninfo = vseq(new_vs);
  if (target != 0 && ninfo.g[ninfo.roof_pos].base != target) {
    return KeyOutcome::NotApplicable;
  }
  if (!valid_cvc(ops.onset_cs(), new_vs, ops.coda_cs())) {
    return KeyOutcome::NotApplicable;
  }
  const int change_pos = double_uo ? vstart : vstart + ninfo.roof_pos;
  if (!cfg.free_marking && change_pos != last) return KeyOutcome::NotApplicable;

  ops.set_vseq_cells(vstart, new_vs);
  ops.retone(vstart, old_tone_pos, tone, new_vs, term);
  return KeyOutcome::Applied;
}

// ---- Móc/trăng (w của Telex, 7/8 của VNI) ---------------------------------

KeyOutcome Word::hook(char32_t key, HookKind kind, const Config& cfg) {
  WordOps ops{*this, cfg};
  if (cells_.empty() || ops.v_end() < 0) return KeyOutcome::NotApplicable;

  const int last = ops.last_idx();
  const int ve = ops.v_end();
  const int vs = ops.cur_vs();
  const VSeqInfo& info = vseq(vs);
  const int vstart = ve - info.len + 1;
  const bool term = ve == last;

  // Họ uo: (u|ư)(o|ô|ơ)… có luật riêng.
  if (info.len > 1 && kind != HookKind::BreveOnly &&
      info.g[0].base == U'u' && info.g[1].base == U'o') {
    if (!cfg.free_marking && !term) return KeyOutcome::NotApplicable;

    const int old_tone_pos = ops.tone_pos(vs, term);
    const ToneId tone = cells_[old_tone_pos].tone;
    const VGlyph v0 = info.g[0];
    const VGlyph v1 = info.g[1];
    VGlyph seq[3] = {v0, v1, {}};
    if (info.len == 3) seq[2] = info.g[2];
    bool removed = false;

    if (v0.mark == Mark::None) {
      if (v1.mark == Mark::None || v1.mark == Mark::Circumflex) {
        // uo|uô sau "th", ở cuối từ → uơ (gõ "thuow" ra thuơ — cho "thuở").
        if ((vs == kVSeq_uo || vs == kVSeq_uor) && term &&
            cells_.back().form == Form::CV && ops.onset_cs() == kCSeq_th) {
          seq[1] = {U'o', Mark::Horn};
        } else {
          seq[0] = {U'u', Mark::Horn};
          seq[1] = {U'o', Mark::Horn};
        }
      } else {  // v1 == ơ → uơ → ươ
        seq[0] = {U'u', Mark::Horn};
      }
    } else {  // v0 == ư
      if (v1.mark == Mark::None) {  // ưo → ươ
        seq[1] = {U'o', Mark::Horn};
      } else {  // ươ → hủy về uo
        seq[0] = {U'u', Mark::None};
        seq[1] = {U'o', Mark::None};
        removed = true;
      }
    }

    const int new_vs = vseq_lookup(seq, info.len);
    if (new_vs == kNoSeq) return KeyOutcome::NotApplicable;
    ops.set_vseq_cells(vstart, new_vs);
    ops.retone(vstart, old_tone_pos, tone, new_vs, term);
    if (removed) {
      single_mode_ = false;
      ops.append(key);
      return KeyOutcome::Reverted;
    }
    return KeyOutcome::Applied;
  }

  const int old_tone_pos = ops.tone_pos(vs, term);
  const ToneId tone = cells_[old_tone_pos].tone;

  const int new_vs = info.with_hook;
  if (new_vs == kNoSeq) {
    if (info.hook_pos < 0) return KeyOutcome::NotApplicable;

    // Đã có móc/trăng → gõ lặp thì hủy.
    const int pos = vstart + info.hook_pos;
    const VGlyph cur = cells_[pos].glyph();
    if (!cfg.free_marking && pos != last) return KeyOutcome::NotApplicable;
    if (kind == HookKind::BreveOnly && cur.mark != Mark::Breve) {
      return KeyOutcome::NotApplicable;
    }
    if (kind == HookKind::HornOnly && cur.mark == Mark::Breve) {
      return KeyOutcome::NotApplicable;
    }

    cells_[pos].mark = Mark::None;
    VGlyph seq[3];
    for (int i = 0; i < info.len; ++i) seq[i] = cells_[vstart + i].glyph();
    const int undo_vs = vseq_lookup(seq, info.len);
    if (undo_vs != kNoSeq) {
      ops.set_vseq_cells(vstart, undo_vs);
      ops.retone(vstart, old_tone_pos, tone, undo_vs, term);
    }
    single_mode_ = false;
    ops.append(key);
    return KeyOutcome::Reverted;
  }

  const VSeqInfo& ninfo = vseq(new_vs);
  const VGlyph hooked = ninfo.g[ninfo.hook_pos];
  if (kind == HookKind::BreveOnly && hooked.mark != Mark::Breve) {
    return KeyOutcome::NotApplicable;
  }
  if (kind == HookKind::HornOnly && hooked.mark == Mark::Breve) {
    return KeyOutcome::NotApplicable;
  }
  if (!valid_cvc(ops.onset_cs(), new_vs, ops.coda_cs())) {
    return KeyOutcome::NotApplicable;
  }
  const int change_pos = vstart + ninfo.hook_pos;
  if (!cfg.free_marking && change_pos != last) return KeyOutcome::NotApplicable;

  ops.set_vseq_cells(vstart, new_vs);
  ops.retone(vstart, old_tone_pos, tone, new_vs, term);
  return KeyOutcome::Applied;
}

// ---- Dấu thanh ------------------------------------------------------------

KeyOutcome Word::tone(char32_t key, ToneId t, const Config& cfg) {
  WordOps ops{*this, cfg};
  if (cells_.empty()) return KeyOutcome::NotApplicable;

  const Cell& last = cells_.back();

  // gi/gin thuần phụ âm vẫn mang được thanh trên i: gif→gì, ginf→gìn.
  if (last.form == Form::C &&
      (last.cs == kCSeq_gi || last.cs == kCSeq_gin)) {
    const int p = last.cs == kCSeq_gi ? ops.last_idx() : ops.last_idx() - 1;
    if (cells_[p].tone == 0 && t == 0) return KeyOutcome::NotApplicable;
    if (cells_[p].tone == t) {
      cells_[p].tone = 0;
      single_mode_ = false;
      ops.append(key);
      return KeyOutcome::Reverted;
    }
    cells_[p].tone = t;
    return KeyOutcome::Applied;
  }

  if (ops.v_end() < 0) return KeyOutcome::NotApplicable;

  const int vs = ops.cur_vs();
  if (cfg.spell_check && !cfg.free_marking && !vseq(vs).complete) {
    return KeyOutcome::NotApplicable;
  }

  // Âm cuối tắc (c, ch, p, t) chỉ nhận sắc/nặng.
  if (last.form == Form::VC || last.form == Form::CVC) {
    if (is_stop_coda(last.cs) && (t == 2 || t == 3 || t == 4)) {
      return KeyOutcome::NotApplicable;
    }
  }

  const int pos = ops.tone_pos(vs, ops.terminated());
  if (cells_[pos].tone == 0 && t == 0) return KeyOutcome::NotApplicable;
  if (cells_[pos].tone == t) {
    cells_[pos].tone = 0;
    single_mode_ = false;
    ops.append(key);
    return KeyOutcome::Reverted;
  }
  cells_[pos].tone = t;
  return KeyOutcome::Applied;
}

// ---- đ --------------------------------------------------------------------

KeyOutcome Word::stroke_d(char32_t key, const Config& cfg) {
  WordOps ops{*this, cfg};
  if (cells_.empty()) return KeyOutcome::NotApplicable;

  Cell& last = cells_.back();

  // Cho phép đ cả trong chuỗi không phải tiếng Việt (viết tắt: hđ, tđ…)
  // miễn là ký tự đứng trước không phải nguyên âm.
  if (last.form == Form::NonVn && last.base == U'd' &&
      last.mark == Mark::None) {
    const bool prev_vowel =
        cells_.size() >= 2 &&
        is_vowel_letter(cells_[cells_.size() - 2].base);
    if (!prev_vowel) {
      last.mark = Mark::Stroke;
      last.form = Form::C;
      VGlyph g{U'd', Mark::Stroke};
      last.cs = static_cast<int16_t>(cseq_lookup(&g, 1));
      last.onset_end = static_cast<int16_t>(ops.last_idx());
      last.v_end = -1;
      single_mode_ = true;
      return KeyOutcome::Applied;
    }
  }

  const int oe = last.onset_end;
  if (oe < 0) return KeyOutcome::NotApplicable;
  if (!cfg.free_marking && oe != ops.last_idx()) {
    return KeyOutcome::NotApplicable;
  }

  if (cells_[oe].cs == kCSeq_d) {
    cells_[oe].mark = Mark::Stroke;
    cells_[oe].cs = static_cast<int16_t>(kCSeq_dd);
    single_mode_ = true;  // từ bắt đầu bằng đ: không spell-check (viết tắt)
    return KeyOutcome::Applied;
  }
  if (cells_[oe].cs == kCSeq_dd) {
    cells_[oe].mark = Mark::None;
    cells_[oe].cs = static_cast<int16_t>(kCSeq_d);
    single_mode_ = false;
    ops.append(key);
    return KeyOutcome::Reverted;
  }
  return KeyOutcome::NotApplicable;
}

// ---- w của Telex ----------------------------------------------------------

KeyOutcome Word::telex_w(char32_t key, const Config& cfg) {
  WordOps ops{*this, cfg};

  // ww: ư sinh từ w đơn → trả lại 'w' thô.
  if (!cells_.empty() && cells_.back().from_w) {
    ops.remove_last_rescue_tone();
    single_mode_ = false;
    ops.append(key);  // 'w' là ký tự ngoại lai → ô nonVn
    return KeyOutcome::Reverted;
  }

  const KeyOutcome h = hook(key, HookKind::All, cfg);
  if (h != KeyOutcome::NotApplicable) return h;

  if (!cfg.w_shorthand) return KeyOutcome::NotApplicable;

  const bool upper = is_ascii_upper(key);
  const KeyOutcome m = map_char(key, upper ? U'Ư' : U'ư', true, cfg);
  if (m == KeyOutcome::Reverted) {
    // Hủy ư đứng trước → nối 'w' thô.
    ops.append(key);
  }
  return m;
}

// ---- MapChar (w→ư, [→ơ, ]→ư…) ---------------------------------------------

KeyOutcome Word::map_char(char32_t key, char32_t sym, bool set_from_w,
                          const Config& cfg) {
  WordOps ops{*this, cfg};

  VGlyph g;
  bool upper = false;
  switch (sym) {
    case U'ơ': g = {U'o', Mark::Horn}; break;
    case U'Ơ': g = {U'o', Mark::Horn}; upper = true; break;
    case U'ư': g = {U'u', Mark::Horn}; break;
    case U'Ư': g = {U'u', Mark::Horn}; upper = true; break;
    default: return KeyOutcome::NotApplicable;
  }

  // Nối được như nguyên âm hợp lệ?
  bool ok = false;
  if (cells_.empty()) {
    ok = true;
  } else {
    const Cell& prev = cells_.back();
    switch (prev.form) {
      case Form::V:
      case Form::CV: {
        const VSeqInfo& info = vseq(prev.vs);
        if (info.len < 3) {
          VGlyph seq[3];
          for (int i = 0; i < info.len; ++i) seq[i] = info.g[i];
          seq[info.len] = g;
          const int new_vs = vseq_lookup(seq, info.len + 1);
          ok = new_vs != kNoSeq &&
               (prev.form == Form::V || valid_cv(ops.onset_cs(), new_vs));
        }
        break;
      }
      case Form::C:
        ok = valid_cv(prev.cs, vseq_lookup(&g, 1));
        break;
      default:
        ok = false;
    }
  }

  if (ok) {
    ops.push_vowel(key, g, upper, set_from_w);
    return KeyOutcome::Applied;
  }

  // Gõ lặp ký tự vừa tạo → hủy nó (caller quyết định nối phím thô/ngắt từ).
  if (!cells_.empty() && cells_.back().glyph() == g) {
    ops.remove_last_rescue_tone();
    single_mode_ = false;
    return KeyOutcome::Reverted;
  }
  return KeyOutcome::NotApplicable;
}

// ---- Glue: từ hỏng + không kiểm tra chính tả → bắt đầu từ mới -------------

void Word::maybe_restart_word(const Config& cfg) {
  if (cells_.empty()) return;
  Cell& last = cells_.back();
  if (last.form != Form::NonVn) return;
  if (!glue_class_vn_) return;
  if (cfg.spell_check && !single_mode_) return;

  const int idx = static_cast<int>(cells_.size()) - 1;
  if (last.base != 0 && is_vowel_letter(last.base)) {
    VGlyph g = last.glyph();
    last.form = Form::V;
    last.vs = static_cast<int16_t>(vseq_lookup(&g, 1));
    last.v_end = static_cast<int16_t>(idx);
    last.onset_end = -1;
    last.cs = kNoSeq;
  } else if (last.base != 0) {
    VGlyph g = last.glyph();
    last.form = Form::C;
    last.cs = static_cast<int16_t>(cseq_lookup(&g, 1));
    last.onset_end = static_cast<int16_t>(idx);
    last.v_end = -1;
    last.vs = kNoSeq;
  }
}

// ---- Backspace ------------------------------------------------------------

void Word::backspace(const Config& cfg) {
  if (cells_.empty()) return;

  const int last_idx = static_cast<int>(cells_.size()) - 1;
  const Cell& last = cells_.back();
  const bool plain =
      cells_.size() == 1 || last.form == Form::NonVn || last.form == Form::C ||
      cells_[last_idx - 1].form == Form::C ||
      cells_[last_idx - 1].form == Form::VC ||
      cells_[last_idx - 1].form == Form::CVC;

  if (!plain) {
    // Từ sau khi xóa kết thúc bằng nguyên âm → dấu thanh có thể phải lùi lại.
    // (Ví dụ: hoàn ⌫ → hòa; thanh trên ký tự bị xóa thì mất theo.)
    const int ve = last.v_end;
    const int vs = cells_[ve].vs;
    const int vstart = ve - vseq(vs).len + 1;
    // Đọc vị trí thanh thật trên các ô (bất biến: chỉ một ô mang thanh).
    int cur_pos = -1;
    for (int i = vstart; i <= last_idx; ++i) {
      if (cells_[i].tone != 0) {
        cur_pos = i;
        break;
      }
    }
    if (cur_pos >= 0 && cur_pos != last_idx) {
      const ToneId tone = cells_[cur_pos].tone;
      const int new_vs = cells_[last_idx - 1].vs;
      const int new_pos =
          vstart + tone_offset(new_vs, true,
                               cfg.tone_style == ToneStyle::Modern);
      if (new_pos != cur_pos && new_pos < last_idx) {
        cells_[new_pos].tone = tone;
        cells_[cur_pos].tone = 0;
      }
    }
  }
  cells_.pop_back();
}

// ---- Đánh giá từ (khôi phục phím) -----------------------------------------

// MUTATION-SKIP-BEGIN
// Hàm này chỉ phục vụ test: gieo lỗi vào nó chỉ làm phép kiểm tra yếu đi,
// mà bộ test thì không thể tự phát hiện điều đó.
bool Word::check_invariants() const {
  const int n = static_cast<int>(cells_.size());

  for (int i = 0; i < n; ++i) {
    const Cell& c = cells_[i];

    if (c.v_end < -1 || c.v_end >= n) return false;
    if (c.onset_end < -1 || c.onset_end >= n) return false;

    if (c.tone != 0) {
      if (c.tone > 5) return false;
      if (!is_vowel_letter(c.base)) return false;  // thanh chỉ nằm trên nguyên âm
    }
    if (c.mark == Mark::Stroke && c.base != U'd') return false;
    if (c.base == 0 && (c.mark != Mark::None || c.tone != 0)) return false;

    // cs có thể là kNoSeq: chữ cái không tạo thành chuỗi phụ âm nào (chỉ có
    // 'z') vẫn được nhận vào vị trí phụ âm, đúng như UniKey.
    switch (c.form) {
      case Form::V:
      case Form::CV:
        if (c.vs == kNoSeq || c.v_end != i) return false;
        break;
      case Form::VC:
      case Form::CVC:
        if (c.v_end < 0 || c.v_end >= i) return false;
        break;
      case Form::C:
        if (c.onset_end != i) return false;
        break;
      case Form::NonVn:
        break;
    }
    // Có phụ âm đầu hay không phải khớp với dạng từ.
    const bool has_onset = c.form == Form::CV || c.form == Form::CVC ||
                           c.form == Form::C;
    if (has_onset && c.onset_end < 0) return false;
    if ((c.form == Form::V || c.form == Form::VC) && c.onset_end != -1) {
      return false;
    }
  }

  if (n == 0) return true;

  const Cell& last = cells_.back();
  if (last.v_end < 0) return true;

  // Vần của âm tiết đang gõ phải khớp đúng bảng vần.
  const int ve = last.v_end;
  const int vs = cells_[ve].vs;
  if (vs == kNoSeq) return false;
  const VSeqInfo& info = vseq(vs);
  const int vstart = ve - info.len + 1;
  if (vstart < 0) return false;

  int tones = 0;
  for (int i = vstart; i <= ve; ++i) {
    if (cells_[i].glyph() != info.g[i - vstart]) return false;
    if (cells_[i].tone != 0) ++tones;
  }
  if (tones > 1) return false;                       // một âm tiết một dấu
  for (int i = ve + 1; i < n; ++i) {
    if (cells_[i].tone != 0) return false;           // thanh không ra khỏi vần
  }
  return true;
}
// MUTATION-SKIP-END

bool Word::is_non_vn(const Config& cfg) const {
  (void)cfg;
  if (cells_.empty()) return false;
  const Cell& last = cells_.back();
  switch (last.form) {
    case Form::NonVn:
      return true;
    case Form::C:
      return false;
    case Form::V:
    case Form::CV:
      return !vseq(last.vs).complete;
    case Form::VC:
    case Form::CVC: {
      // Âm cuối không phải chuỗi phụ âm nào (vd 'z') thì chắc chắn không
      // phải tiếng Việt. UniKey bỏ lọt ca này; ta bắt để khôi phục đúng.
      if (last.cs == kNoSeq) return true;
      const int vs = cells_[last.v_end].vs;
      if (!vseq(vs).complete) return true;
      const int c1 = last.onset_end >= 0 ? cells_[last.onset_end].cs : kNoSeq;
      if (!valid_cvc(c1, vs, last.cs)) return true;
      if (is_stop_coda(last.cs)) {
        for (const Cell& c : cells_) {
          if (c.tone == 2 || c.tone == 3 || c.tone == 4) return true;
        }
      }
      return false;
    }
  }
  return false;
}

bool Word::has_vn_mark() const {
  for (const Cell& c : cells_) {
    if (c.tone != 0 || c.mark != Mark::None) return true;
  }
  return false;
}

}  // namespace hodion::detail
