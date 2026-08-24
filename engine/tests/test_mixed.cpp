// Gõ trộn Việt – Anh: hủy biến đổi cho từ đang gõ (cancel_transform).
//
// Tầng Windows gắn hàm này vào Ctrl+Backspace; ở đây chỉ kiểm tra hợp đồng
// của engine, độc lập hệ điều hành.
#include "test_util.h"

using hodion::Config;
using hodion::Engine;

namespace {

std::string u8(const std::u32string& s) { return hodion::utf::to_utf8(s); }

void feed(Engine& e, const std::string& keys) {
  for (char c : keys) e.process_char(static_cast<char32_t>(c));
}

// Gõ `before`, hủy biến đổi, gõ tiếp `after`; trả về chuỗi đang hiển thị.
std::string cancel_then(const std::string& before, const std::string& after,
                        Config cfg = Config{}) {
  Engine e(cfg);
  feed(e, before);
  e.cancel_transform();
  feed(e, after);
  return u8(e.composition());
}

}  // namespace

void run_mixed_tests() {
  // --- Vì sao cần: những từ này bị biến đổi dù người ta gõ tiếng Anh ---
  EXPECT_EQ(telex("test"), std::string("tét"));
  EXPECT_EQ(telex("meeting"), std::string("mêting"));
  EXPECT_EQ(telex("server"), std::string("sẻver"));
  EXPECT_EQ(telex("cars"), std::string("cá"));

  // --- Hủy biến đổi rồi gõ tiếp: phần còn lại nối nguyên văn ---
  EXPECT_EQ(cancel_then("te", "st"), std::string("test"));
  EXPECT_EQ(cancel_then("m", "eeting"), std::string("meeting"));
  EXPECT_EQ(cancel_then("s", "erver"), std::string("server"));
  // Hủy sau khi chữ đã biến đổi: trả lại đúng chuỗi phím đã gõ.
  EXPECT_EQ(cancel_then("vieejt", ""), std::string("vieejt"));
  EXPECT_EQ(cancel_then("cars", ""), std::string("cars"));

  // --- Trạng thái ---
  {
    Engine e;
    EXPECT_TRUE(!e.literal());
    // Chưa gõ gì thì không có gì để hủy.
    EXPECT_TRUE(e.cancel_transform().action ==
                Engine::Result::Action::None);
    EXPECT_TRUE(!e.literal());

    feed(e, "te");
    const auto r = e.cancel_transform();
    EXPECT_TRUE(r.action == Engine::Result::Action::Composing);
    EXPECT_EQ(u8(r.text), std::string("te"));
    EXPECT_TRUE(e.literal());
    EXPECT_TRUE(e.composing());
    // Hủy lần hai không làm gì nữa.
    EXPECT_TRUE(e.cancel_transform().action ==
                Engine::Result::Action::None);
    EXPECT_TRUE(e.literal());
    EXPECT_TRUE(e.self_check());
  }

  // --- Phím dấu mất hiệu lực sau khi hủy ---
  {
    // s/f/r/x/j chỉ còn là chữ cái.
    EXPECT_EQ(cancel_then("a", "s"), std::string("as"));
    EXPECT_EQ(cancel_then("a", "aeooddw"), std::string("aaeoodd") + "w");
    // VNI: chữ số cũng vậy.
    Config vni_cfg;
    vni_cfg.method = hodion::InputMethod::Vni;
    EXPECT_EQ(cancel_then("a", "1234", vni_cfg), std::string("a1234"));
  }

  // --- Chốt từ ---
  {
    Engine e;
    feed(e, "te");
    e.cancel_transform();
    feed(e, "st");
    const auto r = e.process_char(U' ');
    EXPECT_TRUE(r.action == Engine::Result::Action::Commit);
    EXPECT_EQ(u8(r.text), std::string("test "));
    // Chốt xong là hết chế độ gõ thẳng — từ sau gõ tiếng Việt như thường.
    EXPECT_TRUE(!e.literal());
    EXPECT_TRUE(!e.composing());
    feed(e, "vieejt");
    EXPECT_EQ(u8(e.composition()), std::string("việt"));
    EXPECT_TRUE(e.self_check());
  }

  // commit() thủ công cũng trả về chuỗi thô.
  {
    Engine e;
    feed(e, "boss");
    e.cancel_transform();
    EXPECT_EQ(u8(e.commit()), std::string("boss"));
    EXPECT_TRUE(!e.literal());
  }

  // --- Backspace trong chế độ gõ thẳng ---
  {
    Engine e;
    feed(e, "te");
    e.cancel_transform();
    feed(e, "st");
    e.process_backspace();
    EXPECT_EQ(u8(e.composition()), std::string("tes"));
    EXPECT_TRUE(e.literal());

    // Xóa sạch thì thoát chế độ gõ thẳng, tiếng Việt trở lại ngay trong
    // cùng một từ.
    for (int i = 0; i < 3; ++i) e.process_backspace();
    EXPECT_TRUE(!e.literal());
    EXPECT_TRUE(!e.composing());
    EXPECT_TRUE(e.self_check());
    EXPECT_TRUE(e.process_backspace().action ==
                Engine::Result::Action::None);

    feed(e, "aa");
    EXPECT_EQ(u8(e.composition()), std::string("â"));
  }

  // Hồi quy (fuzz tìm ra): hủy biến đổi rồi xóa sạch phải trả nhật ký phím
  // về trạng thái dùng được, nếu không Esc của từ KẾ TIẾP chỉ trả lại chữ
  // đang hiển thị thay vì chuỗi phím thô.
  {
    Engine e;
    feed(e, "p");
    e.cancel_transform();
    e.process_backspace();  // xóa sạch, thoát gõ thẳng
    EXPECT_TRUE(!e.composing());
    feed(e, "vieejt");
    EXPECT_EQ(u8(e.composition()), std::string("việt"));
    EXPECT_EQ(u8(e.raw()), std::string("vieejt"));
  }

  // --- Hủy sau khi đã Backspace: nhật ký phím không còn, giữ chữ hiển thị ---
  {
    Engine e;
    feed(e, "vieejt");
    e.process_backspace();  // việt → việ
    EXPECT_EQ(u8(e.composition()), std::string("việ"));
    e.cancel_transform();
    EXPECT_EQ(u8(e.composition()), std::string("việ"));
    EXPECT_TRUE(e.literal());
    feed(e, "t");
    EXPECT_EQ(u8(e.composition()), std::string("việt"));
  }

  // --- Esc sau khi hủy: raw() chính là chuỗi đang gõ thẳng ---
  {
    Engine e;
    feed(e, "te");
    e.cancel_transform();
    feed(e, "st");
    EXPECT_EQ(u8(e.raw()), std::string("test"));
  }

  // --- Đổi cấu hình xóa luôn chế độ gõ thẳng ---
  {
    Engine e;
    feed(e, "te");
    e.cancel_transform();
    Config cfg;
    cfg.free_marking = false;
    e.set_config(cfg);
    EXPECT_TRUE(!e.literal());
    EXPECT_TRUE(!e.composing());
    EXPECT_TRUE(e.self_check());
  }

  // --- Từ dài bất thường vẫn tự chốt, không phình vô hạn ---
  {
    Engine e;
    feed(e, "ab");
    e.cancel_transform();
    bool committed = false;
    for (int i = 0; i < 80 && !committed; ++i) {
      committed = e.process_char(U'x').action ==
                  Engine::Result::Action::Commit;
    }
    EXPECT_TRUE(committed);
    EXPECT_TRUE(!e.composing());
    EXPECT_TRUE(e.self_check());
  }
}
