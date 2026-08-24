// Đoán dấu cho chữ tiếng Việt không dấu.
//
// Đây là chỗ dành cho phần "logic nặng" của bộ gõ, và hiện nó mới làm được
// đúng nửa dễ của bài toán: những âm tiết mà **chỉ có một** cách viết có
// dấu tồn tại thật ("nguyet" → "nguyệt"). Với âm tiết nhập nhằng ("toan" →
// toan/toàn/toán/toản) thì không có bằng chứng nào để chọn, và đoán bừa là
// kiểu hỏng tệ nhất — nên nó im lặng trả về rỗng.
//
// Đo trên từ điển 6.502 âm tiết: chỉ 15,6% chuỗi không dấu có đúng một cách
// viết. Nửa còn lại cần mô hình ngôn ngữ có ngữ cảnh (n-gram + Viterbi trên
// cả câu). Khi có, nó cắm vào đúng chỗ này.
#pragma once

#include <string>

#include "hodion/engine.h"

namespace hodion {

// Trả về dạng có dấu của `word`, hoặc chuỗi RỖNG khi không đủ bằng chứng.
// Rỗng nghĩa là "để nguyên", không phải lỗi.
//
// Không đụng tới chuỗi đã có dấu, chuỗi không phải một âm tiết, và những
// chuỗi mà đổi đi cũng bằng không đổi.
std::u32string restore_diacritics(const std::u32string& word,
                                  const Config& cfg, const SyllableSet& known);

}  // namespace hodion
