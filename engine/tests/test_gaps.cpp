// Các ca do mutation testing chỉ ra là bộ test còn bỏ lọt: chỗ nào gieo lỗi
// vào mà không test nào phát hiện thì thêm test ở đây.
//
// Chạy lại bằng:  python3 tools/mutation_test.py
#include "hodion/engine_c.h"
#include "test_util.h"
#include "vnlexi.h"  // white-box: kiểm tra thẳng bảng ký tự

using hodion::Engine;

namespace {

std::string u8(const std::u32string& s) { return hodion::utf::to_utf8(s); }

void feed(Engine& e, const std::string& keys) {
  for (char c : keys) e.process_char(static_cast<char32_t>(c));
}

std::string type_sentence(const std::string& keys, hodion::Config cfg) {
  Engine e(cfg);
  std::string out;
  for (char c : keys) {
    const auto r = e.process_char(static_cast<char32_t>(c));
    if (r.action == Engine::Result::Action::Commit) {
      out += u8(r.text);
    } else if (r.action == Engine::Result::Action::None) {
      out += c;
    }
  }
  out += u8(e.commit());
  return out;
}

}  // namespace

void run_gap_tests() {
  // ---- Âm cuối phải được tính khi kiểm tra dấu có hợp lệ không -----------
  // "oă" đi được với c/m/n/ng/t nhưng không đi với "ch", nên chữ w phải bị
  // từ chối; nếu bỏ qua âm cuối khi kiểm tra thì sẽ ra "hoăch".
  EXPECT_EQ(telex("hoachw"), "hoachw");
  EXPECT_EQ(telex("hoacw"), "hoăc");     // cùng vần nhưng âm cuối hợp lệ
  EXPECT_EQ(telex("khoanw"), "khoăn");
  // "ơ" không đi với âm cuối "ng" → phím w bị từ chối.
  EXPECT_EQ(telex("mongw"), "mongw");
  EXPECT_EQ(telex("monw"), "mơn");

  // ---- Phụ âm đầu phải được tính khi ghép thêm nguyên âm ------------------
  // k chỉ đi với dòng e/i/y: "iu" không nằm trong danh sách nên "kiu" hỏng.
  EXPECT_EQ(telex("kius"), "kius");
  EXPECT_EQ(telex("kieeuj"), "kiệu");    // "iêu" thì hợp lệ với k
  EXPECT_EQ(telex("kieuj"), "kiẹu");     // "ieu" không mũ: thanh vào e
  // qu không đi với vần bắt đầu bằng u.
  EXPECT_EQ(telex("quuas"), "quuas");

  // ---- "ch" cũng là âm cuối tắc, không nhận huyền/hỏi/ngã ----------------
  EXPECT_EQ(telex("bachf"), "bachf");
  EXPECT_EQ(telex("bachx"), "bachx");
  EXPECT_EQ(telex("bachr"), "bachr");
  EXPECT_EQ(telex("bachj"), "bạch");   // nặng thì được
  EXPECT_EQ(telex("saachx"), "sâchx");

  // ---- Chuyển qua lại trong họ vần uo bằng mũ và móc ---------------------
  EXPECT_EQ(telex("uowo"), "uô");      // ươ + mũ → uô (vần hai nguyên âm)
  EXPECT_EQ(telex("uowio"), "uôi");    // ươi + mũ → uôi (vần ba nguyên âm)
  EXPECT_EQ(telex("uoow"), "ươ");      // uô + móc → ươ
  EXPECT_EQ(telex("thuoow"), "thuơ");  // sau "th" thì uô + móc → uơ
  EXPECT_EQ(telex("nuoocw"), "nươc");   // chưa gõ thanh nên không có dấu

  // ---- z xóa dấu thanh đang có trên gi ------------------------------------
  EXPECT_EQ(telex("gifz"), "gi");
  EXPECT_EQ(telex("ginfz"), "gin");
  EXPECT_EQ(telex("giz"), "giz");      // chưa có thanh thì z là chữ thường

  // ---- Cứu dấu thanh khi hủy một nguyên âm bằng phím ngoặc ---------------
  // ]s[ → "ướ"; gõ [ lần nữa hủy ơ, dấu sắc phải lùi về ư chứ không mất.
  {
    Engine e;
    feed(e, "]s[");
    EXPECT_EQ(u8(e.composition()), "ướ");
    const auto r = e.process_char(U'[');
    EXPECT_TRUE(r.action == Engine::Result::Action::Commit);
    EXPECT_EQ(u8(r.text), "ứ[");
  }

  // ---- Ngưỡng độ dài từ ---------------------------------------------------
  {
    // Từ dài đúng ngưỡng thì chốt; ngay trước ngưỡng thì vẫn đang ghép.
    Engine e;
    int committed_at = -1;
    for (int i = 0; i < 60; ++i) {
      const auto r = e.process_char(U'b');
      if (r.action == Engine::Result::Action::Commit) {
        committed_at = i + 1;
        EXPECT_TRUE(u8(r.text).size() == static_cast<size_t>(committed_at));
        break;
      }
    }
    EXPECT_TRUE(committed_at == 40);
    EXPECT_TRUE(!e.composing());
  }

  // ---- Khôi phục tự động: chỉ chạy khi thật sự có phím bị biến đổi -------
  {
    hodion::Config cfg;
    cfg.restore_non_vn = true;
    // Không có phím nào bị biến đổi → giữ nguyên chữ, không "khôi phục".
    EXPECT_EQ(type_sentence("bcd ", cfg), "bcd ");
    // Có biến đổi và từ hỏng → trả lại đúng phím đã gõ.
    EXPECT_EQ(type_sentence("boxing ", cfg), "boxing ");
    // Có biến đổi nhưng từ hợp lệ → giữ kết quả tiếng Việt.
    EXPECT_EQ(type_sentence("tieengs ", cfg), "tiếng ");
    // Ngoặc Telex không áp được thì cũng không tính là biến đổi.
    EXPECT_EQ(type_sentence("ab[", cfg), "ab[");
  }

  // ---- Bảng ký tự: kiểm tra thẳng, kể cả các tổ hợp engine không sinh ra --
  {
    using hodion::detail::composed_char;
    using hodion::detail::Mark;
    EXPECT_TRUE(composed_char(U'a', Mark::None, 0, false) == U'a');
    EXPECT_TRUE(composed_char(U'a', Mark::None, 0, true) == U'A');
    EXPECT_TRUE(composed_char(U'a', Mark::None, 1, false) == U'á');
    EXPECT_TRUE(composed_char(U'a', Mark::Breve, 5, false) == U'ặ');
    EXPECT_TRUE(composed_char(U'o', Mark::Horn, 4, true) == U'Ỡ');
    EXPECT_TRUE(composed_char(U'u', Mark::Horn, 2, false) == U'ừ');
    EXPECT_TRUE(composed_char(U'y', Mark::None, 3, true) == U'Ỷ');
    EXPECT_TRUE(composed_char(U'd', Mark::Stroke, 0, false) == U'đ');
    EXPECT_TRUE(composed_char(U'd', Mark::Stroke, 0, true) == U'Đ');
    // Tổ hợp không tồn tại (i không có mũ, d không có thanh) → trả về chữ
    // gốc, đúng hoa/thường; đây là đường thoát an toàn của bảng.
    EXPECT_TRUE(composed_char(U'i', Mark::Circumflex, 0, false) == U'i');
    EXPECT_TRUE(composed_char(U'a', Mark::Stroke, 0, false) == U'a');
    EXPECT_TRUE(composed_char(U'a', Mark::Stroke, 0, true) == U'A');
    EXPECT_TRUE(composed_char(U'b', Mark::None, 0, false) == U'b');
    EXPECT_TRUE(composed_char(U'b', Mark::None, 0, true) == U'B');
    EXPECT_TRUE(composed_char(U'z', Mark::None, 0, true) == U'Z');
    // Ký tự không phải chữ cái thì giữ nguyên, kể cả khi đòi viết hoa.
    EXPECT_TRUE(composed_char(U'-', Mark::None, 0, true) == U'-');
    EXPECT_TRUE(composed_char(U'5', Mark::None, 0, true) == U'5');
  }

  // ---- Biên của bộ chuyển đổi UTF ----------------------------------------
  {
    using hodion::utf::from_utf8;
    using hodion::utf::to_utf16;
    using hodion::utf::to_utf8;
    // Đúng các mốc đổi số byte: 0x7F/0x80, 0x7FF/0x800, 0xFFFF/0x10000.
    EXPECT_TRUE(to_utf8(std::u32string(1, 0x7F)).size() == 1);
    EXPECT_TRUE(to_utf8(std::u32string(1, 0x80)).size() == 2);
    EXPECT_TRUE(to_utf8(std::u32string(1, 0x7FF)).size() == 2);
    EXPECT_TRUE(to_utf8(std::u32string(1, 0x800)).size() == 3);
    EXPECT_TRUE(to_utf8(std::u32string(1, 0xFFFF)).size() == 3);
    EXPECT_TRUE(to_utf8(std::u32string(1, 0x10000)).size() == 4);
    // UTF-16: 0xFFFF vẫn một đơn vị, 0x10000 thành cặp thay thế.
    EXPECT_TRUE(to_utf16(std::u32string(1, 0xFFFF)).size() == 1);
    EXPECT_TRUE(to_utf16(std::u32string(1, 0x10000)).size() == 2);
    // Khứ hồi đúng ở từng mốc.
    for (char32_t c : {char32_t(0x7F), char32_t(0x80), char32_t(0x7FF),
                       char32_t(0x800), char32_t(0xFFFF), char32_t(0x10000)}) {
      const std::u32string one(1, c);
      EXPECT_TRUE(from_utf8(to_utf8(one)) == one);
    }
  }

  // ---- C API: các đường chưa được đụng tới -------------------------------
  {
    hodion_engine* e = hodion_engine_create();
    char out[64] = {};

    // Đổi kiểu bỏ dấu qua C API.
    hodion_engine_set_tone_style(e, HODION_TONE_MODERN);
    for (const char* p = "hoaf"; *p; ++p) {
      hodion_engine_key(e, static_cast<uint32_t>(*p), out, sizeof(out));
    }
    EXPECT_EQ(std::string(out), "hoà");
    hodion_engine_reset(e);

    hodion_engine_set_tone_style(e, HODION_TONE_TRADITIONAL);
    for (const char* p = "hoaf"; *p; ++p) {
      hodion_engine_key(e, static_cast<uint32_t>(*p), out, sizeof(out));
    }
    EXPECT_EQ(std::string(out), "hòa");
    hodion_engine_reset(e);

    // Cờ tắt rồi bật lại phải có tác dụng cả hai chiều.
    hodion_engine_set_flag(e, HODION_FLAG_W_SHORTHAND, 0);
    hodion_engine_key(e, 't', out, sizeof(out));
    hodion_engine_key(e, 'w', out, sizeof(out));
    EXPECT_EQ(std::string(out), "tw");
    hodion_engine_reset(e);

    hodion_engine_set_flag(e, HODION_FLAG_W_SHORTHAND, 1);
    hodion_engine_key(e, 't', out, sizeof(out));
    hodion_engine_key(e, 'w', out, sizeof(out));
    EXPECT_EQ(std::string(out), "tư");

    // Bộ đệm ra: vừa khít, chật, và cỡ 0 (không được ghi gì).
    const size_t needed = hodion_engine_composition(e, nullptr, 0);
    EXPECT_TRUE(needed == 3);  // "tư" = 1 + 2 byte

    char exact[4] = {'x', 'x', 'x', 'x'};
    hodion_engine_composition(e, exact, sizeof(exact));
    EXPECT_EQ(std::string(exact), "tư");

    char small[3] = {'x', 'x', 'x'};
    hodion_engine_composition(e, small, sizeof(small));
    EXPECT_TRUE(small[2] == '\0');            // luôn kết thúc bằng NUL
    EXPECT_TRUE(std::string(small).size() < 3);

    char one[1] = {'x'};
    hodion_engine_composition(e, one, sizeof(one));
    EXPECT_TRUE(one[0] == '\0');

    char guard[2] = {'x', 'y'};
    hodion_engine_composition(e, guard, 0);   // cỡ 0: không được đụng vào
    EXPECT_TRUE(guard[0] == 'x' && guard[1] == 'y');

    hodion_engine_destroy(e);
  }
}
