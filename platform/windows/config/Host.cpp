// HodionKey host — tiến trình thường trú của bộ gõ.
//
// DLL text service nằm trong từng ứng dụng đang gõ, nên nó phải nhẹ và
// không được giữ thứ gì to. Mọi thứ nặng — sau này là mô hình đoán dấu cho
// chữ không dấu — sống ở ĐÂY, một tiến trình duy nhất cho cả phiên đăng
// nhập, và DLL hỏi sang qua named pipe.
//
// Quan hệ phụ thuộc chỉ có một chiều: **gõ không cần host**. Host tắt, chưa
// chạy, hay treo thì bộ gõ vẫn gõ y như cũ, không chậm một mili giây nào.
// Host chỉ thêm vào những thứ không có cũng không sao.
//
// Bản thân tiến trình này giữ: icon trạng thái ở khay hệ thống, menu, và
// hộp thoại cấu hình (ConfigDialog.cpp).
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>

#include <string>
#include <vector>

#include "ConfigDialog.h"
#include "HostServer.h"
#include "Settings.h"
#include "TrayIcon.h"
#include "hodion/ngram.h"
#include "hodion/predict.h"
#include "hodion/utf.h"
#include "hodion/viet_words.h"
#include "resource.h"

namespace {

constexpr WCHAR kWindowClass[] = L"HodionKeyHostWindow";
constexpr WCHAR kMutexName[] = L"Local\\HodionKey.Host.SingleInstance";
constexpr WCHAR kShowMessageName[] = L"HodionKey.ShowConfig";

constexpr UINT WM_TRAY_CALLBACK = WM_APP + 1;

// Bảng âm tiết đặt cạnh exe. Cố ý là file rời chứ không biên dịch vào:
// từ điển nguồn là GPL-2 (xem tools/build_syllables.py), và để bảng rời thì
// sau này thay bằng mô hình tốt hơn cũng không phải dịch lại.
constexpr WCHAR kSyllableFile[] = L"viet-syllables.txt";
constexpr WCHAR kModelFile[] = L"viet-ngram.bin";
constexpr DWORD kMaxSyllableFileBytes = 8u * 1024 * 1024;
constexpr DWORD kMaxModelFileBytes = 512u * 1024 * 1024;

enum MenuId {
  kMenuVietnamese = 100,
  kMenuTelex,
  kMenuVni,
  kMenuAutoDiacritics,
  kMenuConfig,
  kMenuAutoStart,
  kMenuQuit,
};

UINT g_showMessage = 0;

struct Host {
  HINSTANCE instance = nullptr;
  hodion::SyllableList syllables;
  hodion::NgramModel model;
  HWND window = nullptr;
  TrayIcon tray;
  SettingsWatcher watcher;
  HodionSettings settings;
  HostServer server;
  // Cấu hình mà luồng phục vụ đọc. Chỉ đổi bằng cách ghi nguyên khối từ
  // luồng UI, và mỗi trường đọc ra đều tự nó hợp lệ, nên không cần khoá.
  volatile bool serverVietnameseStyleModern = false;
  volatile float serverMargin = 2.0f;
  bool inDialog = false;
};

Host* g_host = nullptr;

// Đọc cả file vào bộ nhớ. Dùng API Win32 chứ không dùng ifstream vì đường
// dẫn có thể chứa ký tự ngoài ASCII.
bool ReadWholeFile(const std::wstring& path, std::string* out,
                   DWORD maxBytes) {
  HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                            nullptr);
  if (file == INVALID_HANDLE_VALUE) return false;

  LARGE_INTEGER size;
  if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 ||
      size.QuadPart > maxBytes) {
    CloseHandle(file);
    return false;
  }

  out->resize(static_cast<size_t>(size.QuadPart));
  DWORD read = 0;
  const bool ok = ReadFile(file, &(*out)[0], static_cast<DWORD>(out->size()),
                           &read, nullptr) &&
                  read == out->size();
  CloseHandle(file);
  if (!ok) out->clear();
  return ok;
}

// Nạp bảng âm tiết và mô hình đặt cạnh exe. Thiếu bảng thì phần đoán dấu
// không bật được; có bảng mà thiếu mô hình thì vẫn đoán được những chữ chỉ
// có một cách viết. Mọi thứ khác chạy như thường trong cả hai trường hợp.
void LoadLanguageData(Host& host) {
  WCHAR path[MAX_PATH] = {};
  const DWORD len = GetModuleFileNameW(nullptr, path, ARRAYSIZE(path));
  if (len == 0 || len >= ARRAYSIZE(path)) return;

  std::wstring dir(path, len);
  const size_t slash = dir.find_last_of(L'\\');
  if (slash == std::wstring::npos) return;
  dir.resize(slash + 1);

  std::string contents;
  if (ReadWholeFile(dir + kSyllableFile, &contents, kMaxSyllableFileBytes)) {
    host.syllables.load(contents);
  }
  contents.clear();
  contents.shrink_to_fit();
  if (ReadWholeFile(dir + kModelFile, &contents, kMaxModelFileBytes)) {
    host.model.load(contents);
  }
}

