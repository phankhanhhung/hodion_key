#include "HostServer.h"

#include <vector>

namespace {

// Client mở kết nối rồi đóng ngay sau mỗi yêu cầu (xem HostClient.h), nên
// số này chỉ cần đủ cho các yêu cầu CHỒNG NHAU chứ không phải cho số ứng
// dụng đang mở. Người ta chỉ gõ vào một ứng dụng tại một thời điểm, nên
// thực tế gần như không bao giờ quá một.
constexpr int kInstances = 4;

}  // namespace

bool HostServer::Start(Handler handler) {
  Stop();
  handler_ = std::move(handler);

  const std::wstring name = hodionipc::PipeName();
  if (name.empty()) return false;

  stopEvent_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  if (!stopEvent_) return false;

  for (int i = 0; i < kInstances; ++i) {
    threads_.emplace_back([this] { ServeLoop(); });
  }
  return true;
}

void HostServer::Stop() {
  if (stopEvent_) SetEvent(stopEvent_);
  for (std::thread& t : threads_) {
    if (t.joinable()) t.join();
  }
  threads_.clear();
  if (stopEvent_) {
    CloseHandle(stopEvent_);
    stopEvent_ = nullptr;
  }
}

void HostServer::ServeLoop() {
  const std::wstring name = hodionipc::PipeName();

  PSECURITY_DESCRIPTOR sd = nullptr;
  if (!hodionipc::MakePipeSecurity(&sd)) return;  // thà không có kênh
  SECURITY_ATTRIBUTES sa = {};
  sa.nLength = sizeof(sa);
  sa.lpSecurityDescriptor = sd;
  sa.bInheritHandle = FALSE;

  std::vector<char> buffer(hodionipc::kMaxMessage);

  while (WaitForSingleObject(stopEvent_, 0) != WAIT_OBJECT_0) {
    HANDLE pipe = CreateNamedPipeW(
        name.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, kInstances,
        hodionipc::kMaxMessage, hodionipc::kMaxMessage, 0, &sa);
    if (pipe == INVALID_HANDLE_VALUE) break;

    HANDLE event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!event) {
      CloseHandle(pipe);
      break;
    }

    OVERLAPPED ov = {};
    ov.hEvent = event;
    BOOL connected = ConnectNamedPipe(pipe, &ov);
    if (!connected) {
      const DWORD err = GetLastError();
      if (err == ERROR_IO_PENDING) {
        HANDLE waits[2] = {stopEvent_, event};
        if (WaitForMultipleObjects(2, waits, FALSE, INFINITE) ==
            WAIT_OBJECT_0) {
          CancelIoEx(pipe, &ov);
          DWORD ignored = 0;
          GetOverlappedResult(pipe, &ov, &ignored, TRUE);
          CloseHandle(event);
          CloseHandle(pipe);
          break;
        }
        connected = TRUE;
      } else if (err == ERROR_PIPE_CONNECTED) {
        connected = TRUE;  // client nhanh tay nối trước cả ConnectNamedPipe
      }
    }

    // Vòng lặp vẫn phục vụ nhiều yêu cầu trên một kết nối nếu client muốn
    // vậy; client hiện tại đóng sau mỗi yêu cầu nên nó thoát ngay vòng đầu.
    while (connected && WaitForSingleObject(stopEvent_, 0) != WAIT_OBJECT_0) {
      ResetEvent(event);
      DWORD read = 0;
      OVERLAPPED rov = {};
      rov.hEvent = event;
      if (!ReadFile(pipe, buffer.data(), static_cast<DWORD>(buffer.size()),
                    &read, &rov)) {
        if (GetLastError() != ERROR_IO_PENDING) break;
        HANDLE waits[2] = {stopEvent_, event};
        if (WaitForMultipleObjects(2, waits, FALSE, INFINITE) ==
            WAIT_OBJECT_0) {
          CancelIoEx(pipe, &rov);
          DWORD ignored = 0;
          GetOverlappedResult(pipe, &rov, &ignored, TRUE);
          break;
        }
        if (!GetOverlappedResult(pipe, &rov, &read, FALSE)) break;
      }
      if (read < sizeof(hodionipc::Header)) break;

      hodionipc::Header header;
      memcpy(&header, buffer.data(), sizeof(header));
      // Gói tin không đúng khuôn: ngắt, không đoán và không trả lời.
      if (header.magic != hodionipc::kMagic ||
          header.version != hodionipc::kVersion ||
          header.length > read - sizeof(header)) {
        break;
      }

      const std::string payload(buffer.data() + sizeof(header), header.length);
      std::string result = handler_
                               ? handler_(static_cast<hodionipc::Op>(header.op),
                                          payload)
                               : std::string();
      if (result.size() > hodionipc::kMaxPayload) result.clear();

      hodionipc::Header reply = header;
      reply.length = static_cast<uint32_t>(result.size());
      std::vector<char> out(sizeof(reply) + result.size());
      memcpy(out.data(), &reply, sizeof(reply));
      if (!result.empty()) {
        memcpy(out.data() + sizeof(reply), result.data(), result.size());
      }

      ResetEvent(event);
      DWORD written = 0;
      OVERLAPPED wov = {};
      wov.hEvent = event;
      if (!WriteFile(pipe, out.data(), static_cast<DWORD>(out.size()),
                     &written, &wov)) {
        if (GetLastError() != ERROR_IO_PENDING) break;
        HANDLE waits[2] = {stopEvent_, event};
        if (WaitForMultipleObjects(2, waits, FALSE, INFINITE) ==
            WAIT_OBJECT_0) {
          CancelIoEx(pipe, &wov);
          DWORD ignored = 0;
          GetOverlappedResult(pipe, &wov, &ignored, TRUE);
          break;
        }
        if (!GetOverlappedResult(pipe, &wov, &written, FALSE)) break;
      }
    }

    FlushFileBuffers(pipe);
    DisconnectNamedPipe(pipe);
    CloseHandle(event);
    CloseHandle(pipe);
  }

  LocalFree(sd);
}
