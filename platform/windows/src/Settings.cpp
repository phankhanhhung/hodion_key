#include "Settings.h"

#include <msctf.h>

#include <string>

const WCHAR kHodionSettingsKey[] = L"Software\\HodionKey";

namespace {

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

bool WriteDword(HKEY key, const WCHAR* name, DWORD value) {
  return RegSetValueExW(key, name, 0, REG_DWORD,
                        reinterpret_cast<const BYTE*>(&value),
                        sizeof(value)) == ERROR_SUCCESS;
}

// Đọc một phím tắt. Quy ước: vk == 0 là TẮT (hợp lệ). Có vk mà thiếu
// modifier thì hỏng — phím trần sẽ nuốt mất phím đó của người dùng — nên
// quay về mặc định của hành động đó.
ToggleKey ReadKey(HKEY key, const WCHAR* vkName, const WCHAR* modsName,
                  const ToggleKey& fallback) {
  ToggleKey out;
  out.vk = ReadDword(key, vkName, fallback.vk);
  out.mods = ReadDword(key, modsName, fallback.mods);
  if (!out.enabled()) return ToggleKey{};  // tắt hẳn, đúng ý người dùng
  if (!out.valid()) return fallback;
  return out;
}

bool WriteKey(HKEY key, const WCHAR* vkName, const WCHAR* modsName,
              const ToggleKey& value) {
  bool ok = WriteDword(key, vkName, value.vk);
  ok &= WriteDword(key, modsName, value.mods);
  return ok;
}

}  // namespace

ToggleKey HodionDefaultToggleKey() { return ToggleKey{VK_SPACE, TF_MOD_CONTROL}; }
ToggleKey HodionDefaultCancelKey() { return ToggleKey{VK_BACK, TF_MOD_CONTROL}; }
ToggleKey HodionDefaultCycleKey() {
  return ToggleKey{VK_SPACE, TF_MOD_CONTROL | TF_MOD_SHIFT};
}

std::wstring HodionDescribeKey(const ToggleKey& key) {
  if (!key.enabled()) return L"(tắt)";

  std::wstring text;
  if (key.mods & TF_MOD_CONTROL) text += L"Ctrl + ";
  if (key.mods & TF_MOD_ALT) text += L"Alt + ";
  if (key.mods & TF_MOD_SHIFT) text += L"Shift + ";

  WCHAR name[64] = {};
  const UINT sc = MapVirtualKeyW(key.vk, MAPVK_VK_TO_VSC);
  if (sc != 0 && GetKeyNameTextW(static_cast<LONG>(sc) << 16, name,
                                 ARRAYSIZE(name)) > 0) {
    text += name;
  } else {
    WCHAR fallback[16] = {};
    wsprintfW(fallback, L"VK %u", key.vk);
    text += fallback;
  }
  return text;
}

HodionSettings LoadHodionSettings() {
  HodionSettings s;
  s.toggle = HodionDefaultToggleKey();
  s.cancel_key = HodionDefaultCancelKey();
  s.cycle_key = HodionDefaultCycleKey();

  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, kHodionSettingsKey, 0, KEY_QUERY_VALUE,
                    &key) != ERROR_SUCCESS) {
    return s;
  }

  s.engine.method = ReadDword(key, L"InputMethod", 0) == 1
                        ? hodion::InputMethod::Vni
                        : hodion::InputMethod::Telex;
  s.engine.tone_style = ReadDword(key, L"ToneStyle", 0) == 1
                            ? hodion::ToneStyle::Modern
                            : hodion::ToneStyle::Traditional;
  s.engine.free_marking = ReadDword(key, L"FreeMarking", 1) != 0;
  s.engine.spell_check = ReadDword(key, L"SpellCheck", 1) != 0;
  s.engine.restore_non_vn = ReadDword(key, L"RestoreNonVn", 0) != 0;
  s.engine.w_shorthand = ReadDword(key, L"WShorthand", 1) != 0;
  s.engine.telex_brackets = ReadDword(key, L"TelexBrackets", 1) != 0;
  s.engine.english_detect = ReadDword(key, L"EnglishDetect", 1) != 0;

  s.vietnamese_on = ReadDword(key, L"VietnameseOn", 1) != 0;
  s.skip_input_scopes = ReadDword(key, L"SkipInputScopes", 1) != 0;
  s.auto_diacritics = ReadDword(key, L"AutoDiacritics", 0) != 0;
  s.predict_margin = ReadDword(key, L"PredictMargin", 20);
  if (s.predict_margin > 200) s.predict_margin = 20;  // giá trị hỏng
  s.toggle = ReadKey(key, L"ToggleKey", L"ToggleMods",
                     HodionDefaultToggleKey());
  s.method_key = ReadKey(key, L"MethodKey", L"MethodMods", ToggleKey{});
  s.predict_key = ReadKey(key, L"PredictKey", L"PredictMods", ToggleKey{});
  s.cancel_key = ReadKey(key, L"CancelKey", L"CancelMods",
                         HodionDefaultCancelKey());
  s.cycle_key = ReadKey(key, L"CycleKey", L"CycleMods",
                        HodionDefaultCycleKey());

  RegCloseKey(key);
  return s;
}

