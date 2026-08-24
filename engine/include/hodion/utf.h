// Chuyển đổi UTF nhỏ gọn cho tầng host (mọi ký tự tiếng Việt đều thuộc BMP,
// nhưng các hàm này xử lý đầy đủ cả surrogate để dùng được tổng quát).
#pragma once

#include <string>

namespace hodion::utf {

std::string to_utf8(const std::u32string& s);
std::u16string to_utf16(const std::u32string& s);
std::u32string from_utf8(const std::string& s);
// Tầng host đọc văn bản của tài liệu ở UTF-16 (Windows wchar_t, macOS
// NSString) — đây là đường ngược lại. Cặp thay thế lạc/cụt bị bỏ qua chứ
// không sinh ra codepoint sai.
std::u32string from_utf16(const std::u16string& s);

}  // namespace hodion::utf
