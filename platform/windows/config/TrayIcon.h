// Icon khay hệ thống + menu chuột phải.
//
// Tách khỏi phần còn lại của host vì nó có mấy chỗ dễ sai riêng: phải dựng
// lại icon khi Explorer khởi động lại, và TrackPopupMenu cần đúng nghi thức
// SetForegroundWindow nếu không menu sẽ không tự đóng.
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>

class TrayIcon {
 public:
  // `callback` là message riêng của ứng dụng (WM_APP + n) mà shell sẽ gửi
  // về `owner` khi người dùng bấm vào icon.
  bool Create(HWND owner, UINT callback, HINSTANCE instance);
  void Destroy();

  // Đổi hình + chú thích theo trạng thái. Gọi lại bao nhiêu lần cũng được.
  void SetState(bool vietnamese, const std::wstring& tip);

  // Explorer khởi động lại thì mọi icon khay bị xoá sạch — phải thêm lại.
  bool IsTaskbarCreatedMessage(UINT msg) const {
    return msg == taskbarCreated_ && taskbarCreated_ != 0;
  }
  void Restore();

 private:
  bool Notify(DWORD message);

  HWND owner_ = nullptr;
  UINT callback_ = 0;
  HINSTANCE instance_ = nullptr;
  HICON iconVi_ = nullptr;
  HICON iconEn_ = nullptr;
  UINT taskbarCreated_ = 0;
  bool added_ = false;
  bool vietnamese_ = true;
  std::wstring tip_;
};