bool SaveHodionSettings(const HodionSettings& s) {
  HKEY key = nullptr;
  if (RegCreateKeyExW(HKEY_CURRENT_USER, kHodionSettingsKey, 0, nullptr,
                      REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &key,
                      nullptr) != ERROR_SUCCESS) {
    return false;
  }

  bool ok = true;
  ok &= WriteDword(key, L"InputMethod",
                   s.engine.method == hodion::InputMethod::Vni ? 1 : 0);
  ok &= WriteDword(key, L"ToneStyle",
                   s.engine.tone_style == hodion::ToneStyle::Modern ? 1 : 0);
  ok &= WriteDword(key, L"FreeMarking", s.engine.free_marking ? 1 : 0);
  ok &= WriteDword(key, L"SpellCheck", s.engine.spell_check ? 1 : 0);
  ok &= WriteDword(key, L"RestoreNonVn", s.engine.restore_non_vn ? 1 : 0);
  ok &= WriteDword(key, L"WShorthand", s.engine.w_shorthand ? 1 : 0);
  ok &= WriteDword(key, L"TelexBrackets", s.engine.telex_brackets ? 1 : 0);
  ok &= WriteDword(key, L"EnglishDetect", s.engine.english_detect ? 1 : 0);
  ok &= WriteDword(key, L"VietnameseOn", s.vietnamese_on ? 1 : 0);
  ok &= WriteDword(key, L"SkipInputScopes", s.skip_input_scopes ? 1 : 0);
  ok &= WriteDword(key, L"AutoDiacritics", s.auto_diacritics ? 1 : 0);
  ok &= WriteDword(key, L"PredictMargin", s.predict_margin);
  ok &= WriteKey(key, L"ToggleKey", L"ToggleMods", s.toggle);
  ok &= WriteKey(key, L"MethodKey", L"MethodMods", s.method_key);
  ok &= WriteKey(key, L"PredictKey", L"PredictMods", s.predict_key);
  ok &= WriteKey(key, L"CancelKey", L"CancelMods", s.cancel_key);
  ok &= WriteKey(key, L"CycleKey", L"CycleMods", s.cycle_key);

  RegCloseKey(key);
  return ok;
}

namespace {

bool SaveOneDword(const WCHAR* name, DWORD value) {
  HKEY key = nullptr;
  if (RegCreateKeyExW(HKEY_CURRENT_USER, kHodionSettingsKey, 0, nullptr,
                      REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &key,
                      nullptr) != ERROR_SUCCESS) {
    return false;
  }
  const bool ok = WriteDword(key, name, value);
  RegCloseKey(key);
  return ok;
}

}  // namespace

bool SaveHodionVietnameseOn(bool on) {
  return SaveOneDword(L"VietnameseOn", on ? 1 : 0);
}

bool SaveHodionInputMethod(hodion::InputMethod method) {
  return SaveOneDword(L"InputMethod",
                      method == hodion::InputMethod::Vni ? 1 : 0);
}

bool SaveHodionAutoDiacritics(bool on) {
  return SaveOneDword(L"AutoDiacritics", on ? 1 : 0);
}

// ---------------------------------------------------------------------------

namespace {

constexpr WCHAR kRunKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr WCHAR kRunValue[] = L"HodionKey";

}  // namespace

bool HodionGetAutoStart() {
  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_QUERY_VALUE, &key) !=
      ERROR_SUCCESS) {
    return false;
  }
  const bool present =
      RegQueryValueExW(key, kRunValue, nullptr, nullptr, nullptr, nullptr) ==
      ERROR_SUCCESS;
  RegCloseKey(key);
  return present;
}

bool HodionSetAutoStart(bool on) {
  HKEY key = nullptr;
  if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr,
                      REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &key,
                      nullptr) != ERROR_SUCCESS) {
    return false;
  }

  bool ok;
  if (!on) {
    const LSTATUS st = RegDeleteValueW(key, kRunValue);
    ok = st == ERROR_SUCCESS || st == ERROR_FILE_NOT_FOUND;
  } else {
    WCHAR path[MAX_PATH] = {};
    const DWORD len = GetModuleFileNameW(nullptr, path, ARRAYSIZE(path));
    if (len == 0 || len >= ARRAYSIZE(path)) {
      RegCloseKey(key);
      return false;
    }
    // Ngoặc kép vì đường dẫn hầu như luôn có dấu cách (Program Files).
    std::wstring command = L"\"";
    command += path;
    command += L"\" --tray";
    ok = RegSetValueExW(
             key, kRunValue, 0, REG_SZ,
             reinterpret_cast<const BYTE*>(command.c_str()),
             static_cast<DWORD>((command.size() + 1) * sizeof(WCHAR))) ==
         ERROR_SUCCESS;
  }
  RegCloseKey(key);
  return ok;
}

bool SettingsWatcher::start() {
  stop();
  if (RegCreateKeyExW(HKEY_CURRENT_USER, kHodionSettingsKey, 0, nullptr,
                      REG_OPTION_NON_VOLATILE, KEY_NOTIFY | KEY_QUERY_VALUE,
                      nullptr, &key_, nullptr) != ERROR_SUCCESS) {
    key_ = nullptr;
    return false;
  }
  event_ = CreateEventW(nullptr, /*manual reset=*/TRUE, FALSE, nullptr);
  if (!event_) {
    stop();
    return false;
  }
  return arm();
}

bool SettingsWatcher::arm() {
  if (!key_ || !event_) return false;
  ResetEvent(event_);
  return RegNotifyChangeKeyValue(key_, /*watch subtree=*/FALSE,
                                 REG_NOTIFY_CHANGE_LAST_SET, event_,
                                 /*async=*/TRUE) == ERROR_SUCCESS;
}

bool SettingsWatcher::poll() {
  if (!event_) return false;
  if (WaitForSingleObject(event_, 0) != WAIT_OBJECT_0) return false;
  arm();
  return true;
}

void SettingsWatcher::stop() {
  if (event_) {
    CloseHandle(event_);
    event_ = nullptr;
  }
  if (key_) {
    RegCloseKey(key_);
    key_ = nullptr;
  }
}
