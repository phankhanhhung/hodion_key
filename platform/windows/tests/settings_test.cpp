// Kiểm thử tầng Windows chạy được thật: vòng đọc/ghi registry và cơ chế
// theo dõi thay đổi cấu hình (RegNotifyChangeKeyValue) mà text service dùng
// để nhận cấu hình mới ngay khi người dùng bấm OK trong app cấu hình.
//
// Test ghi vào HKCU\Software\HodionKey nên nó lưu lại cấu hình đang có và
// khôi phục trước khi kết thúc.
#include <msctf.h>

#include <algorithm>
#include <cstdio>
#include <string>

#include "Settings.h"
#include "WordScan.h"
#include "hodion/reconvert.h"
#include "hodion/utf.h"

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

std::wstring Wide(const char32_t* s) {
  const std::u16string u16 = hodion::utf::to_utf16(std::u32string(s));
  return std::wstring(u16.begin(), u16.end());
}

// Kiểm tra ranh giới từ quanh con trỏ: `text` là văn bản với dấu | đánh dấu
// vị trí con trỏ; `want` là từ phải lấy ra.
void CheckWord(const char32_t* text, const char32_t* want) {
  const std::wstring all = Wide(text);
  const size_t caret = all.find(L'|');
  const std::wstring before = all.substr(0, caret);
  const std::wstring after = all.substr(caret + 1);

  size_t start = 0, end = 0;
  HodionWordAround(before, after, &start, &end);
  const std::wstring got = before.substr(start) + after.substr(0, end);
  if (got != Wide(want)) {
    ++g_failures;
    std::printf("FAIL: ranh giới từ sai (lấy được %u ký tự, cần %u)\n",
                static_cast<unsigned>(got.size()),
                static_cast<unsigned>(Wide(want).size()));
  }
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

  // --- Ranh giới từ cho reconversion ---
  //
  // Lấy nhầm ranh giới nghĩa là sửa nhầm chữ của người dùng, nên đây là chỗ
  // phải chắc tay nhất trong cả tính năng.
  {
    // Con trỏ ở cuối từ, giữa từ, đầu từ.
    CheckWord(U"viet|", U"viet");
    CheckWord(U"vi|et", U"viet");
    CheckWord(U"|viet", U"viet");
    CheckWord(U"xin chao |viet nam", U"viet");
    CheckWord(U"xin chao vi|et nam", U"viet");
    CheckWord(U"xin chao viet| nam", U"viet");

    // Chữ đã có dấu, kể cả đ.
    CheckWord(U"việt|", U"việt");
    CheckWord(U"đường| phượng", U"đường");
    CheckWord(U"con đ|ường", U"đường");

    // Không có chữ nào cạnh con trỏ.
    CheckWord(U"a |", U"");
    CheckWord(U"| b", U"");
    CheckWord(U"|b c", U"b");
    CheckWord(U"", U"");
    CheckWord(U"|", U"");
    CheckWord(U"123|456", U"");
    CheckWord(U"a, |, b", U"");

    // Dấu câu và chữ số cắt từ.
    CheckWord(U"chao,viet|", U"viet");
    CheckWord(U"a1viet|", U"viet");
    CheckWord(U"viet|.nam", U"viet");
    CheckWord(U"(viet|)", U"viet");

    Check(HodionIsWordChar(L'a') && HodionIsWordChar(L'Z'),
          "chữ cái ASCII là ký tự của từ");
    Check(!HodionIsWordChar(L' ') && !HodionIsWordChar(L'.') &&
              !HodionIsWordChar(L'1') && !HodionIsWordChar(L'\0'),
          "dấu cách, dấu câu, chữ số không phải ký tự của từ");
    Check(HodionIsWordChar(Wide(U"ệ")[0]) && HodionIsWordChar(Wide(U"đ")[0]),
          "chữ tiếng Việt dựng sẵn là ký tự của từ");
    Check(!HodionIsWordChar(Wide(U"中")[0]), "chữ Hán không phải chữ Việt");
  }

  // --- Danh sách phương án lấy đúng từ engine ---
  {
    const auto v = hodion::syllable_variants(U"duong", hodion::Config{});
    Check(v.size() > 10, "duong có nhiều phương án");
    Check(std::find(v.begin(), v.end(), std::u32string(U"đường")) != v.end(),
          "đường nằm trong phương án của duong");
  }

  SaveHodionSettings(original);  // trả lại cấu hình của người dùng

  std::printf(g_failures == 0 ? "settings_test: OK\n"
                              : "settings_test: %d failures\n",
              g_failures);
  return g_failures == 0 ? 0 : 1;
}
