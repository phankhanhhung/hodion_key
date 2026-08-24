// Kiểm thử tầng Windows chạy được thật: vòng đọc/ghi registry và cơ chế
// theo dõi thay đổi cấu hình (RegNotifyChangeKeyValue) mà text service dùng
// để nhận cấu hình mới ngay khi người dùng bấm OK trong app cấu hình.
//
// Test ghi vào HKCU\Software\HodionKey nên nó lưu lại cấu hình đang có và
// khôi phục trước khi kết thúc.
#include <msctf.h>

#include <cstdio>

#include "Settings.h"

namespace {

int g_failures = 0;

void Check(bool cond, const char* what) {
  if (!cond) {
    ++g_failures;
    std::printf("FAIL: %s\n", what);
  }
}

bool SameEngine(const hodion::Config& a, const hodion::Config& b) {
  return a.method == b.method && a.tone_style == b.tone_style &&
         a.free_marking == b.free_marking && a.spell_check == b.spell_check &&
         a.restore_non_vn == b.restore_non_vn &&
         a.w_shorthand == b.w_shorthand &&
         a.telex_brackets == b.telex_brackets &&
         a.english_detect == b.english_detect;
}

}  // namespace

int main() {
  const HodionSettings original = LoadHodionSettings();

  // Ghi cấu hình khác hẳn mặc định rồi đọc lại.
  HodionSettings s;
  s.engine.method = hodion::InputMethod::Vni;
  s.engine.tone_style = hodion::ToneStyle::Modern;
  s.engine.free_marking = false;
  s.engine.spell_check = false;
  s.engine.restore_non_vn = true;
  s.engine.w_shorthand = false;
  s.engine.telex_brackets = false;
  s.engine.english_detect = false;
  s.vietnamese_on = false;
  s.skip_input_scopes = false;
  s.toggle = ToggleKey{'Z', TF_MOD_ALT};

  Check(SaveHodionSettings(s), "SaveHodionSettings");
  HodionSettings loaded = LoadHodionSettings();
  Check(SameEngine(loaded.engine, s.engine), "vòng đọc/ghi cấu hình engine");
  Check(!loaded.vietnamese_on, "vòng đọc/ghi trạng thái bật/tắt");
  Check(!loaded.skip_input_scopes, "vòng đọc/ghi bỏ qua ô URL/mật khẩu");
  Check(loaded.toggle == s.toggle, "vòng đọc/ghi phím chuyển");

  // Chỉ đổi trạng thái bật/tắt (đường đi khi người dùng bấm phím chuyển).
  Check(SaveHodionVietnameseOn(true), "SaveHodionVietnameseOn");
  loaded = LoadHodionSettings();
  Check(loaded.vietnamese_on, "bật lại tiếng Việt");
  Check(loaded.toggle == s.toggle, "phím chuyển không bị đụng tới");

  // Watcher: im lặng khi không có gì đổi, báo đúng một lần sau khi ghi.
  SettingsWatcher watcher;
  Check(watcher.start(), "SettingsWatcher::start");
  Check(!watcher.poll(), "watcher im khi cấu hình chưa đổi");

  SaveHodionVietnameseOn(false);
  bool signalled = false;
  for (int i = 0; i < 50 && !signalled; ++i) {  // registry báo bất đồng bộ
    signalled = watcher.poll();
    if (!signalled) Sleep(20);
  }
  Check(signalled, "watcher báo khi cấu hình đổi");
  Check(!watcher.poll(), "watcher đăng ký lại và im sau khi đã báo");

  // Giá trị mặc định phải trùng mặc định của UniKey.
  const HodionSettings def;
  Check(def.engine.method == hodion::InputMethod::Telex &&
            def.engine.tone_style == hodion::ToneStyle::Traditional &&
            def.engine.free_marking && def.engine.spell_check &&
            !def.engine.restore_non_vn,
        "mặc định trùng UniKey");
  Check(def.skip_input_scopes, "mặc định có bỏ qua ô URL/email/mật khẩu");
  Check(def.engine.english_detect, "mặc định có nhận diện từ tiếng Anh");
  int preset_count = 0;
  const TogglePreset* presets = HodionTogglePresets(&preset_count);
  Check(preset_count > 0 && presets[0].key == def.toggle,
        "phím chuyển mặc định là mục đầu danh sách");
  for (int i = 0; i < preset_count; ++i) {
    Check(presets[i].key.valid(), "mọi phím chuyển dựng sẵn đều có modifier");
  }

  // Phím chuyển hỏng trong registry không được phép chiếm một phím trần.
  {
    HKEY key = nullptr;
    RegCreateKeyExW(HKEY_CURRENT_USER, kHodionSettingsKey, 0, nullptr,
                    REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &key,
                    nullptr);
    const DWORD none = 0;
    RegSetValueExW(key, L"ToggleMods", 0, REG_DWORD,
                   reinterpret_cast<const BYTE*>(&none), sizeof(none));
    RegCloseKey(key);

    const HodionSettings bad = LoadHodionSettings();
    Check(bad.toggle.valid(), "phím chuyển thiếu modifier bị loại");
    Check(bad.toggle == presets[0].key, "quay về phím chuyển mặc định");
  }

  SaveHodionSettings(original);  // trả lại cấu hình của người dùng

  std::printf(g_failures == 0 ? "settings_test: OK\n"
                              : "settings_test: %d failures\n",
              g_failures);
  return g_failures == 0 ? 0 : 1;
}
