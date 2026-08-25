// Hộp thoại cấu hình bộ gõ.
//
// Chỉ đọc/ghi HKCU\Software\HodionKey; text service đang chạy tự nhận thay
// đổi qua RegNotifyChangeKeyValue nên bấm OK là có hiệu lực ngay, không cần
// khởi động lại ứng dụng đang gõ.
//
// Trước đây đây là toàn bộ chương trình. Nay nó chỉ là một mục trong menu
// khay hệ thống — vòng đời tiến trình do Host.cpp giữ.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <msctf.h>

#include <string>

#include "ConfigDialog.h"
#include "Settings.h"
#include "resource.h"

namespace {

struct Labels {
  int id;
  const WCHAR* text;
};

const Labels kLabels[] = {
    {IDC_GRP_METHOD, L"Kiểu gõ"},
    {IDC_METHOD_TELEX, L"&Telex"},
    {IDC_METHOD_VNI, L"&VNI"},
    {IDC_GRP_TONE, L"Bỏ dấu"},
    {IDC_TONE_TRADITIONAL, L"Kiểu &cũ  (hòa, khỏe, thúy)"},
    {IDC_TONE_MODERN, L"Kiểu &mới  (hoà, khoẻ, thuý)"},
    {IDC_GRP_OPTIONS, L"Tùy chọn gõ"},
    {IDC_OPT_FREEMARKING, L"Gõ dấu tự do — đặt dấu ở cuối từ (viete → viêt)"},
    {IDC_OPT_SPELLCHECK, L"Kiểm tra chính tả — từ sai thì ngừng biến đổi"},
    {IDC_OPT_RESTORE, L"Tự khôi phục từ không phải tiếng Việt (boxing)"},
    {IDC_OPT_WSHORTHAND, L"Telex: phím w thành ư khi không đánh được móc"},
    {IDC_OPT_BRACKETS, L"Telex: [ ] { } thành ơ ư Ơ Ư"},
    {IDC_GRP_MIXED, L"Gõ trộn Việt – Anh"},
    {IDC_OPT_INPUTSCOPE, L"Tự tắt ở ô địa chỉ web, email, mật khẩu, ô số"},
    {IDC_LBL_MIXEDHINT, L"Từ tiếng Anh đã biết được trả lại nguyên chữ."},
    {IDC_OPT_ENGLISH, L"Trả lại nguyên chữ với từ tiếng Anh đã biết (meeting)"},
    {IDC_GRP_PREDICT, L"Tự thêm dấu"},
    {IDC_OPT_AUTODIACRITIC,
     L"Thêm dấu cho chữ không dấu khi chốt từ (nguyet → nguyệt)"},
    {IDC_LBL_PREDICTHINT,
     L"Cần tiến trình nền và file dữ liệu đặt cạnh chương trình."},
    {IDC_GRP_KEYS, L"Phím tắt"},
    {IDC_LBL_KEY_TOGGLE, L"Chuyển &Việt / Anh:"},
    {IDC_LBL_KEY_METHOD, L"Chuyển Telex / V&NI:"},
    {IDC_LBL_KEY_PREDICT, L"Bật/tắt tự thêm &dấu:"},
    {IDC_LBL_KEY_CANCEL, L"Bỏ dấu cho &1 từ:"},
    {IDC_LBL_KEY_CYCLE, L"Đổi dấu từ t&rước:"},
    {IDC_LBL_KEYHINT,
     L"Bấm tổ hợp phím vào ô để đặt. Bấm Delete để tắt phím đó.\n"
     L"Mỗi phím phải có Ctrl, Alt hoặc Shift — phím trần sẽ bị nuốt mất.\n"
     L"\"Đổi dấu từ trước\" bấm nhiều lần để đi hết các cách viết."},
    {IDC_CHK_VIETNAMESE, L"Đang &bật gõ tiếng Việt"},
    {IDC_DEFAULTS, L"Mặc &định"},
    {IDOK, L"OK"},
    {IDCANCEL, L"Hủy"},
};

bool Checked(HWND dlg, int id) {
  return IsDlgButtonChecked(dlg, id) == BST_CHECKED;
}

void Check(HWND dlg, int id, bool on) {
  CheckDlgButton(dlg, id, on ? BST_CHECKED : BST_UNCHECKED);
}

// --- Cầu nối giữa ô bắt phím của Windows và cách TSF đánh số modifier ---
//
// Hai bên dùng bit khác nhau cho cùng một thứ, và ô bắt phím thì đảo thứ tự
// (SHIFT là bit thấp nhất bên nó, ALT là bit thấp nhất bên TSF).

WORD ToHotkeyWord(const ToggleKey& key) {
  if (!key.enabled()) return 0;
  BYTE flags = 0;
  if (key.mods & TF_MOD_SHIFT) flags |= HOTKEYF_SHIFT;
  if (key.mods & TF_MOD_CONTROL) flags |= HOTKEYF_CONTROL;
  if (key.mods & TF_MOD_ALT) flags |= HOTKEYF_ALT;
  return MAKEWORD(static_cast<BYTE>(key.vk), flags);
}

ToggleKey FromHotkeyWord(WORD value) {
  ToggleKey key;
  const BYTE vk = LOBYTE(value);
  if (vk == 0) return key;  // ô trống = tắt
  const BYTE flags = HIBYTE(value);
  key.vk = vk;
  if (flags & HOTKEYF_SHIFT) key.mods |= TF_MOD_SHIFT;
  if (flags & HOTKEYF_CONTROL) key.mods |= TF_MOD_CONTROL;
  if (flags & HOTKEYF_ALT) key.mods |= TF_MOD_ALT;
  return key;
}

void SetHotkey(HWND dlg, int id, const ToggleKey& key) {
  SendDlgItemMessageW(dlg, id, HKM_SETHOTKEY, ToHotkeyWord(key), 0);
}

ToggleKey GetHotkey(HWND dlg, int id) {
  return FromHotkeyWord(static_cast<WORD>(
      SendDlgItemMessageW(dlg, id, HKM_GETHOTKEY, 0, 0)));
}

struct KeyField {
  int control;
  const WCHAR* name;
  ToggleKey HodionSettings::*member;
};

const KeyField kKeyFields[] = {
    {IDC_HOTKEY_TOGGLE, L"Chuyển Việt / Anh", &HodionSettings::toggle},
    {IDC_HOTKEY_METHOD, L"Chuyển Telex / VNI", &HodionSettings::method_key},
    {IDC_HOTKEY_PREDICT, L"Bật/tắt tự thêm dấu", &HodionSettings::predict_key},
    {IDC_HOTKEY_CANCEL, L"Bỏ dấu cho 1 từ", &HodionSettings::cancel_key},
    {IDC_HOTKEY_CYCLE, L"Đổi dấu từ trước", &HodionSettings::cycle_key},
};

// Phím tắt hỏng thì phải nói ngay lúc bấm OK, không lặng lẽ bỏ qua: người
// dùng vừa đặt xong mà nó không chạy thì họ sẽ tưởng bộ gõ hỏng.
bool ValidateKeys(HWND dlg, const HodionSettings& s) {
  for (const KeyField& f : kKeyFields) {
    const ToggleKey& key = s.*(f.member);
    if (key.enabled() && !key.valid()) {
      std::wstring message = L"Phím tắt \"";
      message += f.name;
      message += L"\" phải có Ctrl, Alt hoặc Shift.\n\n";
      message += L"Phím trần sẽ bị bộ gõ nuốt mất, không gõ được nữa.";
      MessageBoxW(dlg, message.c_str(), L"HodionKey", MB_ICONWARNING | MB_OK);
      SetFocus(GetDlgItem(dlg, f.control));
      return false;
    }
  }

  for (size_t i = 0; i < ARRAYSIZE(kKeyFields); ++i) {
    for (size_t j = i + 1; j < ARRAYSIZE(kKeyFields); ++j) {
      const ToggleKey& a = s.*(kKeyFields[i].member);
      const ToggleKey& b = s.*(kKeyFields[j].member);
      if (!a.enabled() || a != b) continue;
      std::wstring message = L"\"";
      message += kKeyFields[i].name;
      message += L"\" và \"";
      message += kKeyFields[j].name;
      message += L"\" đang dùng cùng một tổ hợp (";
      message += HodionDescribeKey(a);
      message += L").";
      MessageBoxW(dlg, message.c_str(), L"HodionKey", MB_ICONWARNING | MB_OK);
      SetFocus(GetDlgItem(dlg, kKeyFields[j].control));
      return false;
    }
  }
  return true;
}

void UpdateEnabledState(HWND dlg) {
  const bool telex = Checked(dlg, IDC_METHOD_TELEX);
  EnableWindow(GetDlgItem(dlg, IDC_OPT_WSHORTHAND), telex);
  EnableWindow(GetDlgItem(dlg, IDC_OPT_BRACKETS), telex);
  // VNI dùng chữ số làm phím dấu nên không từ tiếng Anh nào bị biến dạng —
  // từ điển hoàn toàn vô tác dụng ở đó.
  EnableWindow(GetDlgItem(dlg, IDC_OPT_ENGLISH), telex);

  const ToggleKey toggle = GetHotkey(dlg, IDC_HOTKEY_TOGGLE);
  std::wstring hint;
  if (Checked(dlg, IDC_CHK_VIETNAMESE)) {
    hint = toggle.enabled() ? L"Bấm " + HodionDescribeKey(toggle) +
                                  L" khi gõ để chuyển Việt / Anh."
                            : L"Đang bật. Chuyển bằng icon ở khay hệ thống.";
  } else {
    hint = toggle.enabled() ? L"Đang tắt — bấm " + HodionDescribeKey(toggle) +
                                  L" để bật lại."
                            : L"Đang tắt. Bật lại bằng icon ở khay hệ thống.";
  }
  SetDlgItemTextW(dlg, IDC_LBL_HINT, hint.c_str());
}

void LoadIntoDialog(HWND dlg, const HodionSettings& s) {
  const bool telex = s.engine.method == hodion::InputMethod::Telex;
  Check(dlg, IDC_METHOD_TELEX, telex);
  Check(dlg, IDC_METHOD_VNI, !telex);

  const bool modern = s.engine.tone_style == hodion::ToneStyle::Modern;
  Check(dlg, IDC_TONE_TRADITIONAL, !modern);
  Check(dlg, IDC_TONE_MODERN, modern);

  Check(dlg, IDC_OPT_FREEMARKING, s.engine.free_marking);
  Check(dlg, IDC_OPT_SPELLCHECK, s.engine.spell_check);
  Check(dlg, IDC_OPT_RESTORE, s.engine.restore_non_vn);
  Check(dlg, IDC_OPT_WSHORTHAND, s.engine.w_shorthand);
  Check(dlg, IDC_OPT_BRACKETS, s.engine.telex_brackets);
  Check(dlg, IDC_OPT_ENGLISH, s.engine.english_detect);
  Check(dlg, IDC_OPT_INPUTSCOPE, s.skip_input_scopes);
  Check(dlg, IDC_OPT_AUTODIACRITIC, s.auto_diacritics);
  Check(dlg, IDC_CHK_VIETNAMESE, s.vietnamese_on);

  for (const KeyField& f : kKeyFields) SetHotkey(dlg, f.control, s.*(f.member));
  UpdateEnabledState(dlg);
}

HodionSettings ReadFromDialog(HWND dlg, const HodionSettings& previous) {
  HodionSettings s = previous;  // giữ những gì hộp thoại không hiển thị
  s.engine.method = Checked(dlg, IDC_METHOD_VNI) ? hodion::InputMethod::Vni
                                                 : hodion::InputMethod::Telex;
  s.engine.tone_style = Checked(dlg, IDC_TONE_MODERN)
                            ? hodion::ToneStyle::Modern
                            : hodion::ToneStyle::Traditional;
  s.engine.free_marking = Checked(dlg, IDC_OPT_FREEMARKING);
  s.engine.spell_check = Checked(dlg, IDC_OPT_SPELLCHECK);
  s.engine.restore_non_vn = Checked(dlg, IDC_OPT_RESTORE);
  s.engine.w_shorthand = Checked(dlg, IDC_OPT_WSHORTHAND);
  s.engine.telex_brackets = Checked(dlg, IDC_OPT_BRACKETS);
  s.engine.english_detect = Checked(dlg, IDC_OPT_ENGLISH);
  s.skip_input_scopes = Checked(dlg, IDC_OPT_INPUTSCOPE);
  s.auto_diacritics = Checked(dlg, IDC_OPT_AUTODIACRITIC);
  s.vietnamese_on = Checked(dlg, IDC_CHK_VIETNAMESE);
  for (const KeyField& f : kKeyFields) {
    s.*(f.member) = GetHotkey(dlg, f.control);
  }
  return s;
}

INT_PTR CALLBACK DlgProc(HWND dlg, UINT msg, WPARAM wParam, LPARAM) {
  // Cấu hình lúc mở hộp thoại, để giữ lại những giá trị không có ô hiển thị
  // (ví dụ ngưỡng tin cậy của mô hình) thay vì ghi đè bằng mặc định.
  static HodionSettings s_loaded;

  switch (msg) {
    case WM_INITDIALOG: {
      SetWindowTextW(dlg, L"HodionKey — Cấu hình bộ gõ");
      SendMessageW(dlg, WM_SETICON, ICON_BIG,
                   reinterpret_cast<LPARAM>(LoadIconW(
                       GetModuleHandleW(nullptr),
                       MAKEINTRESOURCEW(IDI_APP))));
      for (const Labels& l : kLabels) SetDlgItemTextW(dlg, l.id, l.text);

      s_loaded = LoadHodionSettings();
      LoadIntoDialog(dlg, s_loaded);
      return TRUE;
    }

    case WM_COMMAND:
      switch (LOWORD(wParam)) {
        case IDC_METHOD_TELEX:
        case IDC_METHOD_VNI:
        case IDC_CHK_VIETNAMESE:
          UpdateEnabledState(dlg);
          return TRUE;

        case IDC_HOTKEY_TOGGLE:
          if (HIWORD(wParam) == EN_CHANGE) UpdateEnabledState(dlg);
          return TRUE;

        case IDC_DEFAULTS: {
          HodionSettings def;  // mặc định trùng UniKey
          def.vietnamese_on = Checked(dlg, IDC_CHK_VIETNAMESE);
          def.toggle = HodionDefaultToggleKey();
          def.cancel_key = HodionDefaultCancelKey();
          def.cycle_key = HodionDefaultCycleKey();
          LoadIntoDialog(dlg, def);
          return TRUE;
        }

        case IDOK: {
          const HodionSettings s = ReadFromDialog(dlg, s_loaded);
          if (!ValidateKeys(dlg, s)) return TRUE;
          if (!SaveHodionSettings(s)) {
            MessageBoxW(dlg,
                        L"Không ghi được cấu hình vào registry "
                        L"(HKCU\\Software\\HodionKey).",
                        L"HodionKey", MB_ICONERROR | MB_OK);
            return TRUE;
          }
          EndDialog(dlg, IDOK);
          return TRUE;
        }

        case IDCANCEL:
          EndDialog(dlg, IDCANCEL);
          return TRUE;

        default:
          return FALSE;
      }

    case WM_CLOSE:
      EndDialog(dlg, IDCANCEL);
      return TRUE;

    default:
      return FALSE;
  }
}

}  // namespace

INT_PTR HodionShowConfigDialog(HINSTANCE instance, HWND parent) {
  return DialogBoxParamW(instance, MAKEINTRESOURCEW(IDD_CONFIG), parent,
                         DlgProc, 0);
}
