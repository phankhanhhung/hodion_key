#include "hodion/utf.h"

namespace hodion::utf {

std::string to_utf8(const std::u32string& s) {
  std::string out;
  out.reserve(s.size() * 2);
  for (char32_t c : s) {
    if (c < 0x80) {
      out.push_back(static_cast<char>(c));
    } else if (c < 0x800) {
      out.push_back(static_cast<char>(0xC0 | (c >> 6)));
      out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
    } else if (c < 0x10000) {
      out.push_back(static_cast<char>(0xE0 | (c >> 12)));
      out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
    } else {
      out.push_back(static_cast<char>(0xF0 | (c >> 18)));
      out.push_back(static_cast<char>(0x80 | ((c >> 12) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
    }
  }
  return out;
}

std::u16string to_utf16(const std::u32string& s) {
  std::u16string out;
  out.reserve(s.size());
  for (char32_t c : s) {
    if (c < 0x10000) {
      out.push_back(static_cast<char16_t>(c));
    } else {
      const char32_t v = c - 0x10000;
      out.push_back(static_cast<char16_t>(0xD800 | (v >> 10)));
      out.push_back(static_cast<char16_t>(0xDC00 | (v & 0x3FF)));
    }
  }
  return out;
}

std::u32string from_utf8(const std::string& s) {
  std::u32string out;
  size_t i = 0;
  while (i < s.size()) {
    const unsigned char b = static_cast<unsigned char>(s[i]);
    char32_t c = 0;
    size_t len = 1;
    if (b < 0x80) {
      c = b;
    } else if ((b >> 5) == 0x6) {
      c = b & 0x1F;
      len = 2;
    } else if ((b >> 4) == 0xE) {
      c = b & 0x0F;
      len = 3;
    } else if ((b >> 3) == 0x1E) {
      c = b & 0x07;
      len = 4;
    } else {
      ++i;  // byte hỏng — bỏ qua
      continue;
    }
    if (i + len > s.size()) break;
    bool ok = true;
    for (size_t k = 1; k < len; ++k) {
      const unsigned char cb = static_cast<unsigned char>(s[i + k]);
      if ((cb >> 6) != 0x2) {
        ok = false;
        break;
      }
      c = (c << 6) | (cb & 0x3F);
    }
    if (ok) out.push_back(c);
    i += len;
  }
  return out;
}

}  // namespace hodion::utf
