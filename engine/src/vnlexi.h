// Tầng từ vựng âm tiết tiếng Việt: bảng vần (cụm nguyên âm), phụ âm đầu,
// âm cuối và các luật kết hợp hợp lệ. Đây là dữ liệu ngôn ngữ học thuần —
// hành vi tương thích UniKey (tham khảo tài liệu ukengine, cài đặt lại).
#pragma once

#include <cstdint>

namespace hodion::detail {

enum class Mark : uint8_t { None = 0, Breve, Circumflex, Horn, Stroke };

// Thanh điệu: 0 = không, 1 = sắc, 2 = huyền, 3 = hỏi, 4 = ngã, 5 = nặng.
using ToneId = uint8_t;

// Một nguyên âm trong vần: chữ gốc + dấu phụ (vd ơ = {o, Horn}).
struct VGlyph {
  char32_t base = 0;
  Mark mark = Mark::None;

  bool operator==(const VGlyph& o) const {
    return base == o.base && mark == o.mark;
  }
  bool operator!=(const VGlyph& o) const { return !(*this == o); }
};

constexpr int kNoSeq = -1;

// Thông tin một vần hợp lệ.
struct VSeqInfo {
  int len = 0;
  VGlyph g[3];
  bool complete = false;   // vần trọn vẹn (từ có thể kết thúc hợp lệ)
  bool coda_ok = false;    // cho phép âm cuối theo sau
  int roof_pos = -1;       // vị trí nguyên âm đang mang mũ (â/ê/ô), -1 nếu không
  int with_roof = kNoSeq;  // vần sau khi thêm mũ, kNoSeq nếu không áp được
  int hook_pos = -1;       // vị trí nguyên âm đầu tiên mang móc/trăng (ă/ơ/ư)
  int with_hook = kNoSeq;  // vần sau khi thêm móc/trăng
};

// Thông tin một chuỗi phụ âm hợp lệ (đầu hoặc cuối).
struct CSeqInfo {
  int len = 0;
  VGlyph g[3];             // mark chỉ dùng cho đ (d + Stroke)
  bool coda_ok = false;    // được phép làm âm cuối
};

// Tra cứu vần theo chuỗi glyph; trả về kNoSeq nếu không hợp lệ.
int vseq_lookup(const VGlyph* g, int len);
const VSeqInfo& vseq(int id);

// Tra cứu chuỗi phụ âm; trả về kNoSeq nếu không hợp lệ.
int cseq_lookup(const VGlyph* g, int len);
const CSeqInfo& cseq(int id);

// Âm cuối tắc (c, ch, p, t): chỉ nhận sắc và nặng.
bool is_stop_coda(int cs);

// Một số id dùng trực tiếp trong luật.
extern int kVSeq_oa, kVSeq_oe, kVSeq_uy;
extern int kCSeq_c, kCSeq_ch, kCSeq_p, kCSeq_t;
extern int kCSeq_d, kCSeq_dd, kCSeq_q, kCSeq_g, kCSeq_gi, kCSeq_gin, kCSeq_th,
    kCSeq_qu, kCSeq_k, kCSeq_n, kCSeq_nh, kCSeq_ng, kCSeq_e_dummy;

// Luật kết hợp (mirror hành vi UniKey):
bool valid_cv(int cs, int vs);              // phụ âm đầu + vần
bool valid_vc(int vs, int cs);              // vần + âm cuối
bool valid_cvc(int c1, int vs, int c2);     // cả âm tiết (kèm ngoại lệ quyn(h), gien(g))

bool is_vowel_letter(char32_t lower);       // a e i o u y

// f, j, w không thuộc bảng chữ tiếng Việt — chúng là ký tự "ngoại lai"
// (không mở/nối âm tiết hợp lệ), giống phân loại của UniKey. Lưu ý `z`
// KHÔNG bị loại ở đây, đúng như UniKey phân loại khi gõ.
bool is_vn_letter(char32_t lower);

// Vị trí nhận dấu thanh trong vần (offset trong vần).
// terminated = vần nằm ở cuối từ (chưa có âm cuối).
int tone_offset(int vs, bool terminated, bool modern_style);

// Bảng ký tự: (chữ gốc, dấu phụ, thanh, hoa/thường) → codepoint dựng sẵn.
char32_t composed_char(char32_t base, Mark mark, ToneId tone, bool upper);

// Chiều ngược lại: tách một ký tự dựng sẵn (hoặc chữ cái ASCII) thành các
// thành phần. Trả về false nếu ký tự không phải chữ cái nào — khi đó các
// tham số ra không bị đụng tới.
bool decompose_char(char32_t c, char32_t* base, Mark* mark, ToneId* tone,
                    bool* upper);

// Id vần dùng cho luật họ uo.
extern int kVSeq_uo, kVSeq_uor, kVSeq_uoh, kVSeq_uho, kVSeq_uhoh, kVSeq_uhoi,
    kVSeq_uhohi;

}  // namespace hodion::detail
