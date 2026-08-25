// Đầu host của kênh nói chuyện với DLL text service.
//
// Vài luồng, mỗi luồng giữ một instance của named pipe và lặp
// nối → đọc → trả lời → ngắt. Không dùng thread pool động: số ứng dụng
// đang gõ cùng lúc là hữu hạn và yêu cầu thì siêu ngắn, nên một số luồng
// cố định vừa đủ mà lại không có gì để hỏng.
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <functional>
#include <string>
#include <thread>
#include <vector>

#include "HostChannel.h"

class HostServer {
 public:
  // Chạy trên luồng nền, phải an toàn khi gọi đồng thời.
  using Handler = std::function<std::string(hodionipc::Op,
                                            const std::string&)>;

  ~HostServer() { Stop(); }

  // false = không mở được kênh (không lấy được SID, hoặc đã có host khác).
  // Đây KHÔNG phải lỗi chí mạng: bộ gõ vẫn gõ được, chỉ là không có phần
  // đoán dấu.
  bool Start(Handler handler);
  void Stop();
  bool running() const { return !threads_.empty(); }

 private:
  void ServeLoop();

  Handler handler_;
  HANDLE stopEvent_ = nullptr;
  std::vector<std::thread> threads_;
};
