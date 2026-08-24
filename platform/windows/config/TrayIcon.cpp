#include "TrayIcon.h"

#include <shellapi.h>

#include "resource.h"

namespace {

// Icon khay phải lấy đúng cỡ hệ thống đang dùng (SM_CXSMICON đổi theo DPI),
// không phải cỡ mặc định — nếu không Windows sẽ tự co giãn và nhòe.
HICON LoadTrayIcon(HINSTANCE instance, int id) {
  const int cx = GetSystemMetrics(SM_CXSMICON);
  const int cy = GetSystemMetrics(SM_CYSMICON);
  return static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(id),
                                       IMAGE_ICON, cx, cy, LR_DEFAULTCOLOR));
}

}  // namespace

bool TrayIcon::Create(HWND owner, UINT callback, HINSTANCE instance) {
  owner_ = owner;
  callback_ = callback;
  instance_ = instance;
  taskbarCreated_ = RegisterWindowMessageW(L"TaskbarCreated");

  iconVi_ = LoadTrayIcon(instance, IDI_TRAY_VI);
  iconEn_ = LoadTrayIcon(instance, IDI_TRAY_EN);
  if (!iconVi_ || !iconEn_) return false;

  return Notify(NIM_ADD);
}

void TrayIcon::Destroy() {
  if (added_) {
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = owner_;
    nid.uID = 1;
    Shell_NotifyIconW(NIM_DELETE, &nid);
    added_ = false;
  }
  if (iconVi_) {
    DestroyIcon(iconVi_);
    iconVi_ = nullptr;
  }
  if (iconEn_) {
    DestroyIcon(iconEn_);
    iconEn_ = nullptr;
  }
}

void TrayIcon::SetState(bool vietnamese, const std::wstring& tip) {
  vietnamese_ = vietnamese;
  tip_ = tip;
  if (added_) Notify(NIM_MODIFY);
}

void TrayIcon::Restore() {
  added_ = false;
  Notify(NIM_ADD);
}

bool TrayIcon::Notify(DWORD message) {
  NOTIFYICONDATAW nid = {};
  nid.cbSize = sizeof(nid);
  nid.hWnd = owner_;
  nid.uID = 1;
  nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
  nid.uCallbackMessage = callback_;
  nid.hIcon = vietnamese_ ? iconVi_ : iconEn_;
  lstrcpynW(nid.szTip, tip_.c_str(), ARRAYSIZE(nid.szTip));

  if (!Shell_NotifyIconW(message, &nid)) return false;
  if (message == NIM_ADD) {
    added_ = true;
    // Bật hành vi phiên bản 4: shell gửi kèm toạ độ chuột trong lParam, nhờ
    // vậy menu hiện đúng chỗ kể cả khi có nhiều màn hình.
    nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &nid);
  }
  return true;
}
