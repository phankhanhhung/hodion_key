#include "vnlexi.h"

namespace hodion::detail {

namespace {

// Cột theo mã thanh: 0 = không, 1 = sắc, 2 = huyền, 3 = hỏi, 4 = ngã, 5 = nặng.
constexpr int kToneCount = 6;

constexpr char32_t kLower[12][kToneCount] = {
    {U'a', U'á', U'à', U'ả', U'ã', U'ạ'},
    {U'ă', U'ắ', U'ằ', U'ẳ', U'ẵ', U'ặ'},
    {U'â', U'ấ', U'ầ', U'ẩ', U'ẫ', U'ậ'},
    {U'e', U'é', U'è', U'ẻ', U'ẽ', U'ẹ'},
    {U'ê', U'ế', U'ề', U'ể', U'ễ', U'ệ'},
    {U'i', U'í', U'ì', U'ỉ', U'ĩ', U'ị'},
    {U'o', U'ó', U'ò', U'ỏ', U'õ', U'ọ'},
    {U'ô', U'ố', U'ồ', U'ổ', U'ỗ', U'ộ'},
    {U'ơ', U'ớ', U'ờ', U'ở', U'ỡ', U'ợ'},
    {U'u', U'ú', U'ù', U'ủ', U'ũ', U'ụ'},
    {U'ư', U'ứ', U'ừ', U'ử', U'ữ', U'ự'},
    {U'y', U'ý', U'ỳ', U'ỷ', U'ỹ', U'ỵ'},
};

constexpr char32_t kUpper[12][kToneCount] = {
    {U'A', U'Á', U'À', U'Ả', U'Ã', U'Ạ'},
    {U'Ă', U'Ắ', U'Ằ', U'Ẳ', U'Ẵ', U'Ặ'},
    {U'Â', U'Ấ', U'Ầ', U'Ẩ', U'Ẫ', U'Ậ'},
    {U'E', U'É', U'È', U'Ẻ', U'Ẽ', U'Ẹ'},
    {U'Ê', U'Ế', U'Ề', U'Ể', U'Ễ', U'Ệ'},
    {U'I', U'Í', U'Ì', U'Ỉ', U'Ĩ', U'Ị'},
    {U'O', U'Ó', U'Ò', U'Ỏ', U'Õ', U'Ọ'},
    {U'Ô', U'Ố', U'Ồ', U'Ổ', U'Ỗ', U'Ộ'},
    {U'Ơ', U'Ớ', U'Ờ', U'Ở', U'Ỡ', U'Ợ'},
    {U'U', U'Ú', U'Ù', U'Ủ', U'Ũ', U'Ụ'},
    {U'Ư', U'Ứ', U'Ừ', U'Ử', U'Ữ', U'Ự'},
    {U'Y', U'Ý', U'Ỳ', U'Ỷ', U'Ỹ', U'Ỵ'},
};

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

char32_t composed_char(char32_t base, Mark mark, ToneId tone, bool upper) {
  if (base == U'd' && mark == Mark::Stroke) return upper ? U'Đ' : U'đ';

  const int row = row_of(base, mark);
  if (row < 0) return upper ? ascii_upper(base) : base;

  const int col = tone <= 5 ? tone : 0;
  return upper ? kUpper[row][col] : kLower[row][col];
}

}  // namespace hodion::detail
