// Đầu DLL của kênh nói chuyện với tiến trình nền.
//
// Hợp đồng quan trọng nhất: **không bao giờ chờ lâu**. Host chưa chạy, đã
// tắt, hay đang treo thì hàm này trả về false gần như tức thì và bộ gõ đi
// tiếp như không có gì. Không có tính năng còn hơn gõ bị khựng.
//
// "Đang treo" là ca đáng lo nhất và nó cần xử lý riêng: hạn cứng thôi thì
// chưa đủ. Nếu host treo mà mỗi từ ta lại thử lại rồi chờ hết hạn thì mỗi
// lần chốt từ tốn đúng bằng hạn đó — bộ gõ trở nên ì hẳn dù về lý thuyết
// vẫn "không chờ lâu". Nên hỏng lần nào là im một lúc, và im lâu hơn khi
// hỏng vì quá hạn.
//
// **Mỗi yêu cầu là một kết nối riêng, mở rồi đóng ngay.** Giữ kết nối cho
// cả phiên nghe thì nhanh hơn, nhưng named pipe có số instance hữu hạn:
// DLL này nằm trong MỌI ứng dụng đang gõ, nên nếu mỗi ứng dụng chiếm một
// instance vĩnh viễn thì ứng dụng thứ N+1 trở đi không bao giờ nối được —
// im lặng, vĩnh viễn, và phụ thuộc vào thứ tự mở ứng dụng. Đổi lại chỉ tốn
// vài chục micro giây mỗi lần mở, mà người ta thì chỉ gõ vào một ứng dụng
// tại một thời điểm nên gần như không bao giờ có hai yêu cầu chồng nhau.
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

  // Chỉ dùng cho test: còn bao lâu nữa mới thử lại (0 = sẵn sàng).
  ULONGLONG quiet_remaining_ms() const;

 private:
  bool EnsureConnected();
  void ClosePipe();  // đóng kết nối, giữ lại event dùng cho lần sau
  void Backoff(ULONGLONG ms);

  HANDLE pipe_ = INVALID_HANDLE_VALUE;
  HANDLE event_ = nullptr;
  // Host không chạy là chuyện thường (người dùng chưa mở). Thử kết nối mỗi
  // lần chốt từ thì phí, nên hỏng một lần là im một lúc.
  ULONGLONG quietUntil_ = 0;
};
