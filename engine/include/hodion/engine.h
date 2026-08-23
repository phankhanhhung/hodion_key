// HodionKey — lõi bộ gõ tiếng Việt (portable core).
//
// Tầng này KHÔNG phụ thuộc bất kỳ API hệ điều hành nào: chỉ C++17 chuẩn.
// Mọi frontend (Windows TSF, macOS IMKit, Linux fcitx5/ibus, CLI…) nói
// chuyện với engine qua một giao diện: đẩy từng phím, nhận hành động
// (đang ghép / chốt / bỏ qua) kèm chuỗi kết quả.
//
// Bộ luật gõ tương thích hành vi UniKey (nghiên cứu từ tài liệu ukengine,
// cài đặt lại từ đầu): kiểm tra chính tả cấu trúc âm tiết, gõ dấu tự do
// (dấu ở cuối từ), luật hủy khi gõ lặp, họ vần uo/ưo/uơ/ươ, gi/qu…
#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace hodion {

enum class InputMethod : uint8_t {
  Telex = 0,
  Vni = 1,
};

// Vị trí dấu thanh cho các vần oa/oe/uy không có âm cuối:
//   Traditional (kiểu cũ):  hòa, khỏe, thúy   — mặc định như UniKey
//   Modern (kiểu mới):      hoà, khoẻ, thuý
enum class ToneStyle : uint8_t {
  Traditional = 0,
  Modern = 1,
};

// Mặc định trùng với mặc định của UniKey.
struct Config {
  InputMethod method = InputMethod::Telex;
  ToneStyle tone_style = ToneStyle::Traditional;
  bool free_marking = true;     // gõ dấu tự do (dấu ở cuối từ): viete→viêt, dund→đun
  bool spell_check = true;      // kiểm tra chính tả: từ sai ngừng biến đổi (did→did)
  bool restore_non_vn = false;  // tự khôi phục phím gõ với từ không phải tiếng Việt
  bool w_shorthand = true;      // Telex: w không áp được móc thì thành ư (tw→tư)
  bool telex_brackets = true;   // Telex đầy đủ: [ ] { } → ơ ư Ơ Ư
};

class Engine {
 public:
  struct Result {
    enum class Action : uint8_t {
      None,       // engine không xử lý — host cho phím đi thẳng tới ứng dụng
      Composing,  // text = nội dung composition mới (có thể rỗng sau Backspace)
      Commit,     // text = chuỗi cần chốt (kèm ký tự ngắt từ); engine đã reset
    };
    Action action = Action::None;
    std::u32string text;
  };

  Engine();
  explicit Engine(const Config& cfg);
  ~Engine();
  Engine(Engine&&) noexcept;
  Engine& operator=(Engine&&) noexcept;

  const Config& config() const;
  void set_config(const Config& cfg);  // đổi cấu hình sẽ reset trạng thái gõ dở

  // Phím này có mở một từ mới không (khi chưa compose)?
  // Chữ cái luôn mở từ; [ ] { } mở từ ở Telex đầy đủ (thành ơ/ư).
  bool starts_word(char32_t ch) const;

  // Một ký tự in được (đã phân giải case/shift ở tầng host).
  Result process_char(char32_t ch);
  // Backspace: xóa một ký tự hiển thị (di chuyển dấu thanh nếu cần).
  Result process_backspace();

  bool composing() const;
  std::u32string composition() const;  // chuỗi hiển thị hiện tại
  std::u32string raw() const;          // chuỗi phím thô của từ (Esc khôi phục)
  std::u32string commit();             // trả về chuỗi hiển thị rồi reset
  void reset();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace hodion