// Tách payload: trường đầu là chữ vừa gõ, các trường sau là ngữ cảnh trái
// (cũ nhất trước).
std::vector<std::u32string> SplitPayload(const std::string& payload) {
  std::vector<std::u32string> out;
  size_t start = 0;
  while (start <= payload.size()) {
    size_t end = payload.find('\t', start);
    if (end == std::string::npos) end = payload.size();
    out.push_back(hodion::utf::from_utf8(payload.substr(start, end - start)));
    start = end + 1;
  }
  return out;
}

// Kiểu đặt dấu là thứ duy nhất của cấu hình ảnh hưởng tới chữ sinh ra.
// Đọc từ bản chụp không khoá — RefreshTray ghi, luồng phục vụ đọc, sai
// nhịp một lần thì cùng lắm là một từ ra kiểu dấu cũ.
hodion::Config HostConfig(const Host& host) {
  hodion::Config cfg;
  cfg.tone_style = host.serverVietnameseStyleModern
                       ? hodion::ToneStyle::Modern
                       : hodion::ToneStyle::Traditional;
  return cfg;
}

// Chạy trên luồng của HostServer.
std::string HandleRequest(hodionipc::Op op, const std::string& payload) {
  switch (op) {
    case hodionipc::Op::Ping:
      return "HodionKey host 1";

    case hodionipc::Op::Candidates: {
      Host* host = g_host;
      if (!host || host->syllables.empty()) return {};
      const hodion::Config cfg = HostConfig(*host);

      const std::vector<std::u32string> fields = SplitPayload(payload);
      if (fields.empty() || fields[0].empty()) return {};
      const std::vector<std::u32string> left(fields.begin() + 1, fields.end());

      std::string out;
      for (const std::u32string& c :
           hodion::rank_candidates(left, fields[0], cfg, host->syllables,
                                   host->model)) {
        if (!out.empty()) out += '\t';
        out += hodion::utf::to_utf8(c);
      }
      return out;
    }

    case hodionipc::Op::Restore: {
      Host* host = g_host;
      // Bảng nạp xong TRƯỚC khi mở kênh và không đổi nữa, nên luồng phục vụ
      // đọc thoải mái mà không cần khoá.
      if (!host || host->syllables.empty()) return {};
      const hodion::Config cfg = HostConfig(*host);

      const std::vector<std::u32string> fields = SplitPayload(payload);
      if (fields.empty() || fields[0].empty()) return {};
      const std::u32string& word = fields[0];

      // Chữ chỉ có MỘT cách viết thì không cần mô hình — trả lời chắc chắn.
      std::u32string answer =
          hodion::restore_diacritics(word, cfg, host->syllables);
      if (answer.empty() && !host->model.empty()) {
        const std::vector<std::u32string> left(fields.begin() + 1,
                                               fields.end());
        answer = hodion::restore_in_context(left, word, cfg, host->syllables,
                                            host->model, host->serverMargin);
      }
      return hodion::utf::to_utf8(answer);
    }

    default:
      return {};
  }
}

std::wstring Tooltip(const HodionSettings& s) {
  std::wstring tip = s.vietnamese_on ? L"HodionKey — đang gõ tiếng Việt"
                                     : L"HodionKey — đang tắt (gõ thẳng)";
  tip += s.engine.method == hodion::InputMethod::Vni ? L"\nVNI" : L"\nTelex";
  if (s.auto_diacritics) tip += L" · tự thêm dấu";
  return tip;
}

void RefreshTray(Host& host) {
  host.serverVietnameseStyleModern =
      host.settings.engine.tone_style == hodion::ToneStyle::Modern;
  host.serverMargin = static_cast<float>(host.settings.predict_margin) / 10.0f;
  host.tray.SetState(host.settings.vietnamese_on, Tooltip(host.settings));
}

// Nạp lại cấu hình từ registry. Phím chuyển Việt/Anh nằm trong DLL và ghi
// thẳng vào registry, nên đây là đường duy nhất để icon khay khớp với
// trạng thái thật.
void ReloadSettings(Host& host) {
  host.settings = LoadHodionSettings();
  RefreshTray(host);
}

void SaveAndRefresh(Host& host) {
  SaveHodionSettings(host.settings);
  // Ghi registry sẽ đánh thức watcher của chính ta; nuốt lượt báo đó đi.
  host.watcher.poll();
  RefreshTray(host);
}

