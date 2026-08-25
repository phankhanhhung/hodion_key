// Bảng âm tiết tiếng Việt CÓ THẬT — nạp lúc chạy, không nằm trong mã nguồn.
//
// Engine biết một âm tiết có ghép đúng luật hay không; nó không biết âm tiết
// đó có phải chữ người ta dùng hay không. Đó là kiến thức từ vựng, phải lấy
// từ một cuốn từ điển.
//
// Bảng KHÔNG được biên dịch sẵn vào đây mà nạp lúc chạy, và đó là lựa chọn
// có chủ ý: đổi bảng — hay sau này thay bằng một nguồn từ vựng tốt hơn —
// không phải dịch lại gì cả. Không có file thì phần đoán dấu đơn giản là
// không bật được; mọi thứ còn lại chạy như thường.
//
// Bảng phát hành kèm bản cài nằm ở wordlist/data/viet-syllables.txt, sinh
// từ kho văn bản Wikipedia lọc qua chính bộ luật gõ (tools/build_syllables.py).
// Bản đầu tiên lấy từ hunspell-vi nhưng từ điển đó là GPL-2 nên không phát
// hành kèm được — mà một bảng không giao được cho người dùng thì tính năng
// coi như không có.
#pragma once

#include <string>
#include <vector>

#include "hodion/engine.h"

namespace hodion {

class SyllableList final : public SyllableSet {
 public:
  // Nạp từ nội dung file UTF-8, mỗi dòng một âm tiết. Dòng trống và dòng
  // bắt đầu bằng '#' được bỏ qua.
  //
  // Nhận NỘI DUNG chứ không nhận đường dẫn: đọc file là việc của tầng host,
  // mỗi hệ điều hành xử lý đường dẫn một kiểu, còn tầng này thì portable.
  bool load(const std::string& utf8_contents);

  bool empty() const { return items_.empty(); }
  size_t size() const { return items_.size(); }

  bool contains(const std::u32string& syllable) const override;

  // Đọc thẳng danh sách (đã sắp xếp). Dùng để kiểm chính bảng: mọi mục
  // phải là chữ engine gõ ra được.
  const std::vector<std::u32string>& items() const { return items_; }

 private:
  std::vector<std::u32string> items_;  // đã sắp xếp
};

}  // namespace hodion
