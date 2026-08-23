#include "composer.h"

namespace hodion::detail {

namespace {

// 12 nguyên âm (kể cả biến thể dấu phụ) × 6 thanh (không, huyền, sắc, hỏi, ngã, nặng).
constexpr int kToneCount = 6;

constexpr char32_t kLower[12][kToneCount] = {
    {U'a', U'à', U'á', U'ả', U'ã', U'ạ'},
    {U'ă', U'ằ', U'ắ', U'ẳ', U'ẵ', U'ặ'},
    {U'â', U'ầ', U'ấ', U'ẩ', U'ẫ', U'ậ'},
    {U'e', U'è', U'é', U'ẻ', U'ẽ', U'ẹ'},
    {U'ê', U'ề', U'ế', U'ể', U'ễ', U'ệ'},
    {U'i', U'ì', U'í', U'ỉ', U'ĩ', U'ị'},
    {U'o', U'ò', U'ó', U'ỏ', U'õ', U'ọ'},
    {U'ô', U'ồ', U'ố', U'ổ', U'ỗ', U'ộ'},
    {U'ơ', U'ờ', U'ớ', U'ở', U'ỡ', U'ợ'},
    {U'u', U'ù', U'ú', U'ủ', U'ũ', U'ụ'},
    {U'ư', U'ừ', U'ứ', U'ử', U'ữ', U'ự'},
    {U'y', U'ỳ', U'ý', U'ỷ', U'ỹ', U'ỵ'},
};

constexpr char32_t kUpper[12][kToneCount] = {
    {U'A', U'À', U'Á', U'Ả', U'Ã', U'Ạ'},
    {U'Ă', U'Ằ', U'Ắ', U'Ẳ', U'Ẵ', U'Ặ'},
    {U'Â', U'Ầ', U'Ấ', U'Ẩ', U'Ẫ', U'Ậ'},
    {U'E', U'È', U'É', U'Ẻ', U'Ẽ', U'Ẹ'},
    {U'Ê', U'Ề', U'Ế', U'Ể', U'Ễ', U'Ệ'},
    {U'I', U'Ì', U'Í', U'Ỉ', U'Ĩ', U'Ị'},
    {U'O', U'Ò', U'Ó', U'Ỏ', U'Õ', U'Ọ'},
    {U'Ô', U'Ồ', U'Ố', U'Ổ', U'Ỗ', U'Ộ'},
    {U'Ơ', U'Ờ', U'Ớ', U'Ở', U'Ỡ', U'Ợ'},
    {U'U', U'Ù', U'Ú', U'Ủ', U'Ũ', U'Ụ'},
    {U'Ư', U'Ừ', U'Ứ', U'Ử', U'Ữ', U'Ự'},
    {U'Y', U'Ỳ', U'Ý', U'Ỷ', U'Ỹ', U'Ỵ'},
};

// Hàng trong bảng theo (nguyên âm gốc, dấu phụ); -1 nếu tổ hợp không tồn tại.
int row_of(char32_t base, Mark mark) {
  switch (base) {
    case U'a':
      if (mark == Mark::None) return 0;
      if (mark == Mark::Breve) return 1;
      if (mark == Mark::Circumflex) return 2;
      return -1;
    case U'e':
      if (mark == Mark::None) return 3;
      if (mark == Mark::Circumflex) return 4;
      return -1;
    case U'i':
      return mark == Mark::None ? 5 : -1;
    case U'o':
      if (mark == Mark::None) return 6;
      if (mark == Mark::Circumflex) return 7;
      if (mark == Mark::Horn) return 8;
      return -1;
    case U'u':
      if (mark == Mark::None) return 9;
      if (mark == Mark::Horn) return 10;
      return -1;
    case U'y':
      return mark == Mark::None ? 11 : -1;
    default:
      return -1;
  }
}

char32_t ascii_upper(char32_t c) {
  return (c >= U'a' && c <= U'z') ? c - 32 : c;
}

}  // namespace

bool is_vowel_base(char32_t lower) {
  return lower == U'a' || lower == U'e' || lower == U'i' || lower == U'o' ||
         lower == U'u' || lower == U'y';
}

char32_t composed_char(char32_t base, Mark mark, Tone tone, bool upper) {
  if (base == U'd' && mark == Mark::Stroke) return upper ? U'Đ' : U'đ';

  const int row = row_of(base, mark);
  if (row < 0) return upper ? ascii_upper(base) : base;

  const int col = static_cast<int>(tone);
  return upper ? kUpper[row][col] : kLower[row][col];
}

}  // namespace hodion::detail
