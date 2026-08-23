// HodionKey — lõi bộ gõ tiếng Việt (portable core).
//
// Tầng này KHÔNG phụ thuộc bất kỳ API hệ điều hành nào: chỉ C++17 chuẩn.
// Mọi frontend (Windows TSF, macOS IMKit, Linux fcitx5/ibus, CLI…) đều
// nói chuyện với engine qua đúng một giao diện: đẩy từng phím vào,
// nhận về hành động (đang ghép vần / chốt chữ / bỏ qua) kèm chuỗi kết quả.
//
// Mô hình: engine giữ nguyên chuỗi phím thô (raw keystrokes) của "từ"
// đang gõ và tái dựng chuỗi hiển thị sau mỗi phím. Cách này làm cho các
// luật hủy dấu (aa→â, aaa→aa; as→á, ass→as…) và Backspace (hoàn tác
// đúng một phím) luôn nhất quán, không tích lũy trạng thái sai.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace hodion {

enum class InputMethod : uint8_t {
  Telex = 0,
  Vni = 1,
};

// Vị trí dấu thanh cho các vần oa/oe/uy không có âm cuối:
//   Traditional (kiểu cũ):  hòa, khỏe, thúy
//   Modern (kiểu mới):      hoà, khoẻ, thuý
enum class ToneStyle : uint8_t {
  Traditional = 0,
  Modern = 1,
};

struct Config {
  InputMethod method = InputMethod::Telex;
  ToneStyle tone_style = ToneStyle::Traditional;
  bool w_shorthand = true;  // Telex: phím w đứng một mình → ư
  bool delayed_d = true;    // Telex: phím d ở cuối từ vẫn biến d đầu từ thành đ (dun + d → đun)
};

class Engine {
 public:
  struct Result {
    enum class Action : uint8_t {
      None,       // engine không xử lý phím này — host cho phím đi thẳng tới ứng dụng
      Composing,  // text = nội dung composition mới (có thể rỗng sau Backspace)
      Commit,     // text = chuỗi cần chốt (đã bao gồm ký tự ngắt từ nếu có); engine đã tự reset
    };
    Action action = Action::None;
    std::u32string text;
  };

  Engine() = default;
  explicit Engine(const Config& cfg) : cfg_(cfg) {}

  const Config& config() const { return cfg_; }
  void set_config(const Config& cfg);  // đổi cấu hình sẽ reset trạng thái gõ dở

  // Một ký tự in được (đã phân giải case/shift ở tầng host).
  Result process_char(char32_t ch);
  // Phím Backspace: hoàn tác một phím gõ gần nhất trong từ đang ghép.
  Result process_backspace();

  bool composing() const { return !raw_.empty(); }
  std::u32string composition() const;  // chuỗi hiển thị hiện tại
  std::u32string raw() const;          // chuỗi phím thô (dùng cho Esc — khôi phục ASCII)
  std::u32string commit();             // trả về chuỗi hiển thị rồi reset (Enter, mất focus…)
  void reset() { raw_.clear(); }

 private:
  Result composing_result() const;

  Config cfg_{};
  std::vector<char32_t> raw_;
  // Chặn buffer phình vô hạn khi người dùng gõ chuỗi chữ cái rất dài
  // (URL, mã hash…): vượt ngưỡng thì tự chốt như một từ bình thường.
  static constexpr size_t kMaxRaw = 40;
};

}  // namespace hodion
