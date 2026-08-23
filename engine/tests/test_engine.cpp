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

  // Ký tự không phải chữ cái khi chưa gõ gì → None (đi thẳng tới app).
  {
    Engine e;
    auto r = e.process_char(U' ');
    EXPECT_TRUE(r.action == Engine::Result::Action::None);
    r = e.process_char(U'5');
    EXPECT_TRUE(r.action == Engine::Result::Action::None);
    EXPECT_TRUE(!e.composing());
  }

  // Telex: chữ số là phím ngắt từ.
  {
    Engine e;
    feed(e, "toans");
    auto r = e.process_char(U'1');
    EXPECT_TRUE(r.action == Engine::Result::Action::Commit);
    EXPECT_EQ(u8(r.text), "toán1");
  }

  // VNI: chữ số thuộc về từ đang gõ.
  {
    hodion::Config cfg;
    cfg.method = hodion::InputMethod::Vni;
    Engine e(cfg);
    feed(e, "toan");
    auto r = e.process_char(U'1');
    EXPECT_TRUE(r.action == Engine::Result::Action::Composing);
    EXPECT_EQ(u8(e.composition()), "toán");
  }

  // Backspace = hoàn tác một phím.
  {
    Engine e;
    feed(e, "vieejt");
    auto r = e.process_backspace();
    EXPECT_TRUE(r.action == Engine::Result::Action::Composing);
    EXPECT_EQ(u8(r.text), "việ");
    // Xóa hết thì composition rỗng nhưng vẫn báo Composing để host đóng lại.
    for (int i = 0; i < 5; ++i) e.process_backspace();
    EXPECT_TRUE(!e.composing());
    r = e.process_backspace();
    EXPECT_TRUE(r.action == Engine::Result::Action::None);
  }

  // raw() giữ nguyên phím thô cho Esc; commit() chốt và reset.
  {
    Engine e;
    feed(e, "nuowcs");
    EXPECT_EQ(u8(e.raw()), "nuowcs");
    EXPECT_EQ(u8(e.commit()), "nước");
    EXPECT_TRUE(!e.composing());
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
    for (int i = 0; i < 60; ++i) {
      auto r = e.process_char(U'k');
      if (r.action == Engine::Result::Action::Commit) break;
    }
    EXPECT_TRUE(!e.composing() || e.raw().size() <= 41);
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
    EXPECT_TRUE(hodion_engine_composing(e) == 0);
    hodion_engine_destroy(e);
  }

  // UTF-16 cho tầng Windows.
  {
    const std::u16string w = hodion::utf::to_utf16(U"việt");
    EXPECT_TRUE(w.size() == 4 && w[2] == u'ệ');
  }
}
