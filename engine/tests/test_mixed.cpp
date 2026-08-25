// Gõ trộn Việt – Anh: hủy biến đổi cho từ đang gõ (cancel_transform).
//
// Tầng Windows gắn hàm này vào Ctrl+Backspace; ở đây chỉ kiểm tra hợp đồng
// của engine, độc lập hệ điều hành.
#include <set>
#include <string>

#include "test_util.h"

using hodion::Config;
using hodion::Engine;

namespace {

// Từ điển giả: test hợp đồng của engine, độc lập với bảng thật ở wordlist/.
class FakeWords : public hodion::ForeignWords {
 public:
  explicit FakeWords(std::set<std::u32string> words)
      : words_(std::move(words)) {}
  bool contains(const std::u32string& key) const override {
    ++queries;
    last = key;
    return words_.count(key) != 0;
  }
  mutable int queries = 0;
  mutable std::u32string last;

 private:
  std::set<std::u32string> words_;
};

std::string u8(const std::u32string& s) { return hodion::utf::to_utf8(s); }

void feed(Engine& e, const std::string& keys) {
  for (char c : keys) e.process_char(static_cast<char32_t>(c));
}

// Gõ rồi chốt bằng dấu cách; trả về chuỗi được chốt (bỏ dấu cách cuối).
std::string commit_by_space(const std::string& keys, Config cfg) {
  Engine e(cfg);
  feed(e, keys);
  const auto r = e.process_char(U' ');
  std::string out = u8(r.text);
  if (!out.empty() && out.back() == ' ') out.pop_back();
  return out;
}

// Như trên nhưng có nạp từ điển.
std::string commit_with(const hodion::ForeignWords& dict,
                        const std::string& keys, Config cfg) {
  Engine e(cfg);
  e.set_foreign_words(&dict);
  feed(e, keys);
  return u8(e.commit());
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

  // ======================================================================
  // Từ điển ngoại lai: quyết định lúc CHỐT từ
  // ======================================================================

  // Không nạp từ điển thì hành vi không đổi một li.
  EXPECT_EQ(commit_by_space("meeting", Config{}), std::string("mêting"));

  {
    FakeWords dict({U"meeting", U"server", U"boss", U"cart", U"bozo"});
    Config cfg;

    // Từ tiếng Anh đã biết: trả lại đúng chuỗi phím đã gõ.
    EXPECT_EQ(commit_with(dict, "meeting", cfg), std::string("meeting"));
    EXPECT_EQ(commit_with(dict, "server", cfg), std::string("server"));
    EXPECT_EQ(commit_with(dict, "boss", cfg), std::string("boss"));
    // Hai đầu bảng chữ cái: 'a' và 'z' cũng phải là ký tự hợp lệ của từ.
    EXPECT_EQ(commit_with(dict, "cart", cfg), std::string("cart"));
    EXPECT_EQ(commit_with(dict, "bozo", cfg), std::string("bozo"));
    // Không có trong từ điển thì giữ nguyên chữ tiếng Việt.
    EXPECT_EQ(commit_with(dict, "test", cfg), std::string("tét"));
    EXPECT_EQ(commit_with(dict, "vieejt", cfg), std::string("việt"));

    // Hoa/thường: tra bằng chữ thường nhưng trả lại đúng dạng đã gõ.
    EXPECT_EQ(commit_with(dict, "Meeting", cfg), std::string("Meeting"));
    EXPECT_EQ(commit_with(dict, "SERVER", cfg), std::string("SERVER"));

    // Tắt tùy chọn thì không tra nữa.
    Config off = cfg;
    off.english_detect = false;
    const int before = dict.queries;
    EXPECT_EQ(commit_with(dict, "meeting", off), std::string("mêting"));
    EXPECT_TRUE(dict.queries == before);
  }

  // Chỉ tra lúc chốt, không tra trên đường gõ từng phím.
  {
    FakeWords dict({U"meeting"});
    Engine e;
    e.set_foreign_words(&dict);
    feed(e, "meeting");
    EXPECT_TRUE(dict.queries == 0);
    EXPECT_EQ(u8(e.commit()), std::string("meeting"));
    EXPECT_TRUE(dict.queries == 1);
  }

  // Từ ngắn không bao giờ được tra: "as"/"is"/"or" vừa là từ tiếng Anh vừa
  // là chuỗi gõ hợp lệ của á/í/ỏ.
  {
    FakeWords dict({U"as", U"is", U"or", U"aws", U"book"});
    Config cfg;
    EXPECT_EQ(commit_with(dict, "as", cfg), std::string("á"));
    EXPECT_EQ(commit_with(dict, "is", cfg), std::string("í"));
    EXPECT_EQ(commit_with(dict, "or", cfg), std::string("ỏ"));
    EXPECT_EQ(commit_with(dict, "aws", cfg), std::string("ắ"));  // 3 phím
    EXPECT_TRUE(dict.queries == 0);
    // Đủ 4 phím thì mới bắt đầu xét.
    EXPECT_EQ(commit_with(dict, "book", cfg), std::string("book"));
    EXPECT_TRUE(dict.queries == 1);
  }

  // Chuỗi phím không thuần chữ cái không được tra (VNI có chữ số, và các
  // ký tự lạ lọt vào giữa từ).
  {
    FakeWords dict({U"meeting"});
    Config vni_cfg;
    vni_cfg.method = hodion::InputMethod::Vni;
    commit_with(dict, "vie6t5", vni_cfg);
    commit_with(dict, "me2eting", Config{});
    EXPECT_TRUE(dict.queries == 0);
  }

  // Sau Backspace, nhật ký phím không còn tin được → không khôi phục gì.
  {
    FakeWords dict({U"meeting", U"meetin"});
    Engine e;
    e.set_foreign_words(&dict);
    feed(e, "meeting");
    e.process_backspace();
    EXPECT_EQ(u8(e.commit()), std::string("mêtin"));
    EXPECT_TRUE(dict.queries == 0);
  }

  // Sau khi hủy biến đổi thì đã là chuỗi thô rồi, không cần tra.
  {
    FakeWords dict({U"meeting"});
    Engine e;
    e.set_foreign_words(&dict);
    feed(e, "mee");
    e.cancel_transform();
    feed(e, "ting");
    EXPECT_EQ(u8(e.commit()), std::string("meeting"));
    EXPECT_TRUE(dict.queries == 0);
  }

  // Gỡ từ điển ra thì quay lại hành vi cũ.
  {
    FakeWords dict({U"meeting"});
    Engine e;
    e.set_foreign_words(&dict);
    e.set_foreign_words(nullptr);
    feed(e, "meeting");
    EXPECT_EQ(u8(e.commit()), std::string("mêting"));
    EXPECT_TRUE(dict.queries == 0);
  }

  // Đi cùng restore_non_vn: hai luật độc lập, từ điển bắt được cả ca mà
  // luật cấu trúc thấy hợp lệ, và không cấm luật kia chạy.
  {
    FakeWords dict({U"meeting"});
    Config cfg;
    cfg.restore_non_vn = true;
    EXPECT_EQ(commit_with(dict, "meeting", cfg), std::string("meeting"));
    // Không có trong từ điển nhưng cũng không phải tiếng Việt → luật cấu
    // trúc khôi phục.
    EXPECT_EQ(commit_with(dict, "boxing", cfg), std::string("boxing"));
  }

  // composing_is_vietnamese phản ánh đúng cấu trúc âm tiết.
  {
    Engine e;
    EXPECT_TRUE(!e.composing_is_vietnamese());  // rỗng
    feed(e, "vieejt");
    EXPECT_TRUE(e.composing_is_vietnamese());
    e.reset();
    feed(e, "meeting");
    EXPECT_TRUE(!e.composing_is_vietnamese());
    e.reset();
    feed(e, "tesst");  // "tét" — vẫn là âm tiết tiếng Việt hợp lệ
    e.reset();
    feed(e, "test");
    EXPECT_TRUE(e.composing_is_vietnamese());
    e.cancel_transform();
    EXPECT_TRUE(!e.composing_is_vietnamese());  // gõ thẳng thì không xét
  }

  // Tính chất: chuỗi chốt CHỈ được là một trong hai thứ — chữ đã ghép, hoặc
  // đúng chuỗi phím thô. Không bao giờ là thứ ba, dù từ điển nói gì.
  {
    class AlwaysForeign : public hodion::ForeignWords {
      bool contains(const std::u32string&) const override { return true; }
    } yes;

    static const char* kSeqs[] = {
        "meeting", "server", "test",   "vieejt", "nguyeenx", "boxing",
        "thuowng", "dduongwf", "cars", "boss",   "khoongr",  "aaaa",
        "wwww",    "ddddd",   "qusa",  "gieengs", "uouwj",   "zzzz"};
    for (int fm = 0; fm < 2; ++fm) {
      for (int sc = 0; sc < 2; ++sc) {
        for (int rn = 0; rn < 2; ++rn) {
          Config cfg;
          cfg.free_marking = fm != 0;
          cfg.spell_check = sc != 0;
          cfg.restore_non_vn = rn != 0;
          for (const char* seq : kSeqs) {
            Engine e(cfg);
            e.set_foreign_words(&yes);
            feed(e, seq);
            const std::string render = u8(e.composition());
            const std::string raw = u8(e.raw());
            const std::string got = u8(e.commit());
            EXPECT_TRUE(got == render || got == raw);
          }
        }
      }
    }
  }

  // --- Từ dài bất thường vẫn tự chốt, không phình vô hạn ---
  //
  // Gõ thẳng phải dùng ĐÚNG cùng một chặn độ dài với đường ghép bình
  // thường, không lệch một ký tự.
  {
    auto cap = [](bool cancel) {
      Engine e;
      feed(e, "ab");
      if (cancel) e.cancel_transform();
      for (int i = 0; i < 200; ++i) {
        const auto r = e.process_char(U'x');
        if (r.action == Engine::Result::Action::Commit) {
          return static_cast<int>(r.text.size());
        }
      }
      return -1;
    };
    const int normal = cap(false);
    const int literal = cap(true);
    EXPECT_TRUE(normal > 0);
    EXPECT_TRUE(literal == normal);

    Engine e;
    feed(e, "ab");
    e.cancel_transform();
    for (int i = 0; i < 200; ++i) {
      if (e.process_char(U'x').action == Engine::Result::Action::Commit) break;
    }
    EXPECT_TRUE(!e.composing());
    EXPECT_TRUE(e.self_check());
  }
}
