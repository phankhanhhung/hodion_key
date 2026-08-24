// HodionKeyConfig — hộp thoại cấu hình bộ gõ.
//
// Chỉ đọc/ghi HKCU\Software\HodionKey; text service đang chạy tự nhận thay
// đổi qua RegNotifyChangeKeyValue nên bấm OK là có hiệu lực ngay, không cần
// khởi động lại ứng dụng đang gõ.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <msctf.h>

#include <string>

#include "Settings.h"
#include "resource.h"

namespace {

// Đưa cửa sổ của phiên bản đang chạy lên trước thay vì mở thêm hộp thoại.
UINT g_showMessage = 0;
constexpr WCHAR kMutexName[] = L"Local\\HodionKeyConfig.SingleInstance";
constexpr WCHAR kShowMessageName[] = L"HodionKeyConfig.Show";

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
    {IDC_OPT_INPUTSCOPE,
     L"Tự tắt ở ô địa chỉ web, email, mật khẩu, ô số"},
    {IDC_LBL_MIXEDHINT,
     L"Ctrl + Backspace khi đang gõ dở: bỏ dấu cho riêng từ đó."},
    {IDC_GRP_TOGGLE, L"Chuyển Việt / Anh"},
    {IDC_LBL_TOGGLE, L"&Phím chuyển:"},
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

std::wstring DescribeToggleKey(const ToggleKey& key) {
  // Ưu tiên nhãn dựng sẵn để chữ trong gợi ý khớp hệt mục trong danh sách.
  int count = 0;
  const TogglePreset* presets = HodionTogglePresets(&count);
  for (int i = 0; i < count; ++i) {
    if (presets[i].key == key) return presets[i].label;
  }

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

void FillToggleCombo(HWND dlg, const ToggleKey& current) {
  HWND combo = GetDlgItem(dlg, IDC_COMBO_TOGGLE);
  SendMessageW(combo, CB_RESETCONTENT, 0, 0);

  int count = 0;
  const TogglePreset* presets = HodionTogglePresets(&count);
  int selected = -1;
  for (int i = 0; i < count; ++i) {
    const int index = static_cast<int>(SendMessageW(
        combo, CB_ADDSTRING, 0,
        reinterpret_cast<LPARAM>(presets[i].label)));
    SendMessageW(combo, CB_SETITEMDATA, index, static_cast<LPARAM>(i));
    if (presets[i].key == current) selected = index;
  }

  if (selected < 0) {
    // Phím do người dùng đặt tay trong registry — giữ nguyên, chỉ hiển thị.
    const std::wstring text = DescribeToggleKey(current);
    const int index = static_cast<int>(SendMessageW(
        combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str())));
    SendMessageW(combo, CB_SETITEMDATA, index, static_cast<LPARAM>(-1));
    selected = index;
  }
  SendMessageW(combo, CB_SETCURSEL, selected, 0);
}

ToggleKey SelectedToggleKey(HWND dlg, const ToggleKey& fallback) {
  HWND combo = GetDlgItem(dlg, IDC_COMBO_TOGGLE);
  const int sel = static_cast<int>(SendMessageW(combo, CB_GETCURSEL, 0, 0));
  if (sel == CB_ERR) return fallback;

  const int preset =
      static_cast<int>(SendMessageW(combo, CB_GETITEMDATA, sel, 0));
  if (preset < 0) return fallback;  // mục "tự đặt"

  int count = 0;
  const TogglePreset* presets = HodionTogglePresets(&count);
  if (preset >= count) return fallback;
  return presets[preset].key;
}

void UpdateEnabledState(HWND dlg) {
  const bool telex = Checked(dlg, IDC_METHOD_TELEX);
  EnableWindow(GetDlgItem(dlg, IDC_OPT_WSHORTHAND), telex);
  EnableWindow(GetDlgItem(dlg, IDC_OPT_BRACKETS), telex);

  const ToggleKey key = SelectedToggleKey(dlg, ToggleKey{});
  std::wstring hint = Checked(dlg, IDC_CHK_VIETNAMESE)
                          ? L"Bấm "
                          : L"Đang tắt — bấm ";
  hint += DescribeToggleKey(key);
  hint += L" khi gõ để chuyển Việt / Anh.";
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
  Check(dlg, IDC_OPT_INPUTSCOPE, s.skip_input_scopes);
  Check(dlg, IDC_CHK_VIETNAMESE, s.vietnamese_on);

  FillToggleCombo(dlg, s.toggle);
  UpdateEnabledState(dlg);
}

HodionSettings ReadFromDialog(HWND dlg, const ToggleKey& fallback) {
  HodionSettings s;
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
  s.skip_input_scopes = Checked(dlg, IDC_OPT_INPUTSCOPE);
  s.vietnamese_on = Checked(dlg, IDC_CHK_VIETNAMESE);
  s.toggle = SelectedToggleKey(dlg, fallback);
  return s;
}

INT_PTR CALLBACK DlgProc(HWND dlg, UINT msg, WPARAM wParam, LPARAM) {
  // Phím chuyển của cấu hình đang mở, dùng khi người dùng để mục "tự đặt".
  static ToggleKey s_toggle{};

  if (msg == g_showMessage && g_showMessage != 0) {
    if (IsIconic(dlg)) ShowWindow(dlg, SW_RESTORE);
    SetForegroundWindow(dlg);
    return TRUE;
  }

  switch (msg) {
    case WM_INITDIALOG: {
      SetWindowTextW(dlg, L"HodionKey — Cấu hình bộ gõ");
      for (const Labels& l : kLabels) SetDlgItemTextW(dlg, l.id, l.text);

      const HodionSettings s = LoadHodionSettings();
      s_toggle = s.toggle;
      LoadIntoDialog(dlg, s);
      return TRUE;
    }

    case WM_COMMAND:
      switch (LOWORD(wParam)) {
        case IDC_METHOD_TELEX:
        case IDC_METHOD_VNI:
        case IDC_CHK_VIETNAMESE:
          UpdateEnabledState(dlg);
          return TRUE;

        case IDC_COMBO_TOGGLE:
          if (HIWORD(wParam) == CBN_SELCHANGE) UpdateEnabledState(dlg);
          return TRUE;

        case IDC_DEFAULTS: {
          HodionSettings def;  // mặc định trùng UniKey
          def.vietnamese_on = Checked(dlg, IDC_CHK_VIETNAMESE);
          int count = 0;
          def.toggle = HodionTogglePresets(&count)[0].key;
          s_toggle = def.toggle;
          LoadIntoDialog(dlg, def);
          return TRUE;
        }

        case IDOK: {
          const HodionSettings s = ReadFromDialog(dlg, s_toggle);
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

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int) {
  g_showMessage = RegisterWindowMessageW(kShowMessageName);

  HANDLE mutex = CreateMutexW(nullptr, TRUE, kMutexName);
  if (mutex && GetLastError() == ERROR_ALREADY_EXISTS) {
    // Đã có hộp thoại đang mở — đánh thức nó rồi thoát.
    if (g_showMessage != 0) {
      SendMessageTimeoutW(HWND_BROADCAST, g_showMessage, 0, 0, SMTO_ABORTIFHUNG,
                          200, nullptr);
    }
    CloseHandle(mutex);
    return 0;
  }

  INITCOMMONCONTROLSEX icc;
  icc.dwSize = sizeof(icc);
  icc.dwICC = ICC_STANDARD_CLASSES;
  InitCommonControlsEx(&icc);

  DialogBoxParamW(instance, MAKEINTRESOURCEW(IDD_CONFIG), nullptr, DlgProc, 0);

  if (mutex) {
    ReleaseMutex(mutex);
    CloseHandle(mutex);
  }
  return 0;
}
