// Máy trạng thái một "từ" đang gõ — mirror hành vi UniKey:
// mỗi ô (cell) là một ký tự hiển thị kèm snapshot cấu trúc âm tiết của từ
// tính đến ô đó, nhờ vậy Backspace chỉ việc bỏ ô cuối.
#pragma once

#include <string>
#include <vector>

#include "hodion/engine.h"
#include "vnlexi.h"

namespace hodion::detail {

// Dạng của từ tính đến một ô: rỗng, không phải tiếng Việt, C (chỉ phụ âm
// đầu), V (chỉ vần), CV, VC, CVC.
enum class Form : uint8_t { NonVn, C, V, CV, VC, CVC };

struct Cell {
  char32_t key = 0;        // phím gốc (hiển thị ký tự nonVn, khôi phục từ)
  char32_t base = 0;       // chữ cái gốc lowercase; 0 nếu không phải chữ
  Mark mark = Mark::None;
  bool upper = false;
  ToneId tone = 0;         // dấu thanh nằm trên ô này (0 nếu không)
  bool from_w = false;     // ư sinh từ phím w đơn (Telex)

  Form form = Form::NonVn;
  int16_t vs = kNoSeq;     // vần kết thúc tại ô này (ô nguyên âm)
  int16_t cs = kNoSeq;     // chuỗi phụ âm: onset khi form C, coda khi VC/CVC
  int16_t v_end = -1;      // chỉ số ô nguyên âm cuối của từ (-1 nếu chưa có)
  int16_t onset_end = -1;  // chỉ số ô cuối của phụ âm đầu (-1 nếu không có)

  VGlyph glyph() const { return {base, mark}; }
};

enum class HookKind : uint8_t { All, HornOnly, BreveOnly };

// Kết quả xử lý một phím trong từ.
enum class KeyOutcome : uint8_t {
  Applied,       // phím gây biến đổi (dấu, mũ, móc, đ, thanh…)
  Appended,      // phím được nối vào từ như ký tự thường
  Reverted,      // gõ lặp → hủy biến đổi cũ và nối phím dưới dạng ký tự thường
  NotApplicable, // không áp được và KHÔNG tự nối (chỉ dùng nội bộ cho w)
};

class Word {
 public:
  void clear() {
    cells_.clear();
    single_mode_ = false;
  }
  bool empty() const { return cells_.empty(); }
  size_t size() const { return cells_.size(); }
  const std::vector<Cell>& cells() const { return cells_; }
  bool single_mode() const { return single_mode_; }

  std::u32string render() const;

  // Các sự kiện phím (mirror ukengine, cài đặt lại):
  KeyOutcome roof(char32_t key, char32_t target /*0 = mọi nguyên âm*/,
                  const Config& cfg);
  KeyOutcome hook(char32_t key, HookKind kind, const Config& cfg);
  KeyOutcome tone(char32_t key, ToneId t, const Config& cfg);
  KeyOutcome stroke_d(char32_t key, const Config& cfg);
  KeyOutcome telex_w(char32_t key, const Config& cfg);
  KeyOutcome map_char(char32_t key, char32_t sym, bool set_from_w,
                      const Config& cfg);
  void append(char32_t key, const Config& cfg);

  // Xử lý sau sự kiện: từ hỏng chính tả + không bật kiểm tra (hoặc từ bắt đầu
  // bằng đ) → coi ký tự vừa gõ là đầu từ mới.
  void maybe_restart_word(const Config& cfg);

  void backspace(const Config& cfg);

  // Kiểm tra các bất biến nội bộ của bộ ô (chỉ dùng cho test/fuzz): chỉ số
  // liên kết hợp lệ, dấu đặt đúng chỗ, glyph khớp bảng vần, mỗi âm tiết
  // nhiều nhất một dấu thanh và thanh không nằm ngoài vần.
  bool check_invariants() const;

  // Từ hiện tại không phải tiếng Việt hợp lệ? (dùng cho khôi phục phím)
  bool is_non_vn(const Config& cfg) const;
  // Từ có mang dấu tiếng Việt (thanh/mũ/móc/đ)?
  bool has_vn_mark() const;

 private:
  friend struct WordOps;

  std::vector<Cell> cells_;
  bool single_mode_ = false;
  // Ký tự nối gần nhất có thuộc bảng chữ tiếng Việt không (phục vụ glue).
  bool glue_class_vn_ = false;
};

}  // namespace hodion::detail
