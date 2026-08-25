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

#include <string>

#include "hodion/engine.h"

extern const WCHAR kHodionSettingsKey[];

// Một phím tắt. vk == 0 nghĩa là TẮT hẳn hành động đó.
struct ToggleKey {
  UINT vk = 0;
  // Tổ hợp TF_MOD_ALT | TF_MOD_CONTROL | TF_MOD_SHIFT. Bắt buộc phải có ít
  // nhất một modifier khi phím được bật: phím tắt trần sẽ nuốt mất phím đó
  // của người dùng (đặt Space làm phím chuyển là mất luôn dấu cách).
  UINT mods = 0;

  bool enabled() const { return vk != 0; }
  bool valid() const { return vk != 0 && mods != 0; }
  bool operator==(const ToggleKey& o) const {
    return vk == o.vk && mods == o.mods;
  }
  bool operator!=(const ToggleKey& o) const { return !(*this == o); }
};

// Mặc định của từng phím. Khai TRƯỚC HodionSettings để chính struct đó
// mang sẵn giá trị mặc định — một HodionSettings mới dựng phải LÀ cấu hình
// mặc định, chứ không phải một cái vỏ rỗng chờ ai đó nhớ điền vào.
ToggleKey HodionDefaultToggleKey();
ToggleKey HodionDefaultCancelKey();
ToggleKey HodionDefaultCycleKey();

struct HodionSettings {
  hodion::Config engine;
  bool vietnamese_on = true;  // trạng thái gõ tiếng Việt hiện tại

  // --- Phím tắt. Cái nào cũng tắt được (vk = 0). ---
  ToggleKey toggle = HodionDefaultToggleKey();  // chuyển Việt / Anh
  ToggleKey method_key;  // chuyển Telex / VNI         (mặc định tắt)
  ToggleKey predict_key; // bật/tắt tự thêm dấu        (mặc định tắt)
  // Hủy biến đổi cho RIÊNG từ đang gõ. Khác ba cái trên ở chỗ nó chỉ có
  // hiệu lực khi đang gõ dở, nên nó KHÔNG được đăng ký làm preserved key —
  // đăng ký sẽ chiếm mất tổ hợp đó của ứng dụng kể cả lúc không gõ.
  ToggleKey cancel_key = HodionDefaultCancelKey();  // Ctrl+Backspace
  // Xoay từ ngay trước con trỏ qua các cách viết có dấu. Là preserved key
  // vì nó phải chạy SAU khi từ đã chốt — đó mới là lúc người ta thấy sai.
  ToggleKey cycle_key = HodionDefaultCycleKey();  // Ctrl+Shift+Space
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

// Mô tả một phím tắt cho người đọc ("Ctrl + Space"). Trả về "(tắt)" khi
// phím không được bật.
std::wstring HodionDescribeKey(const ToggleKey& key);

HodionSettings LoadHodionSettings();
bool SaveHodionSettings(const HodionSettings& s);
// Chỉ ghi trạng thái bật/tắt (dùng khi người dùng bấm phím chuyển).
bool SaveHodionVietnameseOn(bool on);
// Ghi đúng một giá trị, dùng khi người dùng bấm phím tắt trong lúc gõ.
// Ghi cả khối sẽ đè lên thay đổi mà app cấu hình vừa lưu.
bool SaveHodionInputMethod(hodion::InputMethod method);
bool SaveHodionAutoDiacritics(bool on);

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
