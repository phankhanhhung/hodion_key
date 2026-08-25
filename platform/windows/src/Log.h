// Nhật ký chẩn đoán, TẮT theo mặc định.
//
// Bộ gõ nằm trong tiến trình của ứng dụng khác, không có cửa sổ, không có
// console. Khi nó cư xử sai trong một ứng dụng cụ thể thì người dùng chỉ
// nói được "không thấy gì" — mà "không thấy gì" là triệu chứng chung của
// hàng chục nguyên nhân khác nhau. Bật cái này lên là đọc được đúng chỗ
// nào trong chuỗi xử lý đã dừng lại.
//
// Bật:  reg add HKCU\Software\HodionKey /v Debug /t REG_DWORD /d 1 /f
// Tắt:  reg add HKCU\Software\HodionKey /v Debug /t REG_DWORD /d 0 /f
// File: %LOCALAPPDATA%\HodionKey\hodionkey.log
//
// Tắt thì mỗi lời gọi chỉ tốn một phép so sánh bool, nên để nguyên lời gọi
// trên đường gõ cũng không sao.
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Đọc lại cờ từ registry. Gọi lúc kích hoạt và mỗi khi cấu hình đổi.
void HodionLogRefresh();

bool HodionLogEnabled();

// Ghi một dòng (tự thêm giờ, pid, tên tiến trình). Không làm gì khi tắt.
void HodionLogWrite(const WCHAR* format, ...);

#define HODION_LOG(...)                     \
  do {                                      \
    if (HodionLogEnabled()) {               \
      HodionLogWrite(__VA_ARGS__);          \
    }                                       \
  } while (0)
