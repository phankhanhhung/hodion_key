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
#include <vector>

#include "hodion/engine.h"
#include "hodion/ngram.h"

namespace hodion {

// Trả về dạng có dấu của `word`, hoặc chuỗi RỖNG khi không đủ bằng chứng.
// Rỗng nghĩa là "để nguyên", không phải lỗi.
//
// Không đụng tới chuỗi đã có dấu, chuỗi không phải một âm tiết, và những
// chuỗi mà đổi đi cũng bằng không đổi.
std::u32string restore_diacritics(const std::u32string& word,
                                  const Config& cfg, const SyllableSet& known);

// Đoán dấu THEO NGỮ CẢNH TRÁI: `left` là vài âm tiết đã chốt ngay trước
// (mới nhất ở cuối), `word` là chữ không dấu vừa gõ xong.
//
// Chỉ nhìn sang trái, cố ý. Lúc gõ thì chữ bên phải chưa tồn tại, và nếu
// giải mã lại cả câu sau mỗi từ thì chữ ĐÃ hiện trên màn hình sẽ tự đổi
// sau lưng người dùng — khó chịu hơn hẳn so với đoán sai. Chữ đã chốt là
// chốt.
//
// Trả về rỗng khi không đủ bằng chứng: không có mô hình, chữ không nhập
// nhằng (đã có restore_diacritics lo), hoặc phương án đầu không hơn phương
// án nhì đủ nhiều.
// `margin` là khoảng cách tối thiểu (log) mà phương án đầu phải hơn phương
// án nhì thì mới được nhận. Cao hơn = ít trả lời hơn nhưng đúng hơn. Với
// một bộ gõ, đổi SAI tệ hơn là không đổi: chữ còn không dấu thì người dùng
// nhìn thấy ngay, còn chữ sai dấu thì trông như đã xong và lọt qua.
std::u32string restore_in_context(const std::vector<std::u32string>& left,
                                  const std::u32string& word,
                                  const Config& cfg, const SyllableSet& known,
                                  const NgramModel& model,
                                  float margin = 1.0f);

// Giải mã CẢ CÂU bằng Viterbi — dùng cho reconversion, khi người dùng bôi
// đen một đoạn đã gõ xong. Ở đó chữ bên phải đã có sẵn nên tối ưu chung cả
// chuỗi được, và không có chuyện đổi chữ sau lưng ai.
//
// Trả về đúng số phần tử như đầu vào; phần tử nào không đoán được thì giữ
// nguyên chữ gốc.
std::vector<std::u32string> restore_sentence(
    const std::vector<std::u32string>& words, const Config& cfg,
    const SyllableSet& known, const NgramModel& model);

}  // namespace hodion
