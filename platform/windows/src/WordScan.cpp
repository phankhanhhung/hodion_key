#include "WordScan.h"

#include "hodion/reconvert.h"

bool HodionIsWordChar(wchar_t c) {
  if ((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z')) return true;
  if (c < 0x80) return false;
  // Chữ tiếng Việt dựng sẵn thì bỏ dấu ra sẽ khác chính nó — kể cả đ (→ d).
  const std::u32string one(1, static_cast<char32_t>(c));
  return hodion::strip_diacritics(one) != one;
}

void HodionWordAround(const std::wstring& before, const std::wstring& after,
                      size_t* start, size_t* end) {
  size_t s = before.size();
  while (s > 0 && HodionIsWordChar(before[s - 1])) --s;
  size_t e = 0;
  while (e < after.size() && HodionIsWordChar(after[e])) ++e;
  *start = s;
  *end = e;
}
