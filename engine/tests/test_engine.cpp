#include <cstring>

#include "hodion/engine_c.h"
#include "test_util.h"

using hodion::Engine;

namespace {

std::string u8(const std::u32string& s) { return hodion::utf::to_utf8(s); }

void feed(Engine& e, const std::string& keys) {
  for (char c : keys) e.process_char(static_cast<char32_t>(c));
}

}  // namespace

void run_engine_tests() {
  // Vòng đời cơ bản: gõ → composing, phím ngắt → commit kèm ký tự ngắt.
  {
    Engine e;
    auto r = e.process_char(U'v');
    EXPECT_TRUE(r.action == Engine::Result::Action::Composing);
    feed(e, "ieejt");
    EXPECT_EQ(u8(e.composition()), "việt");
    r = e.process_char(U' ');
    EXPECT_TRUE(r.action == Engine::Result::Action::Commit);
    EXPECT_EQ(u8(r.text), "việt ");
    EXPECT_TRUE(!e.composing());
  }

  // Ký tự không mở từ khi chưa gõ gì → None (đi thẳng tới app).
  {
    Engine e;
    EXPECT_TRUE(e.process_char(U' ').action == Engine::Result::Action::None);
    EXPECT_TRUE(e.process_char(U'5').action == Engine::Result::Action::None);
    EXPECT_TRUE(!e.composing());
  }

  // Dấu câu là ký tự ngắt từ.
  {
    Engine e;
    feed(e, "toans");
    auto r = e.process_char(U'!');
    EXPECT_TRUE(r.action == Engine::Result::Action::Commit);
    EXPECT_EQ(u8(r.text), "toán!");
  }

  // Chữ số KHÔNG ngắt từ — nó nằm trong từ và đóng băng từ (như UniKey).
  {
    Engine e;
    feed(e, "toans");
    auto r = e.process_char(U'1');
    EXPECT_TRUE(r.action == Engine::Result::Action::Composing);
    EXPECT_EQ(u8(e.composition()), "toán1");
    r = e.process_char(U's');  // từ đã hỏng → s là chữ thường
    EXPECT_EQ(u8(e.composition()), "toán1s");
  }

  // Backspace xóa một KÝ TỰ hiển thị và dấu thanh lùi vị trí nếu cần.
  {
    Engine e;
    feed(e, "hoafn");
    EXPECT_EQ(u8(e.composition()), "hoàn");
    auto r = e.process_backspace();
    EXPECT_EQ(u8(r.text), "hòa");  // xóa n → thanh lùi về o (kiểu cũ)
  }
  {
    hodion::Config cfg;
    cfg.tone_style = hodion::ToneStyle::Modern;
    Engine e(cfg);
    feed(e, "hoaf");
    EXPECT_EQ(u8(e.composition()), "hoà");
    auto r = e.process_backspace();
    EXPECT_EQ(u8(r.text), "ho");  // thanh nằm trên ký tự bị xóa → mất theo
  }
  {
    Engine e;
    feed(e, "vieejt");
    auto r = e.process_backspace();
    EXPECT_EQ(u8(r.text), "việ");
    for (int i = 0; i < 5; ++i) e.process_backspace();
    EXPECT_TRUE(!e.composing());
    EXPECT_TRUE(e.process_backspace().action == Engine::Result::Action::None);
  }

  // raw() giữ phím thô cho Esc; commit() chốt và reset.
  {
    Engine e;
    feed(e, "nuowcs");
    EXPECT_EQ(u8(e.raw()), "nuowcs");
    EXPECT_EQ(u8(e.commit()), "nước");
    EXPECT_TRUE(!e.composing());
  }

  // Tự khôi phục từ không phải tiếng Việt (autoNonVnRestore).
  {
    hodion::Config cfg;
    cfg.restore_non_vn = true;
    Engine e(cfg);
    feed(e, "boxing");
    EXPECT_EQ(u8(e.composition()), "bõing");
    auto r = e.process_char(U' ');
    EXPECT_TRUE(r.action == Engine::Result::Action::Commit);
    EXPECT_EQ(u8(r.text), "boxing ");  // trả lại đúng phím đã gõ
  }
  {
    hodion::Config cfg;
    cfg.restore_non_vn = true;
    Engine e(cfg);
    feed(e, "toans");  // từ hợp lệ thì giữ nguyên kết quả
    auto r = e.process_char(U' ');
    EXPECT_EQ(u8(r.text), "toán ");
  }

  // Tắt kiểm tra chính tả: ký tự hỏng mở từ mới thay vì đóng băng.
  {
    hodion::Config cfg;
    cfg.spell_check = false;
    Engine e(cfg);
    feed(e, "cases");
    // c,a,s→cá; e không ghép được → mở âm tiết mới; s thứ hai ăn sắc vào e.
    EXPECT_EQ(u8(e.composition()), "cáé");
  }

  // Telex bracket khi không áp được → ký tự ngắt từ.
  {
    Engine e;
    feed(e, "tan");
    auto r = e.process_char(U'[');
    EXPECT_TRUE(r.action == Engine::Result::Action::Commit);
    EXPECT_EQ(u8(r.text), "tan[");
  }
  {
    Engine e;
    auto r = e.process_char(U'[');
    EXPECT_TRUE(r.action == Engine::Result::Action::Composing);
    EXPECT_EQ(u8(e.composition()), "ơ");
  }

  // Đổi cấu hình giữa chừng thì reset trạng thái.
  {
    Engine e;
    feed(e, "abc");
    hodion::Config cfg;
    cfg.method = hodion::InputMethod::Vni;
    e.set_config(cfg);
    EXPECT_TRUE(!e.composing());
  }

  // Buffer quá dài thì tự chốt.
  {
    Engine e;
    bool committed = false;
    for (int i = 0; i < 60; ++i) {
      if (e.process_char(U'k').action == Engine::Result::Action::Commit) {
        committed = true;
        break;
      }
    }
    EXPECT_TRUE(committed);
  }

  // C API smoke test.
  {
    hodion_engine* e = hodion_engine_create();
    char out[64];
    for (const char* p = "vieej"; *p; ++p) {
      hodion_engine_key(e, static_cast<uint32_t>(*p), out, sizeof(out));
    }
    int action = hodion_engine_key(e, 't', out, sizeof(out));
    EXPECT_TRUE(action == HODION_ACTION_COMPOSING);
    EXPECT_EQ(std::string(out), "việt");
    action = hodion_engine_key(e, ' ', out, sizeof(out));
    EXPECT_TRUE(action == HODION_ACTION_COMMIT);
    EXPECT_EQ(std::string(out), "việt ");
    hodion_engine_set_flag(e, HODION_FLAG_SPELL_CHECK, 0);
    EXPECT_TRUE(hodion_engine_composing(e) == 0);
    hodion_engine_destroy(e);
  }

  // UTF-16 cho tầng Windows.
  {
    const std::u16string w = hodion::utf::to_utf16(U"việt");
    EXPECT_TRUE(w.size() == 4 && w[2] == u'ệ');
  }
}
