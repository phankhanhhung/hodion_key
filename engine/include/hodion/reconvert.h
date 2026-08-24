// Sinh lại dấu cho chữ ĐÃ gõ xong ("reconversion").
//
// Người dùng bôi đen một chữ trong tài liệu — có dấu hay không dấu — và
// muốn đổi sang phương án khác mà không phải gõ lại cả từ. Tầng này chỉ
// trả lời một câu: những âm tiết tiếng Việt hợp lệ nào có cùng chuỗi chữ
// cái gốc với chữ đó?
//
// Portable như phần còn lại của engine: Windows nối vào ITfFnReconversion,
// macOS/Linux nối vào cơ chế tương đương của mình.
#pragma once

#include <string>
#include <vector>

#include "hodion/engine.h"

namespace hodion {

// Bỏ mọi dấu tiếng Việt, giữ nguyên hoa/thường: "Việt" → "Viet", "đường"
// → "duong". Ký tự không phải chữ cái được giữ nguyên.
std::u32string strip_diacritics(const std::u32string& s);

// Mọi âm tiết tiếng Việt hợp lệ có cùng chuỗi chữ cái gốc với `word`, theo
// đúng bộ luật mà engine dùng khi gõ. Hoa/thường giữ theo từng vị trí của
// `word` ("Viet" → "Việt", không phải "việt").
//
// Chặt hơn đường gõ đúng một chỗ: engine cố ý bỏ kiểm chính tả cho từ có
// đ (luật viết tắt của UniKey, gõ được "đt", "đc"), nên gõ tay ra được cả
// "đoait". Danh sách này không đề xuất những chữ như vậy.
//
// Trả về rỗng nếu `word` không phải một âm tiết (có dấu cách, chữ số, quá
// dài) hoặc không có phương án nào hợp lệ. Nếu chính `word` là một âm tiết
// hợp lệ thì nó đứng đầu danh sách; phần còn lại theo thứ tự cố định của
// bảng vần, không xếp theo tần suất (engine không có dữ liệu tần suất, và
// một thứ tự đoán mò còn tệ hơn một thứ tự học thuộc được).
//
// cfg chỉ dùng tone_style (chỗ đặt dấu thanh: "hòa" hay "hoà").
std::vector<std::u32string> syllable_variants(const std::u32string& word,
                                              const Config& cfg,
                                              size_t max_results = 64);

}  // namespace hodion
