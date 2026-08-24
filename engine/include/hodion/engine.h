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

// Nguồn tra từ ngoại lai (tiếng Anh) do host cấp.
//
// Engine KHÔNG mang theo dữ liệu từ điển: nó chỉ biết hỏi. Nhờ vậy lõi vẫn
// build và chạy đúng khi không có từ điển nào, và mỗi nền tảng tự chọn
// cách nạp danh sách của mình.
class ForeignWords {
 public:
  virtual ~ForeignWords() = default;
  // `key` là chuỗi phím thô của từ, đã hạ về chữ thường, chỉ gồm a–z.
  virtual bool contains(const std::u32string& key) const = 0;
};

// Tập âm tiết tiếng Việt CÓ THẬT, do host cấp.
//
// Engine biết "duông" ghép đúng luật âm tiết, nhưng không biết nó có phải
// một chữ người ta dùng hay không — đó là kiến thức từ vựng, không suy ra
// được từ bảng vần. Ai muốn phần kiến thức đó thì nạp vào.
class SyllableSet {
 public:
  virtual ~SyllableSet() = default;
  // `syllable` là một âm tiết đã dựng sẵn, chữ thường.
  virtual bool contains(const std::u32string& syllable) const = 0;
};

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
  // Lúc chốt từ, nếu chuỗi phím thô là một từ tiếng Anh đã biết thì trả lại
  // nguyên chuỗi đó (test → "test" chứ không phải "tét"). Chỉ có tác dụng
  // khi host đã nạp từ điển bằng set_foreign_words().
  bool english_detect = true;
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

  // Nạp từ điển từ ngoại lai. Con trỏ phải sống lâu hơn engine; nullptr để
  // tắt. Chỉ được tra lúc CHỐT từ, không nằm trên đường gõ từng phím.
  void set_foreign_words(const ForeignWords* words);

  // Phím này có mở một từ mới không (khi chưa compose)?
  // Chữ cái luôn mở từ; [ ] { } mở từ ở Telex đầy đủ (thành ơ/ư).
  bool starts_word(char32_t ch) const;

  // Một ký tự in được (đã phân giải case/shift ở tầng host).
  Result process_char(char32_t ch);
  // Backspace: xóa một ký tự hiển thị (di chuyển dấu thanh nếu cần).
  Result process_backspace();

  // Hủy mọi biến đổi tiếng Việt của từ ĐANG gõ và chuyển sang gõ thẳng:
  // phần còn lại của từ này được nối nguyên văn, không phím nào là phím
  // dấu nữa. Dành cho từ tiếng Anh lọt giữa câu tiếng Việt ("deadline",
  // "test") — rẻ hơn nhiều so với tắt/bật lại chế độ tiếng Việt.
  //
  // Cấu hình không đổi: chốt xong từ này là gõ tiếng Việt lại như thường.
  // Trả về Action::None nếu không có gì đang gõ dở.
  Result cancel_transform();
  // Đang gõ thẳng (sau cancel_transform) — từ này không còn biến đổi nữa.
  bool literal() const;

  bool composing() const;
  // Từ đang gõ dở có cấu trúc âm tiết tiếng Việt hợp lệ không? (rỗng → false)
  bool composing_is_vietnamese() const;
  std::u32string composition() const;  // chuỗi hiển thị hiện tại
  // Chuỗi phím thô của từ, dùng cho Esc ("trả lại đúng chữ tao gõ"). Sau khi
  // người dùng bấm Backspace thì không dựng lại được nữa (một ký tự có thể
  // do nhiều phím tạo ra) nên hàm này trả về chính chuỗi đang hiển thị —
  // Esc khi đó chỉ kết thúc composition mà không đổi chữ.
  std::u32string raw() const;
  std::u32string commit();             // trả về chuỗi hiển thị rồi reset
  void reset();

  // Kiểm tra bất biến nội bộ — dành cho test/fuzz, không dùng trong bộ gõ.
  bool self_check() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace hodion
