// Nội bộ engine: mô hình âm tiết đang ghép và bộ luật Telex/VNI.
#pragma once

#include <string>
#include <vector>

#include "hodion/engine.h"

namespace hodion::detail {

// Thứ tự trùng với cột trong bảng ký tự (chartable.cpp).
enum class Tone : uint8_t { None = 0, Grave, Acute, Hook, Tilde, Dot };
enum class Mark : uint8_t { None = 0, Breve, Circumflex, Horn, Stroke };

struct VChar {
  char32_t base = 0;      // ký tự gốc, lowercase ('a'..'z' hoặc ký tự literal khác)
  Mark mark = Mark::None;
  bool upper = false;
  bool from_w = false;    // ư sinh từ phím w đơn (Telex) — để ww hủy về 'w'
};

struct ComposeState {
  std::vector<VChar> chars;
  Tone tone = Tone::None;

  bool has_vowel() const;
};

// Áp một phím gõ vào trạng thái (luật của method trong cfg).
void apply_key(ComposeState& st, char32_t key, const Config& cfg);

// Dựng chuỗi hiển thị: đặt dấu thanh đúng vị trí rồi tra bảng ký tự.
std::u32string render(const ComposeState& st, ToneStyle style);

// Bảng ký tự: (chữ gốc, dấu phụ, dấu thanh, hoa/thường) → codepoint.
char32_t composed_char(char32_t base, Mark mark, Tone tone, bool upper);

bool is_vowel_base(char32_t lower);

}  // namespace hodion::detail
