// Đọc cấu hình người dùng từ registry (mặc định theo UniKey):
//   HKCU\Software\HodionKey
//     InputMethod    (DWORD) 0 = Telex (mặc định), 1 = VNI
//     ToneStyle      (DWORD) 0 = kiểu cũ — hòa (mặc định), 1 = kiểu mới — hoà
//     FreeMarking    (DWORD) 1 = gõ dấu tự do, dấu ở cuối từ (mặc định 1)
//     SpellCheck     (DWORD) 1 = kiểm tra chính tả âm tiết (mặc định 1)
//     RestoreNonVn   (DWORD) 1 = tự khôi phục phím với từ không phải TV (mặc định 0)
//     WShorthand     (DWORD) 1 = Telex: w đơn → ư (mặc định 1)
//     TelexBrackets  (DWORD) 1 = Telex: [ ] { } → ơ ư Ơ Ư (mặc định 1)
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
    cfg.free_marking = ReadDword(key, L"FreeMarking", 1) != 0;
    cfg.spell_check = ReadDword(key, L"SpellCheck", 1) != 0;
    cfg.restore_non_vn = ReadDword(key, L"RestoreNonVn", 0) != 0;
    cfg.w_shorthand = ReadDword(key, L"WShorthand", 1) != 0;
    cfg.telex_brackets = ReadDword(key, L"TelexBrackets", 1) != 0;
    RegCloseKey(key);
  }

  engine_.set_config(cfg);
}