// Ghép phím tắt vào nhãn menu. Menu Windows canh phải phần sau dấu tab,
// nên đây cũng là chỗ người dùng khám phá ra các phím tắt.
std::wstring WithKey(const WCHAR* label, const ToggleKey& key) {
  std::wstring text = label;
  if (key.enabled()) {
    text += L'\t';
    text += HodionDescribeKey(key);
  }
  return text;
}

void ShowMenu(Host& host) {
  HMENU menu = CreatePopupMenu();
  if (!menu) return;

  const HodionSettings& s = host.settings;
  const bool vn = s.vietnamese_on;
  const bool telex = s.engine.method == hodion::InputMethod::Telex;

  AppendMenuW(menu, MF_STRING | (vn ? MF_CHECKED : 0), kMenuVietnamese,
              WithKey(L"Gõ tiếng &Việt", s.toggle).c_str());
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING | (telex ? MF_CHECKED : 0), kMenuTelex,
              WithKey(L"&Telex", s.method_key).c_str());
  AppendMenuW(menu, MF_STRING | (!telex ? MF_CHECKED : 0), kMenuVni,
              WithKey(L"V&NI", s.method_key).c_str());
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  const bool canPredict = host.server.running() && !host.syllables.empty();
  AppendMenuW(
      menu,
      MF_STRING | (s.auto_diacritics ? MF_CHECKED : 0) |
          (canPredict ? 0 : MF_GRAYED),
      kMenuAutoDiacritics,
      canPredict
          ? WithKey(L"Tự thêm &dấu cho chữ không dấu", s.predict_key).c_str()
          : L"Tự thêm &dấu — thiếu viet-syllables.txt");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, kMenuConfig, L"&Cấu hình…");
  AppendMenuW(menu, MF_STRING | (HodionGetAutoStart() ? MF_CHECKED : 0),
              kMenuAutoStart, L"&Khởi động cùng Windows");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, kMenuQuit, L"T&hoát");

  POINT pt;
  GetCursorPos(&pt);
  // Nghi thức bắt buộc của menu khay: không đưa cửa sổ lên trước thì menu
  // sẽ không tự đóng khi người dùng bấm ra ngoài.
  SetForegroundWindow(host.window);
  TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, host.window, nullptr);
  PostMessageW(host.window, WM_NULL, 0, 0);
  DestroyMenu(menu);
}

void OpenConfig(Host& host) {
  if (host.inDialog) return;  // đã mở rồi
  host.inDialog = true;
  HodionShowConfigDialog(host.instance, host.window);
  host.inDialog = false;
  ReloadSettings(host);
}

void OnCommand(Host& host, int id) {
  switch (id) {
    case kMenuVietnamese:
      host.settings.vietnamese_on = !host.settings.vietnamese_on;
      SaveHodionVietnameseOn(host.settings.vietnamese_on);
      host.watcher.poll();
      RefreshTray(host);
      break;
    case kMenuTelex:
    case kMenuVni:
      host.settings.engine.method = id == kMenuTelex
                                        ? hodion::InputMethod::Telex
                                        : hodion::InputMethod::Vni;
      SaveAndRefresh(host);
      break;
    case kMenuAutoDiacritics:
      host.settings.auto_diacritics = !host.settings.auto_diacritics;
      SaveAndRefresh(host);
      break;
    case kMenuConfig:
      OpenConfig(host);
      break;
    case kMenuAutoStart:
      HodionSetAutoStart(!HodionGetAutoStart());
      break;
    case kMenuQuit:
      DestroyWindow(host.window);
      break;
    default:
      break;
  }
}

LRESULT CALLBACK HostWndProc(HWND hwnd, UINT msg, WPARAM wParam,
                             LPARAM lParam) {
  Host* host = g_host;
  if (!host) return DefWindowProcW(hwnd, msg, wParam, lParam);

  if (msg == g_showMessage && g_showMessage != 0) {
    OpenConfig(*host);
    return 0;
  }
  if (host->tray.IsTaskbarCreatedMessage(msg)) {
    host->tray.Restore();
    RefreshTray(*host);
    return 0;
  }

  switch (msg) {
    case WM_TRAY_CALLBACK:
      switch (LOWORD(lParam)) {
        case NIN_SELECT:
        case NIN_KEYSELECT:
          // Bấm trái = bật/tắt tiếng Việt, giống thói quen dùng UniKey.
          //
          // KHÔNG bắt bấm đúp để mở cấu hình: shell gửi NIN_SELECT trước cả
          // WM_LBUTTONDBLCLK, nên bấm đúp sẽ vừa đổi chế độ vừa mở hộp
          // thoại. Cấu hình mở từ menu chuột phải.
          OnCommand(*host, kMenuVietnamese);
          return 0;
        case WM_CONTEXTMENU:
        case WM_RBUTTONUP:
          ShowMenu(*host);
          return 0;
        default:
          return 0;
      }

    case WM_COMMAND:
      OnCommand(*host, LOWORD(wParam));
      return 0;

    case WM_SETTINGCHANGE:
    case WM_DPICHANGED:
      // Cỡ icon khay đổi theo DPI — dựng lại cho khỏi nhòe.
      host->tray.Destroy();
      host->tray.Create(hwnd, WM_TRAY_CALLBACK, host->instance);
      RefreshTray(*host);
      return 0;

    case WM_CLOSE:
      DestroyWindow(hwnd);
      return 0;

    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;

    default:
      return DefWindowProcW(hwnd, msg, wParam, lParam);
  }
}

