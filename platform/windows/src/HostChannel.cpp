#include "HostChannel.h"

#include <sddl.h>

#include <vector>

namespace hodionipc {
namespace {

// Chuỗi SID của người dùng đang chạy tiến trình này.
std::wstring CurrentUserSid() {
  HANDLE token = nullptr;
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return {};

  DWORD size = 0;
  GetTokenInformation(token, TokenUser, nullptr, 0, &size);
  if (size == 0) {
    CloseHandle(token);
    return {};
  }
  std::vector<BYTE> buffer(size);
  std::wstring result;
  if (GetTokenInformation(token, TokenUser, buffer.data(), size, &size)) {
    LPWSTR text = nullptr;
    const TOKEN_USER* user = reinterpret_cast<TOKEN_USER*>(buffer.data());
    if (ConvertSidToStringSidW(user->User.Sid, &text)) {
      result = text;
      LocalFree(text);
    }
  }
  CloseHandle(token);
  return result;
}

}  // namespace

std::wstring PipeName() {
  const std::wstring sid = CurrentUserSid();
  if (sid.empty()) return {};
  return L"\\\\.\\pipe\\HodionKey." + sid;
}

bool MakePipeSecurity(PSECURITY_DESCRIPTOR* sd) {
  *sd = nullptr;
  const std::wstring sid = CurrentUserSid();
  if (sid.empty()) return false;

  // D:P = DACL tường minh, không kế thừa gì. GA = toàn quyền.
  const std::wstring sddl = L"D:P(A;;GA;;;" + sid + L")(A;;GA;;;SY)";
  return ConvertStringSecurityDescriptorToSecurityDescriptorW(
             sddl.c_str(), SDDL_REVISION_1, sd, nullptr) != FALSE;
}

}  // namespace hodionipc
