// Từ điển tiếng Anh dựng sẵn cho HodionKey.
//
// Tầng RIÊNG, tùy chọn: lõi engine không biết gì về dữ liệu này và vẫn
// build/chạy đúng khi không có nó. Frontend nào muốn dùng thì link thư viện
// này rồi gọi Engine::set_foreign_words().
//
// Bảng chỉ chứa những từ mà gõ Telex sẽ làm biến dạng, và ĐÃ LOẠI mọi từ
// ghép ra một âm tiết tiếng Việt hợp lệ (bans→bán, bust→bút, test→tét).
// Nhờ vậy nó không bao giờ ghi đè lên chữ tiếng Việt đúng — xem
// tools/build_wordlist.py và wordlist/tests.
#pragma once

#include "hodion/engine.h"

namespace hodion {

// Bảng tra cố định, không trạng thái, không cấp phát — dùng chung được cho
// mọi thread và mọi engine. Tham chiếu sống suốt đời chương trình.
const ForeignWords& english_words();

// Số từ trong bảng (dùng cho test và để báo cáo).
size_t english_words_count();

}  // namespace hodion
