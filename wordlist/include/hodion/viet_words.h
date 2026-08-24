// Bảng âm tiết tiếng Việt CÓ THẬT — nạp lúc chạy, không nằm trong mã nguồn.
//
// Engine biết một âm tiết có ghép đúng luật hay không; nó không biết âm tiết
// đó có phải chữ người ta dùng hay không. Đó là kiến thức từ vựng, phải lấy
// từ một cuốn từ điển.
//
// KHÔNG có bảng nào được biên dịch sẵn vào đây, và đó là lựa chọn có chủ ý:
// từ điển chính tả tiếng Việt sẵn có (gói hunspell-vi của LibreOffice) là
// **GPL-2**, đưa dữ liệu dẫn xuất từ nó vào đây sẽ kéo giấy phép đó lên cả
// dự án. Thay vào đó, người dùng tự sinh file bằng tools/build_syllables.py
// trên máy mình và đặt cạnh exe. Không có file thì phần đoán dấu đơn giản
// là không bật được — mọi thứ còn lại chạy như thường.
//
// Cùng lý do đó, đây là nơi cắm mô hình tốt hơn sau này: đổi file, không
// phải dịch lại.
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

 private:
  std::vector<std::u32string> items_;  // đã sắp xếp
};

}  // namespace hodion
