// Giao thức giữa DLL text service và tiến trình nền.
//
// Dùng chung cho cả hai đầu, nên mọi thứ về khuôn dạng gói tin nằm ở đây và
// chỉ ở đây.
//
// Nguyên tắc: kênh này CHỞ CHỮ NGƯỜI DÙNG ĐANG GÕ. Vì vậy named pipe mang
// tên gắn SID của người dùng và có DACL chỉ cho chính họ cộng SYSTEM — tiến
// trình khác trên cùng máy không được đọc, cũng không được giả làm host để
// nhét chữ vào.
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdint>
#include <string>

namespace hodionipc {

// "HKI" + số phiên bản. Sai magic là ngắt kết nối ngay, không đoán.
constexpr uint32_t kMagic = 0x314948;
constexpr uint16_t kVersion = 1;

enum class Op : uint16_t {
  Ping = 1,     // payload rỗng; trả về chuỗi mô tả phiên bản host
  Restore = 2,  // payload = từ không dấu; trả về từ có dấu, rỗng = chịu
};

#pragma pack(push, 1)
struct Header {
  uint32_t magic;
  uint16_t version;
  uint16_t op;
  uint32_t length;  // số byte payload theo sau
};
#pragma pack(pop)

// Gói tin phải vừa một message của named pipe. Một âm tiết dài nhất 8 ký
// tự; giới hạn rộng rãi này là để sau còn chở được ngữ cảnh cả câu.
constexpr uint32_t kMaxPayload = 4096;
constexpr uint32_t kMaxMessage =
    static_cast<uint32_t>(sizeof(Header)) + kMaxPayload;

// Tên pipe gắn SID người dùng: hai người cùng đăng nhập một máy không đụng
// nhau. Trả về chuỗi rỗng nếu không lấy được SID.
std::wstring PipeName();

// Security descriptor cho pipe: chỉ chủ sở hữu và SYSTEM. Người gọi phải
// LocalFree(sd) sau khi dùng xong. Trả về false thì KHÔNG được tạo pipe với
// bảo mật mặc định — thà không có tính năng còn hơn mở cửa cho tiến trình
// khác đọc chữ người dùng gõ.
bool MakePipeSecurity(PSECURITY_DESCRIPTOR* sd);

}  // namespace hodionipc
