// Cấu hình người dùng lưu ở HKCU\Software\HodionKey — dùng chung cho text
// service (đọc) và app cấu hình (đọc/ghi).
//
// Text service theo dõi khoá registry này bằng RegNotifyChangeKeyValue nên
// đổi cấu hình có hiệu lực ngay, không cần khởi động lại ứng dụng. Trạng
// thái bật/tắt tiếng Việt cũng nằm ở đây, nhờ vậy mọi tiến trình đang gõ
// đều chuyển theo nhau (giống cảm giác dùng UniKey).
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <msctf.h>

#include "hodion/engine.h"

extern const WCHAR kHodionSettingsKey[];

struct ToggleKey {
  UINT vk = VK_SPACE;
  // Tổ hợp TF_MOD_ALT | TF_MOD_CONTROL | TF_MOD_SHIFT. Bắt buộc phải có ít
  // nhất một modifier: phím chuyển trần sẽ nuốt phím đó của người dùng
  // (đặt Space làm phím chuyển là mất luôn dấu cách).
  UINT mods = TF_MOD_CONTROL;

  bool valid() const { return vk != 0 && mods != 0; }
  bool operator==(const ToggleKey& o) const {
    return vk == o.vk && mods == o.mods;
  }
};

struct HodionSettings {
  hodion::Config engine;
  bool vietnamese_on = true;  // trạng thái gõ tiếng Việt hiện tại
  ToggleKey toggle;           // phím chuyển Việt/Anh
  // Tự tắt tiếng Việt ở ô mà ứng dụng khai là URL/email/mật khẩu/số
  // (hỏi qua ITfInputScope — xem InputScope.cpp).
  bool skip_input_scopes = true;
  // Tự thêm dấu cho chữ không dấu lúc chốt từ, hỏi qua tiến trình nền.
  // Mặc định TẮT: nó đổi thứ người dùng vừa gõ, nên phải là lựa chọn
  // tường minh chứ không phải mặc định.
  bool auto_diacritics = false;
  // Ngưỡng tin cậy của mô hình, tính theo phần mười (20 = 2,0). Cao hơn =
  // ít đổi hơn nhưng đúng hơn. Với bộ gõ thì đổi SAI tệ hơn không đổi: chữ
  // còn không dấu thì nhìn thấy ngay, chữ sai dấu thì trông như đã xong.
  unsigned predict_margin = 20;
};

// Danh sách phím chuyển dựng sẵn cho app cấu hình (phần tử 0 là mặc định).
struct TogglePreset {
  ToggleKey key;
  const WCHAR* label;
};
const TogglePreset* HodionTogglePresets(int* count);

HodionSettings LoadHodionSettings();
bool SaveHodionSettings(const HodionSettings& s);
// Chỉ ghi trạng thái bật/tắt (dùng khi người dùng bấm phím chuyển).
bool SaveHodionVietnameseOn(bool on);

// Khởi động cùng Windows: mục trong HKCU\...\CurrentVersion\Run, trỏ tới
// chính exe host kèm --tray để nó nằm im ở khay thay vì mở hộp thoại.
bool HodionGetAutoStart();
bool HodionSetAutoStart(bool on);

// Theo dõi thay đổi cấu hình mà không cần đọc registry mỗi lần gõ phím.
class SettingsWatcher {
 public:
  ~SettingsWatcher() { stop(); }

  bool start();
  // true nếu có thay đổi kể từ lần gọi trước (tự đăng ký lại lượt theo dõi).
  bool poll();
  void stop();

  // Sự kiện báo registry đổi, để vòng lặp thông điệp chờ chung với thông
  // điệp cửa sổ (MsgWaitForMultipleObjects) thay vì hỏi theo nhịp. Không
  // được đóng handle này.
  HANDLE event() const { return event_; }

 private:
  bool arm();

  HKEY key_ = nullptr;
  HANDLE event_ = nullptr;
};
