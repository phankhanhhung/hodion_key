#include "vnlexi.h"

#include <cstring>
#include <vector>

#include "hodion/utf.h"

namespace hodion::detail {

namespace {

// ---- Bảng nguồn (dữ liệu chính tả tiếng Việt, hành vi khớp UniKey) --------

struct VSeqDef {
  const char* seq;
  bool complete;
  bool coda_ok;
  const char* roofed;  // vần khi thêm mũ (nullptr = không áp được)
  const char* hooked;  // vần khi thêm móc/trăng
};

// 69 vần hợp lệ. "Thêm mũ/móc" có thể thay dấu đang có (â+w→ă, uơ+roof→uô…).
// Họ uo (ưo/uơ/ươ và biến thể) có thêm luật riêng trong word.cpp.
const VSeqDef kVSeqDefs[] = {
    {"a", true, true, "â", "ă"},
    {"â", true, true, nullptr, "ă"},
    {"ă", true, true, "â", nullptr},
    {"e", true, true, "ê", nullptr},
    {"ê", true, true, nullptr, nullptr},
    {"i", true, true, nullptr, nullptr},
    {"o", true, true, "ô", "ơ"},
    {"ô", true, true, nullptr, "ơ"},
    {"ơ", true, true, "ô", nullptr},
    {"u", true, true, nullptr, "ư"},
    {"ư", true, true, nullptr, nullptr},
    {"y", true, true, nullptr, nullptr},
    {"ai", true, false, nullptr, nullptr},
    {"ao", true, false, nullptr, nullptr},
    {"au", true, false, "âu", nullptr},
    {"ay", true, false, "ây", nullptr},
    {"âu", true, false, nullptr, nullptr},
    {"ây", true, false, nullptr, nullptr},
    {"eo", true, false, nullptr, nullptr},
    {"eu", false, false, "êu", nullptr},
    {"êu", true, false, nullptr, nullptr},
    {"ia", true, false, nullptr, nullptr},
    {"ie", false, true, "iê", nullptr},
    {"iê", true, true, nullptr, nullptr},
    {"iu", true, false, nullptr, nullptr},
    {"oa", true, true, nullptr, "oă"},
    {"oă", true, true, nullptr, nullptr},
    {"oe", true, true, nullptr, nullptr},
    {"oi", true, false, "ôi", "ơi"},
    {"ôi", true, false, nullptr, "ơi"},
    {"ơi", true, false, "ôi", nullptr},
    {"ua", true, true, "uâ", "ưa"},
    {"uâ", true, true, nullptr, nullptr},
    {"ue", false, true, "uê", nullptr},
    {"uê", true, true, nullptr, nullptr},
    {"ui", true, false, nullptr, "ưi"},
    {"uo", false, true, "uô", "ưo"},
    {"uô", true, true, nullptr, "uơ"},
    {"uơ", true, true, "uô", "ươ"},
    {"uu", false, false, nullptr, "ưu"},
    {"uy", true, true, nullptr, nullptr},
    {"ưa", true, false, nullptr, nullptr},
    {"ưi", true, false, nullptr, nullptr},
    {"ưo", false, true, nullptr, "ươ"},
    {"ươ", true, true, nullptr, nullptr},
    {"ưu", true, false, nullptr, nullptr},
    {"ye", false, true, "yê", nullptr},
    {"yê", true, true, nullptr, nullptr},
    {"ieu", false, false, "iêu", nullptr},
    {"iêu", true, false, nullptr, nullptr},
    {"oai", true, false, nullptr, nullptr},
    {"oay", true, false, nullptr, nullptr},
    {"oeo", true, false, nullptr, nullptr},
    {"uay", false, false, "uây", nullptr},
    {"uây", true, false, nullptr, nullptr},
    {"uoi", false, false, "uôi", "ưoi"},
    {"uou", false, false, nullptr, "ưou"},
    {"uôi", true, false, nullptr, "uơi"},
    {"uơi", false, false, "uôi", "ươi"},
    {"uơu", false, false, nullptr, "ươu"},
    {"uya", true, false, nullptr, nullptr},
    {"uye", false, true, "uyê", nullptr},
    {"uyê", true, true, nullptr, nullptr},
    {"uyu", true, false, nullptr, nullptr},
    {"ưoi", false, false, nullptr, "ươi"},
    {"ưou", false, false, nullptr, "ươu"},
    {"ươi", true, false, nullptr, nullptr},
    {"ươu", true, false, nullptr, nullptr},
    {"yeu", false, false, "yêu", nullptr},
    {"yêu", true, false, nullptr, nullptr},
};
constexpr int kVSeqCount = sizeof(kVSeqDefs) / sizeof(kVSeqDefs[0]);

struct CSeqDef {
  const char* seq;
  bool coda_ok;
};

const CSeqDef kCSeqDefs[] = {
    {"b", false}, {"c", true},   {"ch", true}, {"d", false}, {"đ", false},
    {"dz", false}, {"g", false}, {"gh", false}, {"gi", false}, {"gin", false},
    {"h", false}, {"k", false},  {"kh", false}, {"l", false}, {"m", true},
    {"n", true},  {"ng", true},  {"ngh", false}, {"nh", true}, {"p", true},
    {"ph", false}, {"q", false}, {"qu", false}, {"r", false}, {"s", false},
    {"t", true},  {"th", false}, {"tr", false}, {"v", false}, {"x", false},
};
constexpr int kCSeqCount = sizeof(kCSeqDefs) / sizeof(kCSeqDefs[0]);

// Cặp (vần, âm cuối) hợp lệ.
struct VCDef {
  const char* v;
  const char* codas;  // các âm cuối cách nhau bởi khoảng trắng
};

const VCDef kVCDefs[] = {
    {"a", "c ch m n ng nh p t"},
    {"â", "c m n ng p t"},
    {"ă", "c m n ng p t"},
    {"e", "c ch m n ng nh p t"},
    {"ê", "c ch m n nh p t"},
    {"i", "c ch m n nh p t"},
    {"o", "c m n ng p t"},
    {"ô", "c m n ng p t"},
    {"ơ", "m n p t"},
    {"u", "c m n ng p t"},
    {"ư", "c m n ng t"},
    {"y", "t"},
    {"ie", "c m n ng p t"},
    {"iê", "c m n ng p t"},
    {"oa", "c ch m n ng nh p t"},
    {"oă", "c m n ng t"},
    {"oe", "n t"},
    {"ua", "n ng t"},
    {"uâ", "n ng t"},
    {"ue", "c ch n nh"},
    {"uê", "c ch n nh"},
    {"uo", "c m n ng p t"},
    {"uô", "c m n ng t"},
    {"ưo", "c m n ng p t"},
    {"ươ", "c m n ng p t"},
    {"uy", "c ch n nh p t"},
    {"ye", "m n ng p t"},
    {"yê", "m n ng t"},
    {"uye", "n t"},
    {"uyê", "n t"},
};

// Phụ âm k chỉ đi với các vần dòng trước (e/i/y).
const char* kAfterK[] = {"e",  "ê",  "i",  "y",  "eo",  "eu",
                         "êu", "ia", "ie", "iê", "ieu", "iêu"};

// ---- Bảng đã phân giải ----------------------------------------------------

VSeqInfo g_vseq[kVSeqCount];
CSeqInfo g_cseq[kCSeqCount];
bool g_vc[kVSeqCount][kCSeqCount];
bool g_k_ok[kVSeqCount];

bool g_init = false;

VGlyph glyph_of(char32_t c) {
  switch (c) {
    case U'â': return {U'a', Mark::Circumflex};
    case U'ă': return {U'a', Mark::Breve};
    case U'ê': return {U'e', Mark::Circumflex};
    case U'ô': return {U'o', Mark::Circumflex};
    case U'ơ': return {U'o', Mark::Horn};
    case U'ư': return {U'u', Mark::Horn};
    case U'đ': return {U'd', Mark::Stroke};
    default: return {c, Mark::None};
  }
}

int parse_glyphs(const char* s, VGlyph out[3]) {
  const std::u32string u = hodion::utf::from_utf8(s);
  int n = 0;
  for (char32_t c : u) {
    if (n < 3) out[n++] = glyph_of(c);
  }
  return n;
}

int find_vseq(const VGlyph* g, int len) {
  for (int i = 0; i < kVSeqCount; ++i) {
    if (g_vseq[i].len != len) continue;
    bool eq = true;
    for (int k = 0; k < len; ++k) {
      if (g_vseq[i].g[k] != g[k]) {
        eq = false;
        break;
      }
    }
    if (eq) return i;
  }
  return kNoSeq;
}

int find_cseq(const VGlyph* g, int len) {
  for (int i = 0; i < kCSeqCount; ++i) {
    if (g_cseq[i].len != len) continue;
    bool eq = true;
    for (int k = 0; k < len; ++k) {
      if (g_cseq[i].g[k] != g[k]) {
        eq = false;
        break;
      }
    }
    if (eq) return i;
  }
  return kNoSeq;
}

int find_vseq_str(const char* s) {
  VGlyph g[3];
  const int n = parse_glyphs(s, g);
  return find_vseq(g, n);
}

int find_cseq_str(const char* s) {
  VGlyph g[3];
  const int n = parse_glyphs(s, g);
  return find_cseq(g, n);
}

void init_tables() {
  if (g_init) return;

  for (int i = 0; i < kVSeqCount; ++i) {
    VSeqInfo& v = g_vseq[i];
    v.len = parse_glyphs(kVSeqDefs[i].seq, v.g);
    v.complete = kVSeqDefs[i].complete;
    v.coda_ok = kVSeqDefs[i].coda_ok;
    v.roof_pos = -1;
    v.hook_pos = -1;
    for (int k = 0; k < v.len; ++k) {
      if (v.g[k].mark == Mark::Circumflex && v.roof_pos < 0) v.roof_pos = k;
      if ((v.g[k].mark == Mark::Breve || v.g[k].mark == Mark::Horn) &&
          v.hook_pos < 0) {
        v.hook_pos = k;
      }
    }
  }
  for (int i = 0; i < kCSeqCount; ++i) {
    CSeqInfo& c = g_cseq[i];
    c.len = parse_glyphs(kCSeqDefs[i].seq, c.g);
    c.coda_ok = kCSeqDefs[i].coda_ok;
  }

  // Liên kết withRoof/withHook sau khi mọi vần đã nạp.
  for (int i = 0; i < kVSeqCount; ++i) {
    g_vseq[i].with_roof =
        kVSeqDefs[i].roofed ? find_vseq_str(kVSeqDefs[i].roofed) : kNoSeq;
    g_vseq[i].with_hook =
        kVSeqDefs[i].hooked ? find_vseq_str(kVSeqDefs[i].hooked) : kNoSeq;
  }

  std::memset(g_vc, 0, sizeof(g_vc));
  for (const VCDef& d : kVCDefs) {
    const int vs = find_vseq_str(d.v);
    const char* p = d.codas;
    while (*p) {
      char tok[4] = {};
      int n = 0;
      while (*p && *p != ' ' && n < 3) tok[n++] = *p++;
      while (*p == ' ') ++p;
      const int cs = find_cseq_str(tok);
      if (vs >= 0 && cs >= 0) g_vc[vs][cs] = true;
    }
  }

  std::memset(g_k_ok, 0, sizeof(g_k_ok));
  for (const char* s : kAfterK) {
    const int vs = find_vseq_str(s);
    if (vs >= 0) g_k_ok[vs] = true;
  }

  g_init = true;
}

struct Ids {
  Ids() {
    init_tables();
    oa = find_vseq_str("oa");
    oe = find_vseq_str("oe");
    uy = find_vseq_str("uy");
    c = find_cseq_str("c");
    ch = find_cseq_str("ch");
    p = find_cseq_str("p");
    t = find_cseq_str("t");
    d = find_cseq_str("d");
    dd = find_cseq_str("đ");
    q = find_cseq_str("q");
    g = find_cseq_str("g");
    gi = find_cseq_str("gi");
    gin = find_cseq_str("gin");
    th = find_cseq_str("th");
    qu = find_cseq_str("qu");
    k = find_cseq_str("k");
    n = find_cseq_str("n");
    nh = find_cseq_str("nh");
    ng = find_cseq_str("ng");
    e = find_vseq_str("e");
    er = find_vseq_str("ê");
    y = find_vseq_str("y");
    uo = find_vseq_str("uo");
    uor = find_vseq_str("uô");
    uoh = find_vseq_str("uơ");
    uho = find_vseq_str("ưo");
    uhoh = find_vseq_str("ươ");
    uhoi = find_vseq_str("ưoi");
    uhohi = find_vseq_str("ươi");
  }
  int oa, oe, uy;
  int c, ch, p, t, d, dd, q, g, gi, gin, th, qu, k, n, nh, ng;
  int e, er, y;
  int uo, uor, uoh, uho, uhoh, uhoi, uhohi;
};

const Ids& ids() {
  static Ids v;
  return v;
}

}  // namespace

int kVSeq_oa, kVSeq_oe, kVSeq_uy;
int kVSeq_uo, kVSeq_uor, kVSeq_uoh, kVSeq_uho, kVSeq_uhoh, kVSeq_uhoi,
    kVSeq_uhohi;
int kCSeq_c, kCSeq_ch, kCSeq_p, kCSeq_t, kCSeq_d, kCSeq_dd, kCSeq_q, kCSeq_g,
    kCSeq_gi, kCSeq_gin, kCSeq_th, kCSeq_qu, kCSeq_k, kCSeq_n, kCSeq_nh,
    kCSeq_ng, kCSeq_e_dummy;

namespace {
struct ExportIds {
  ExportIds() {
    const Ids& v = ids();
    kVSeq_oa = v.oa;
    kVSeq_oe = v.oe;
    kVSeq_uy = v.uy;
    kVSeq_uo = v.uo;
    kVSeq_uor = v.uor;
    kVSeq_uoh = v.uoh;
    kVSeq_uho = v.uho;
    kVSeq_uhoh = v.uhoh;
    kVSeq_uhoi = v.uhoi;
    kVSeq_uhohi = v.uhohi;
    kCSeq_c = v.c;
    kCSeq_ch = v.ch;
    kCSeq_p = v.p;
    kCSeq_t = v.t;
    kCSeq_d = v.d;
    kCSeq_dd = v.dd;
    kCSeq_q = v.q;
    kCSeq_g = v.g;
    kCSeq_gi = v.gi;
    kCSeq_gin = v.gin;
    kCSeq_th = v.th;
    kCSeq_qu = v.qu;
    kCSeq_k = v.k;
    kCSeq_n = v.n;
    kCSeq_nh = v.nh;
    kCSeq_ng = v.ng;
  }
} g_exportIds;
}  // namespace

int vseq_lookup(const VGlyph* g, int len) {
  init_tables();
  if (len < 1 || len > 3) return kNoSeq;
  return find_vseq(g, len);
}

const VSeqInfo& vseq(int id) {
  init_tables();
  return g_vseq[id];
}

int cseq_lookup(const VGlyph* g, int len) {
  init_tables();
  if (len < 1 || len > 3) return kNoSeq;
  return find_cseq(g, len);
}

const CSeqInfo& cseq(int id) {
  init_tables();
  return g_cseq[id];
}

bool is_vowel_letter(char32_t lower) {
  return lower == U'a' || lower == U'e' || lower == U'i' || lower == U'o' ||
         lower == U'u' || lower == U'y';
}

bool valid_cv(int cs, int vs) {
  init_tables();
  if (cs == kNoSeq || vs == kNoSeq) return true;

  const VSeqInfo& v = g_vseq[vs];
  // gi không đi với vần bắt đầu bằng i, qu không đi với vần bắt đầu bằng u.
  if ((cs == ids().gi && v.g[0] == VGlyph{U'i', Mark::None}) ||
      (cs == ids().qu && v.g[0] == VGlyph{U'u', Mark::None})) {
    return false;
  }
  if (cs == ids().k) return g_k_ok[vs];
  return true;
}

bool valid_vc(int vs, int cs) {
  init_tables();
  if (vs == kNoSeq || cs == kNoSeq) return true;
  if (!g_vseq[vs].coda_ok) return false;
  if (!g_cseq[cs].coda_ok) return false;
  return g_vc[vs][cs];
}

bool valid_cvc(int c1, int vs, int c2) {
  init_tables();
  if (vs == kNoSeq) return c1 == kNoSeq || c2 != kNoSeq;
  if (c1 == kNoSeq) return valid_vc(vs, c2);
  if (c2 == kNoSeq) return valid_cv(c1, vs);

  const bool ok_cv = valid_cv(c1, vs);
  const bool ok_vc = valid_vc(vs, c2);
  if (ok_cv && ok_vc) return true;

  if (!ok_vc) {
    // Ngoại lệ: quyn/quynh, gien(g)/giêng hợp lệ dù VC đơn lẻ không hợp lệ.
    if (c1 == ids().qu && vs == ids().y &&
        (c2 == ids().n || c2 == ids().nh)) {
      return true;
    }
    if (c1 == ids().gi && (vs == ids().e || vs == ids().er) &&
        (c2 == ids().n || c2 == ids().ng)) {
      return true;
    }
  }
  return false;
}

int tone_offset(int vs, bool terminated, bool modern_style) {
  init_tables();
  const VSeqInfo& info = g_vseq[vs];
  if (info.len == 1) return 0;

  if (info.roof_pos >= 0) return info.roof_pos;
  if (info.hook_pos >= 0) {
    // ươ / ươi / ươu: thanh nằm trên ơ (vị trí 1).
    if (info.g[0] == VGlyph{U'u', Mark::Horn} &&
        info.g[1] == VGlyph{U'o', Mark::Horn}) {
      return 1;
    }
    return info.hook_pos;
  }

  if (info.len == 3) return 1;

  if (modern_style && (vs == ids().oa || vs == ids().oe || vs == ids().uy)) {
    return 1;
  }

  return terminated ? 0 : 1;
}

}  // namespace hodion::detail
