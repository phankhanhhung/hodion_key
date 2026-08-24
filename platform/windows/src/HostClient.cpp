#include "HostClient.h"

#include <vector>

namespace {

// Thử lại sau ngần này khi không kết nối được. Đủ lâu để không tốn gì, đủ
// ngắn để người dùng mở host lên là dùng được ngay mà không phải khởi động
// lại ứng dụng đang gõ.
constexpr ULONGLONG kQuietMs = 3000;

}  // namespace

void HostClient::Close() {
  if (pipe_ != INVALID_HANDLE_VALUE) {
    CloseHandle(pipe_);
    pipe_ = INVALID_HANDLE_VALUE;
  }
  if (event_) {
    CloseHandle(event_);
    event_ = nullptr;
  }
}

bool HostClient::EnsureConnected() {
  if (pipe_ != INVALID_HANDLE_VALUE) return true;
  if (GetTickCount64() < quietUntil_) return false;

  const std::wstring name = hodionipc::PipeName();
  if (name.empty()) {
    quietUntil_ = GetTickCount64() + kQuietMs;
    return false;
  }

  // KHÔNG dùng WaitNamedPipe: pipe bận thì bỏ qua luôn, thà mất một lần
  // đoán dấu còn hơn giữ chân người đang gõ.
  HANDLE pipe = CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                            nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED,
                            nullptr);
  if (pipe == INVALID_HANDLE_VALUE) {
    quietUntil_ = GetTickCount64() + kQuietMs;
    return false;
  }

  DWORD mode = PIPE_READMODE_MESSAGE;
  if (!SetNamedPipeHandleState(pipe, &mode, nullptr, nullptr)) {
    CloseHandle(pipe);
    quietUntil_ = GetTickCount64() + kQuietMs;
    return false;
  }

  if (!event_) {
    event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!event_) {
      CloseHandle(pipe);
      quietUntil_ = GetTickCount64() + kQuietMs;
      return false;
    }
  }
  pipe_ = pipe;
  return true;
}

bool HostClient::Request(hodionipc::Op op, const std::string& payload,
                         std::string* reply, DWORD timeout_ms) {
  if (!reply) return false;
  reply->clear();
  if (payload.size() > hodionipc::kMaxPayload) return false;
  if (!EnsureConnected()) return false;

  hodionipc::Header header;
  header.magic = hodionipc::kMagic;
  header.version = hodionipc::kVersion;
  header.op = static_cast<uint16_t>(op);
  header.length = static_cast<uint32_t>(payload.size());

  std::vector<char> request(sizeof(header) + payload.size());
  memcpy(request.data(), &header, sizeof(header));
  if (!payload.empty()) {
    memcpy(request.data() + sizeof(header), payload.data(), payload.size());
  }

  std::vector<char> buffer(hodionipc::kMaxMessage);
  OVERLAPPED ov = {};
  ov.hEvent = event_;
  ResetEvent(event_);

  DWORD read = 0;
  BOOL ok = TransactNamedPipe(pipe_, request.data(),
                              static_cast<DWORD>(request.size()),
                              buffer.data(),
                              static_cast<DWORD>(buffer.size()), &read, &ov);
  if (!ok) {
    if (GetLastError() != ERROR_IO_PENDING) {
      Close();
      return false;
    }
    if (WaitForSingleObject(event_, timeout_ms) != WAIT_OBJECT_0) {
      // Quá hạn. Huỷ rồi PHẢI đợi I/O kết thúc thật sự trước khi buffer ra
      // khỏi phạm vi, nếu không kernel còn đang ghi vào bộ nhớ đã chết.
      CancelIoEx(pipe_, &ov);
      GetOverlappedResult(pipe_, &ov, &read, TRUE);
      Close();  // trạng thái pipe không còn tin được
      return false;
    }
    if (!GetOverlappedResult(pipe_, &ov, &read, FALSE)) {
      Close();
      return false;
    }
  }

  if (read < sizeof(hodionipc::Header)) {
    Close();
    return false;
  }
  hodionipc::Header response;
  memcpy(&response, buffer.data(), sizeof(response));
  if (response.magic != hodionipc::kMagic ||
      response.version != hodionipc::kVersion ||
      response.length > read - sizeof(response)) {
    Close();
    return false;
  }
  reply->assign(buffer.data() + sizeof(response), response.length);
  return true;
}
