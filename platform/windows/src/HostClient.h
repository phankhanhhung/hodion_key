// Đầu DLL của kênh nói chuyện với tiến trình nền.
//
// Hợp đồng quan trọng nhất: **không bao giờ chờ lâu**. Host chưa chạy, đã
// tắt, hay đang treo thì hàm này trả về false gần như tức thì và bộ gõ đi
// tiếp như không có gì. Không có tính năng còn hơn gõ bị khựng.
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>

#include "HostChannel.h"

class HostClient {
 public:
  ~HostClient() { Close(); }

  // Gửi một yêu cầu và chờ trả lời, tối đa `timeout_ms`. false = không có
  // host, quá hạn, hoặc gói tin hỏng — mọi trường hợp đều là "cứ gõ tiếp".
  bool Request(hodionipc::Op op, const std::string& payload,
               std::string* reply, DWORD timeout_ms = 20);

  void Close();

 private:
  bool EnsureConnected();

  HANDLE pipe_ = INVALID_HANDLE_VALUE;
  HANDLE event_ = nullptr;
  // Host không chạy là chuyện thường (người dùng chưa mở). Thử kết nối mỗi
  // lần chốt từ thì phí, nên hỏng một lần là im một lúc.
  ULONGLONG quietUntil_ = 0;
};
