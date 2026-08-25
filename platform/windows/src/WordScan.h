// Dò ranh giới từ quanh con trỏ.
//
// Tách khỏi Reconversion.cpp để test được: phần còn lại của reconversion là
// số học trên ITfRange, chỉ chạy được trong một ứng dụng TSF thật, còn đoạn
// này là logic thuần và là chỗ dễ sai nhất (lấy nhầm ranh giới là sửa nhầm
// chữ của người ta).
#pragma once

#include <string>

// Ký tự có thuộc về một từ tiếng Việt không? Chữ cái ASCII và mọi chữ tiếng
// Việt dựng sẵn (kể cả đ) thì có; dấu cách, dấu câu, chữ số thì không.
bool HodionIsWordChar(wchar_t c);

// Cho `before` (văn bản ngay TRƯỚC con trỏ) và `after` (ngay SAU), trả về
// từ mà con trỏ đang đứng trong đó:
//   *start = chỉ số bắt đầu trong `before` (before.size() nghĩa là không
//            lấy ký tự nào bên trái)
//   *end   = số ký tự lấy từ đầu `after`
// Con trỏ không nằm cạnh chữ nào thì *start == before.size() và *end == 0.
void HodionWordAround(const std::wstring& before, const std::wstring& after,
                      size_t* start, size_t* end);
