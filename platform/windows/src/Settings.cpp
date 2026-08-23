// Đọc cấu hình người dùng từ registry:
//   HKCU\Software\HodionKey
//     InputMethod  (DWORD) 0 = Telex (mặc định), 1 = VNI
//     ToneStyle    (DWORD) 0 = kiểu cũ — hòa (mặc định), 1 = kiểu mới — hoà
//     WShorthand   (DWORD) 1 = phím w đơn thành ư (mặc định)
//     DelayedD     (DWORD) 1 = phím d cuối từ thành đ (mặc định)
#include "TextService.h"

namespace {

constexpr WCHAR kSettingsKey[] = L"Software\\HodionKey";

DWORD ReadDword(HKEY key, const WCHAR* name, DWORD fallback) {
  DWORD value = 0;
  DWORD size = sizeof(value);
  DWORD type = 0;
  if (RegQueryValueExW(key, name, nullptr, &type,
                       reinterpret_cast<BYTE*>(&value),
                       &size) == ERROR_SUCCESS &&
      type == REG_DWORD) {
    return value;
  }
  return fallback;
}

}  // namespace

void CTextService::LoadSettings() {
  hodion::Config cfg;

  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, kSettingsKey, 0, KEY_QUERY_VALUE,
                    &key) == ERROR_SUCCESS) {
    cfg.method = ReadDword(key, L"InputMethod", 0) == 1
                     ? hodion::InputMethod::Vni
                     : hodion::InputMethod::Telex;
    cfg.tone_style = ReadDword(key, L"ToneStyle", 0) == 1
                         ? hodion::ToneStyle::Modern
                         : hodion::ToneStyle::Traditional;
    cfg.w_shorthand = ReadDword(key, L"WShorthand", 1) != 0;
    cfg.delayed_d = ReadDword(key, L"DelayedD", 1) != 0;
    RegCloseKey(key);
  }

  engine_.set_config(cfg);
}
