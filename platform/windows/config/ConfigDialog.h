// Hộp thoại cấu hình bộ gõ — mở từ menu khay hệ thống.
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Modal. Trả về IDOK nếu người dùng bấm OK (cấu hình đã được ghi).
INT_PTR HodionShowConfigDialog(HINSTANCE instance, HWND parent);
