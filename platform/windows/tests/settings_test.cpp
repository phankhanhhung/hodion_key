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

#include "HostClient.h"
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

// So MỌI trường. Có hàm này để khi ai đó thêm một tuỳ chọn mà quên ghi vào
// registry thì test đỏ ngay, chứ không phải đợi người dùng phát hiện cấu
// hình của mình biến mất sau khi khởi động lại máy.
bool SameSettings(const HodionSettings& a, const HodionSettings& b) {
  return SameEngine(a.engine, b.engine) &&
         a.vietnamese_on == b.vietnamese_on &&
         a.skip_input_scopes == b.skip_input_scopes &&
         a.auto_diacritics == b.auto_diacritics &&
         a.predict_margin == b.predict_margin && a.toggle == b.toggle &&
         a.method_key == b.method_key && a.predict_key == b.predict_key &&
         a.cancel_key == b.cancel_key && a.cycle_key == b.cycle_key;
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
  s.auto_diacritics = true;
  s.predict_margin = 35;
  s.toggle = ToggleKey{'Z', TF_MOD_ALT};
  s.method_key = ToggleKey{VK_F9, TF_MOD_CONTROL | TF_MOD_SHIFT};
  s.predict_key = ToggleKey{};                       // tắt hẳn
  s.cancel_key = ToggleKey{VK_OEM_5, TF_MOD_ALT};
  s.cycle_key = ToggleKey{VK_OEM_PERIOD, TF_MOD_CONTROL};

  Check(SaveHodionSettings(s), "SaveHodionSettings");
  HodionSettings loaded = LoadHodionSettings();
  Check(SameEngine(loaded.engine, s.engine), "vòng đọc/ghi cấu hình engine");
  Check(SameSettings(loaded, s), "MỌI tuỳ chọn đều sống qua một vòng ghi/đọc");
  Check(!loaded.vietnamese_on, "vòng đọc/ghi trạng thái bật/tắt");
  Check(!loaded.skip_input_scopes, "vòng đọc/ghi bỏ qua ô URL/mật khẩu");
  Check(loaded.auto_diacritics, "vòng đọc/ghi tự thêm dấu");
  Check(loaded.predict_margin == 35, "vòng đọc/ghi ngưỡng tin cậy");
  Check(loaded.toggle == s.toggle, "vòng đọc/ghi phím chuyển");
  Check(loaded.method_key == s.method_key, "vòng đọc/ghi phím Telex/VNI");
  Check(!loaded.predict_key.enabled(), "phím tắt hẳn vẫn tắt sau khi đọc lại");
  Check(loaded.cancel_key == s.cancel_key, "vòng đọc/ghi phím hủy dấu");
  Check(loaded.cycle_key == s.cycle_key, "vòng đọc/ghi phím xoay vòng");

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
  // Tự thêm dấu ĐỔI thứ người dùng vừa gõ, nên phải là lựa chọn tường minh.
  Check(!def.auto_diacritics, "mặc định TẮT tự thêm dấu");
  Check(def.predict_margin == 20, "ngưỡng tin cậy mặc định 2,0");

  // Ngưỡng hỏng trong registry không được làm mô hình đoán bừa.
  {
    HKEY key = nullptr;
    RegCreateKeyExW(HKEY_CURRENT_USER, kHodionSettingsKey, 0, nullptr,
                    REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &key,
                    nullptr);
    const DWORD huge = 999999;
    RegSetValueExW(key, L"PredictMargin", 0, REG_DWORD,
                   reinterpret_cast<const BYTE*>(&huge), sizeof(huge));
    RegCloseKey(key);
    Check(LoadHodionSettings().predict_margin == 20,
          "ngưỡng hỏng quay về mặc định");
  }
  Check(def.toggle == HodionDefaultToggleKey(), "phím chuyển mặc định");
  Check(def.cancel_key == HodionDefaultCancelKey(), "phím hủy dấu mặc định");
  Check(def.cycle_key == HodionDefaultCycleKey(), "phím xoay vòng mặc định");
  Check(def.toggle.valid() && def.cancel_key.valid(),
        "phím mặc định đều có modifier");
  // Hai phím bật sẵn phải khác nhau, nếu không cái sau đăng ký hỏng.
  Check(def.toggle != def.cancel_key && def.toggle != def.cycle_key &&
            def.cancel_key != def.cycle_key,
        "phím mặc định không trùng nhau");
  Check(!def.method_key.enabled() && !def.predict_key.enabled(),
        "phím Telex/VNI và tự thêm dấu mặc định tắt");

  // --- Phím tắt: tắt được, nhưng không được phép là phím trần ---
  {
    HKEY key = nullptr;
    RegCreateKeyExW(HKEY_CURRENT_USER, kHodionSettingsKey, 0, nullptr,
                    REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &key,
                    nullptr);
    const auto put = [&](const WCHAR* name, DWORD v) {
      RegSetValueExW(key, name, 0, REG_DWORD,
                     reinterpret_cast<const BYTE*>(&v), sizeof(v));
    };

    // Có phím nhưng không modifier: sẽ nuốt mất phím đó khi gõ → về mặc định.
    put(L"ToggleKey", VK_SPACE);
    put(L"ToggleMods", 0);
    Check(LoadHodionSettings().toggle == HodionDefaultToggleKey(),
          "phím thiếu modifier quay về mặc định");

    // vk = 0 là TẮT hẳn — hợp lệ, phải giữ nguyên ý người dùng.
    put(L"ToggleKey", 0);
    put(L"ToggleMods", 0);
    Check(!LoadHodionSettings().toggle.enabled(), "vk = 0 nghĩa là tắt hẳn");

    put(L"CancelKey", 0);
    put(L"CancelMods", 0);
    Check(!LoadHodionSettings().cancel_key.enabled(),
          "phím hủy dấu cũng tắt được");

    RegCloseKey(key);
  }

  // --- Mô tả phím cho người đọc ---
  Check(HodionDescribeKey(ToggleKey{}) == std::wstring(L"(tắt)"),
        "phím tắt hiện là (tắt)");
  {
    const std::wstring text =
        HodionDescribeKey(ToggleKey{VK_SPACE, TF_MOD_CONTROL});
    Check(text.find(L"Ctrl") != std::wstring::npos, "có nhắc Ctrl");
    Check(text.find(L"+") != std::wstring::npos, "có dấu cộng");
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

  // --- Kênh tới tiến trình nền: hỏng thì phải hỏng NHANH ---
  //
  // Test này chạy khi không có host nào, tức là đúng ca thường gặp nhất:
  // người dùng chưa mở tiến trình nền. Gõ không được phép chậm đi vì thế.
  {
    HostClient client;
    std::string reply;

    const ULONGLONG t0 = GetTickCount64();
    const bool ok = client.Request(hodionipc::Op::Ping, "", &reply, 20);
    const ULONGLONG elapsed = GetTickCount64() - t0;
    Check(!ok, "không có host thì Request trả false");
    Check(elapsed < 200, "không có host thì trả về ngay, không chờ");
    Check(client.quiet_remaining_ms() > 0, "hỏng rồi thì im một lúc");

    // Lần sau phải trả về tức thì mà không thử kết nối lại.
    const ULONGLONG t1 = GetTickCount64();
    Check(!client.Request(hodionipc::Op::Ping, "", &reply, 20),
          "vẫn trả false trong lúc đang im");
    Check(GetTickCount64() - t1 < 50, "trong lúc im thì không thử lại");

    // Gói tin quá khổ bị chặn ngay, không gửi đi đâu cả.
    const std::string huge(hodionipc::kMaxPayload + 1, 'a');
    Check(!client.Request(hodionipc::Op::Restore, huge, &reply, 20),
          "payload quá khổ bị từ chối");
    Check(!client.Request(hodionipc::Op::Ping, "", nullptr, 20),
          "reply null bị từ chối");
  }

  // --- Khởi động cùng Windows ---
  {
    const bool had = HodionGetAutoStart();

    Check(HodionSetAutoStart(true), "bật khởi động cùng Windows");
    Check(HodionGetAutoStart(), "bật rồi thì đọc lại thấy có");
    Check(HodionSetAutoStart(true), "bật hai lần không lỗi");

    Check(HodionSetAutoStart(false), "tắt khởi động cùng Windows");
    Check(!HodionGetAutoStart(), "tắt rồi thì đọc lại thấy không");
    // Xoá một mục không tồn tại không được coi là lỗi.
    Check(HodionSetAutoStart(false), "tắt hai lần không lỗi");

    HodionSetAutoStart(had);  // trả lại như cũ
  }

  SaveHodionSettings(original);  // trả lại cấu hình của người dùng

  std::printf(g_failures == 0 ? "settings_test: OK\n"
                              : "settings_test: %d failures\n",
              g_failures);
  return g_failures == 0 ? 0 : 1;
}
