#include "Log.h"

#include <cstdio>
#include <string>

#include "Settings.h"

namespace {

// Đọc registry mỗi lần ghi thì hỏng đúng cái nó phải đo (chậm đi), nên
// nhớ lại; HodionLogRefresh() làm mới khi cấu hình đổi.
bool g_enabled = false;

// Chặn trên cho file log. Nhật ký bật lên là để bắt một lỗi cụ thể trong
// vài phút, không phải để chạy mãi — nhưng người dùng có thể quên tắt, và
// một file lớn dần vô hạn trong tiến trình của ứng dụng khác thì tệ hơn
// hẳn việc mất phần đầu nhật ký.
constexpr DWORD kMaxBytes = 4u * 1024 * 1024;

// Lấy thư mục qua biến môi trường chứ không qua SHGetKnownFolderPath: DLL
// này bị nạp vào tiến trình của người khác, càng ít kéo theo shell32 và COM
// càng tốt — nhất là khi chưa chắc COM đã được khởi tạo ở luồng đó.
std::wstring LogPath() {
  WCHAR base[MAX_PATH] = {};
  DWORD len = GetEnvironmentVariableW(L"LOCALAPPDATA", base, ARRAYSIZE(base));
  if (len == 0 || len >= ARRAYSIZE(base)) {
    len = GetEnvironmentVariableW(L"TEMP", base, ARRAYSIZE(base));
    if (len == 0 || len >= ARRAYSIZE(base)) return std::wstring();
  }
  std::wstring dir(base, len);
  dir += L"\\HodionKey";
  CreateDirectoryW(dir.c_str(), nullptr);
  return dir + L"\\hodionkey.log";
}

std::wstring ProcessName() {
  WCHAR path[MAX_PATH] = {};
  const DWORD len = GetModuleFileNameW(nullptr, path, ARRAYSIZE(path));
  if (len == 0 || len >= ARRAYSIZE(path)) return L"?";
  const std::wstring full(path, len);
  const size_t slash = full.find_last_of(L'\\');
  return slash == std::wstring::npos ? full : full.substr(slash + 1);
}

}  // namespace

void HodionLogRefresh() {
  g_enabled = false;
  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, kHodionSettingsKey, 0, KEY_QUERY_VALUE,
                    &key) != ERROR_SUCCESS) {
    return;
  }
  DWORD value = 0, size = sizeof(value), type = 0;
  if (RegQueryValueExW(key, L"Debug", nullptr, &type,
                       reinterpret_cast<BYTE*>(&value), &size) == ERROR_SUCCESS &&
      type == REG_DWORD) {
    g_enabled = value != 0;
  }
  RegCloseKey(key);
}

bool HodionLogEnabled() { return g_enabled; }

void HodionLogWrite(const WCHAR* format, ...) {
  if (!g_enabled) return;

  WCHAR body[1024];
  va_list args;
  va_start(args, format);
  const int written = _vsnwprintf_s(body, ARRAYSIZE(body), _TRUNCATE, format, args);
  va_end(args);
  if (written < 0) return;

  SYSTEMTIME now;
  GetLocalTime(&now);
  static const std::wstring exe = ProcessName();

  WCHAR line[1280];
  const int n = _snwprintf_s(
      line, ARRAYSIZE(line), _TRUNCATE, L"%02u:%02u:%02u.%03u %5lu %-16s %s\r\n",
      now.wHour, now.wMinute, now.wSecond, now.wMilliseconds,
      GetCurrentProcessId(), exe.c_str(), body);
  if (n <= 0) return;

  // Mở/đóng mỗi dòng: DLL này nằm trong MỌI ứng dụng đang gõ, giữ handle
  // mở là giữ khoá file suốt phiên. Ghi nối với FILE_SHARE_* đầy đủ để
  // nhiều tiến trình cùng ghi được, và đọc được trong lúc đang ghi.
  static const std::wstring path = LogPath();
  if (path.empty()) return;
  const HANDLE file = CreateFileW(
      path.c_str(), FILE_APPEND_DATA,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
      OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return;

  LARGE_INTEGER size = {};
  if (GetFileSizeEx(file, &size) && size.QuadPart > kMaxBytes) {
    CloseHandle(file);
    // Cắt về rỗng rồi ghi tiếp: giữ phần MỚI, vì lỗi vừa xảy ra mới là
    // thứ cần đọc.
    const HANDLE trunc = CreateFileW(
        path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, TRUNCATE_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (trunc != INVALID_HANDLE_VALUE) CloseHandle(trunc);
    return;
  }

  const std::string utf8 = [&] {
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, line, n, nullptr, 0,
                                          nullptr, nullptr);
    std::string out(bytes > 0 ? static_cast<size_t>(bytes) : 0, '\0');
    if (bytes > 0) {
      WideCharToMultiByte(CP_UTF8, 0, line, n, out.data(), bytes, nullptr,
                          nullptr);
    }
    return out;
  }();
  DWORD ignored = 0;
  if (!utf8.empty()) {
    WriteFile(file, utf8.data(), static_cast<DWORD>(utf8.size()), &ignored,
              nullptr);
  }
  CloseHandle(file);
}