// Cửa sổ ẩn nhưng là cửa sổ cấp cao nhất thật, KHÔNG phải HWND_MESSAGE:
// cửa sổ message-only không nhận được message quảng bá, mà ta cần đúng hai
// cái đó — "TaskbarCreated" của Explorer và message đánh thức của phiên bản
// thứ hai.
HWND CreateHostWindow(HINSTANCE instance) {
  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = HostWndProc;
  wc.hInstance = instance;
  wc.lpszClassName = kWindowClass;
  wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP));
  if (!RegisterClassExW(&wc)) return nullptr;

  return CreateWindowExW(0, kWindowClass, L"HodionKey", WS_OVERLAPPED, 0, 0,
                         0, 0, nullptr, nullptr, instance, nullptr);
}

bool WantsTrayOnly(LPWSTR commandLine) {
  return commandLine && wcsstr(commandLine, L"--tray") != nullptr;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR commandLine, int) {
  g_showMessage = RegisterWindowMessageW(kShowMessageName);

  HANDLE mutex = CreateMutexW(nullptr, TRUE, kMutexName);
  if (mutex && GetLastError() == ERROR_ALREADY_EXISTS) {
    // Đã có host đang chạy — nhờ nó mở hộp thoại rồi thoát.
    if (g_showMessage != 0) {
      SendMessageTimeoutW(HWND_BROADCAST, g_showMessage, 0, 0,
                          SMTO_ABORTIFHUNG, 200, nullptr);
    }
    CloseHandle(mutex);
    return 0;
  }

  INITCOMMONCONTROLSEX icc;
  icc.dwSize = sizeof(icc);
  icc.dwICC = ICC_STANDARD_CLASSES | ICC_HOTKEY_CLASS;
  InitCommonControlsEx(&icc);

  Host host;
  host.instance = instance;
  g_host = &host;

  host.window = CreateHostWindow(instance);
  if (!host.window) return 1;

  host.settings = LoadHodionSettings();
  host.watcher.start();
  LoadLanguageData(host);  // phải xong TRƯỚC khi mở kênh
  // Kênh cho DLL. Mở không được cũng không sao — chỉ là không có phần đoán
  // dấu; mọi thứ khác vẫn chạy.
  host.server.Start(HandleRequest);
  if (!host.tray.Create(host.window, WM_TRAY_CALLBACK, instance)) {
    // Không có khay hệ thống (phiên Server Core, shell lạ): vẫn mở được
    // hộp thoại cấu hình rồi thoát, chứ không im lặng chạy vô hình.
    HodionShowConfigDialog(instance, nullptr);
    return 0;
  }
  RefreshTray(host);

  // Chạy tay thì mở luôn hộp thoại; mục khởi động cùng Windows truyền
  // --tray để nó nằm im ở khay.
  if (!WantsTrayOnly(commandLine)) OpenConfig(host);

  // Vòng lặp thông điệp có thêm một tay chờ: sự kiện đổi registry. Nhờ vậy
  // icon khay bám theo phím chuyển Việt/Anh bấm trong ứng dụng khác mà
  // không cần hỏi registry theo nhịp.
  HANDLE waits[1] = {host.watcher.event()};
  const DWORD waitCount = waits[0] ? 1 : 0;
  for (;;) {
    const DWORD r = MsgWaitForMultipleObjects(waitCount, waits, FALSE,
                                              INFINITE, QS_ALLINPUT);
    if (waitCount == 1 && r == WAIT_OBJECT_0) {
      if (host.watcher.poll()) ReloadSettings(host);
      continue;
    }

    MSG msg;
    bool quit = false;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) {
        quit = true;
        break;
      }
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
    if (quit) break;
  }

  host.server.Stop();
  host.tray.Destroy();
  host.watcher.stop();
  g_host = nullptr;
  if (mutex) {
    ReleaseMutex(mutex);
    CloseHandle(mutex);
  }
  return 0;
}
