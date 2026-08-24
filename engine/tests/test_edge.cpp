// Edge case: chữ hoa, ranh giới âm tiết, thanh nhảy vị trí, backspace,
// tương tác giữa các tùy chọn, và fuzz bất biến.
#include <utility>

#include "hodion/engine_c.h"
#include "test_util.h"

using hodion::Engine;

namespace {

std::string u8(const std::u32string& s) { return hodion::utf::to_utf8(s); }

void feed(Engine& e, const std::string& keys) {
  for (char c : keys) e.process_char(static_cast<char32_t>(c));
}

std::string type_bs(const std::string& keys, int backspaces,
                    hodion::Config cfg = hodion::Config{}) {
  Engine e(cfg);
  feed(e, keys);
  for (int i = 0; i < backspaces; ++i) e.process_backspace();
  return u8(e.composition());
}

std::string no_spell(const std::string& keys) {
  hodion::Config cfg;
  cfg.spell_check = false;
  return type_word(keys, cfg);
}

std::string no_free(const std::string& keys) {
  hodion::Config cfg;
  cfg.free_marking = false;
  return type_word(keys, cfg);
}

// Gõ xong rồi chốt bằng dấu cách; trả về chuỗi được chốt (không gồm dấu cách).
std::string commit_word(const std::string& keys, hodion::Config cfg) {
  Engine e(cfg);
  feed(e, keys);
  const auto r = e.process_char(U' ');
  std::string out = u8(r.text);
  if (!out.empty() && out.back() == ' ') out.pop_back();
  return out;
}

// Gõ cả câu qua đúng luồng của host: nối phần chốt, phần không xử lý, và
// phần còn đang ghép ở cuối.
std::string type_sentence(const std::string& keys,
                          hodion::Config cfg = hodion::Config{}) {
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

void run_edge_tests() {
  // ---- Chữ hoa giữ nguyên qua mọi phép biến đổi ---------------------------
  EXPECT_EQ(telex("Nuowcs"), "Nước");
  EXPECT_EQ(telex("NUOWCS"), "NƯỚC");
  EXPECT_EQ(telex("NGUYEENX"), "NGUYỄN");
  EXPECT_EQ(telex("Dd"), "Đ");
  EXPECT_EQ(telex("dD"), "đ");       // dấu đ lấy hoa/thường của chữ d đầu
  EXPECT_EQ(telex("DDoNGf"), "ĐòNG");
  EXPECT_EQ(telex("DDooNGf"), "ĐồNG");
  EXPECT_EQ(telex("ThuOwr"), "ThuỞ");
  EXPECT_EQ(telex("W"), "Ư");
  EXPECT_EQ(telex("tW"), "tƯ");
  // Phím thanh không phân biệt hoa/thường khi ăn dấu; lúc gõ lặp để hủy thì
  // ký tự nối vào là chính phím vừa bấm (nên hoa/thường theo phím thứ hai).
  EXPECT_EQ(telex("ASS"), "AS");
  EXPECT_EQ(telex("aSs"), "as");
  EXPECT_EQ(telex("aSS"), "aS");

  // ---- w, ư và ranh giới âm tiết ------------------------------------------
  EXPECT_EQ(telex("wu"), "ưu");
  EXPECT_EQ(telex("wt"), "ưt");
  EXPECT_EQ(telex("wa"), "ưa");
  EXPECT_EQ(telex("waf"), "ừa");
  EXPECT_EQ(telex("nwowcs"), "nước");  // w ngay sau phụ âm đầu
  EXPECT_EQ(telex("wowngf"), "ường");
  EXPECT_EQ(telex("wr"), "ử");

  // ---- qu / gi sâu hơn -----------------------------------------------------
  EXPECT_EQ(telex("qu"), "qu");
  EXPECT_EQ(telex("qur"), "qur");     // chưa có nguyên âm → r là chữ thường
  EXPECT_EQ(telex("quoocs"), "quốc");
  EXPECT_EQ(telex("quyeetj"), "quyệt");
  EXPECT_EQ(telex("gi"), "gi");
  // gi + i không ghép được → từ hỏng, phím thanh sau đó là chữ thường.
  EXPECT_EQ(telex("giif"), "giif");
  EXPECT_EQ(telex("giaf"), "già");
  EXPECT_EQ(telex("giawy"), "giăy");
  EXPECT_EQ(telex("nghieengf"), "nghiềng");

  // ---- Âm cuối và thanh ----------------------------------------------------
  EXPECT_EQ(telex("bachs"), "bách");
  EXPECT_EQ(telex("banhs"), "bánh");
  EXPECT_EQ(telex("bangs"), "báng");
  EXPECT_EQ(telex("bangh"), "bangh");  // ngh không làm âm cuối
  EXPECT_EQ(telex("hocj"), "học");
  EXPECT_EQ(telex("hocf"), "hocf");    // coda tắc không nhận huyền
  EXPECT_EQ(telex("mats"), "mát");
  EXPECT_EQ(telex("matx"), "matx");
  EXPECT_EQ(telex("sachs"), "sách");
  // "âch" không phải vần tiếng Việt: thêm h làm hỏng từ, s thành chữ thường.
  EXPECT_EQ(telex("saachs"), "sâchs");

  // ---- Thanh nhảy vị trí khi từ dài ra ------------------------------------
  EXPECT_EQ(telex("hoaf"), "hòa");
  EXPECT_EQ(telex("hoafn"), "hoàn");
  EXPECT_EQ(telex("hoafng"), "hoàng");
  EXPECT_EQ(telex("tuaf"), "tùa");
  EXPECT_EQ(telex("tuafn"), "tuàn");
  EXPECT_EQ(telex("nguyeefn"), "nguyền");
  EXPECT_EQ(telex("khuyeenr"), "khuyển");
  EXPECT_EQ(telex("nhieeuf"), "nhiều");

  // ---- Backspace ----------------------------------------------------------
  EXPECT_EQ(type_bs("hoafn", 1), "hòa");   // thanh lùi về o
  EXPECT_EQ(type_bs("hoafn", 2), "hò");
  EXPECT_EQ(type_bs("nuowcs", 1), "nướ");
  EXPECT_EQ(type_bs("nuowcs", 2), "nư");
  EXPECT_EQ(type_bs("nuowcs", 3), "n");
  EXPECT_EQ(type_bs("vieejt", 6), "");
  EXPECT_EQ(type_bs("nguyeenx", 1), "nguyễ");  // thanh ở ê, không phải ở n
  EXPECT_EQ(type_bs("nguyeenx", 2), "nguy");   // xóa ê là mất luôn thanh
  EXPECT_EQ(type_bs("ddoongf", 1), "đồn");
  {  // xóa hết rồi gõ tiếp phải sạch trạng thái
    Engine e;
    feed(e, "hoafn");
    for (int i = 0; i < 5; ++i) e.process_backspace();
    EXPECT_TRUE(!e.composing());
    feed(e, "vieejt");
    EXPECT_EQ(u8(e.composition()), "việt");
  }
  {  // backspace giữa chừng rồi gõ tiếp
    Engine e;
    feed(e, "nuowc");
    e.process_backspace();
    feed(e, "cs");
    EXPECT_EQ(u8(e.composition()), "nước");
  }

  // ---- raw() phục vụ Esc ---------------------------------------------------
  {
    Engine e;
    feed(e, "dduongwf");
    EXPECT_EQ(u8(e.composition()), "đường");
    EXPECT_EQ(u8(e.raw()), "dduongwf");
  }
  {
    // Sau Backspace không dựng lại được chuỗi phím (5 ký tự ↔ 8 phím), nên
    // raw() trả về chính chữ đang hiển thị — Esc khi đó không phá chữ.
    Engine e;
    feed(e, "dduongwf");
    e.process_backspace();
    EXPECT_EQ(u8(e.composition()), "đườn");
    EXPECT_EQ(u8(e.raw()), "đườn");
    // Xóa sạch thì nhật ký phím dùng lại được cho từ kế tiếp.
    for (int i = 0; i < 4; ++i) e.process_backspace();
    EXPECT_TRUE(!e.composing());
    EXPECT_EQ(u8(e.raw()), "");
    feed(e, "nuowcs");
    EXPECT_EQ(u8(e.raw()), "nuowcs");
  }
  {
    // Khôi phục tự động cũng không dùng nhật ký phím đã hỏng.
    hodion::Config cfg;
    cfg.restore_non_vn = true;
    Engine e(cfg);
    feed(e, "boxing");
    e.process_backspace();
    const auto r = e.process_char(U' ');
    EXPECT_EQ(u8(r.text), "bõin ");  // giữ chữ đang hiển thị, không cắt cụt
  }

  // ---- Hủy dấu khi dấu KHÔNG nằm ở cuối từ (cần gõ dấu tự do) -------------
  // Mũ/móc/thanh đã đặt ở giữa từ vẫn hủy được bằng cách gõ lặp phím dấu,
  // dù con trỏ đã đi qua âm cuối.
  EXPECT_EQ(telex("vieete"), "viete");
  EXPECT_EQ(telex("dduongwwf"), "đuongwf");
  EXPECT_EQ(telex("muonwwc"), "muonwc");
  EXPECT_EQ(telex("tuoiww"), "tuoiw");
  EXPECT_EQ(telex("banass"), "bâns");  // thanh trên â (giữa từ) hủy được
  EXPECT_EQ(vni("viet661"), "viet61");
  EXPECT_EQ(vni("hoan88"), "hoan8");
  // Tắt gõ dấu tự do thì dấu ở giữa từ không hủy được nữa.
  EXPECT_EQ(no_free("vieete"), "viête");

  // ---- Tùy chọn: tắt gõ dấu tự do -----------------------------------------
  EXPECT_EQ(no_free("viete"), "viete");   // mũ chỉ ăn khi đứng ngay sau nguyên âm
  EXPECT_EQ(no_free("vieet"), "viêt");
  EXPECT_EQ(no_free("dund"), "dund");
  EXPECT_EQ(no_free("ddun"), "đun");
  EXPECT_EQ(no_free("nuocws"), "nuocws");
  EXPECT_EQ(no_free("nuowcs"), "nước");

  // ---- Tùy chọn: tắt kiểm tra chính tả ------------------------------------
  EXPECT_EQ(no_spell("did"), "đi");
  EXPECT_EQ(no_spell("vietr"), "vietr");  // r vẫn không đánh được vào coda tắc
  EXPECT_EQ(no_spell("asss"), "ass");

  // ---- Tùy chọn: khôi phục từ không phải tiếng Việt ------------------------
  {
    hodion::Config cfg;
    cfg.restore_non_vn = true;
    EXPECT_EQ(commit_word("boxing", cfg), "boxing");
    EXPECT_EQ(commit_word("hello", cfg), "hello");
    // "test" gõ Telex ra "tét" — một âm tiết tiếng Việt hợp lệ, nên khôi
    // phục tự động không đụng tới (đúng như UniKey).
    EXPECT_EQ(commit_word("test", cfg), "tét");
    EXPECT_EQ(commit_word("dog", cfg), "dog");      // không biến đổi gì → giữ nguyên
    EXPECT_EQ(commit_word("toans", cfg), "toán");   // từ hợp lệ → không khôi phục
    EXPECT_EQ(commit_word("nguyeenx", cfg), "nguyễn");
    EXPECT_EQ(commit_word("ddungs", cfg), "đúng");
    EXPECT_EQ(commit_word("ddd", cfg), "dd");       // đ đã bị hủy → không có dấu
  }

  // ---- Ký tự mở/không mở composition --------------------------------------
  {
    Engine e;
    EXPECT_TRUE(e.starts_word(U'a'));
    EXPECT_TRUE(e.starts_word(U'['));
    EXPECT_TRUE(!e.starts_word(U'1'));
    EXPECT_TRUE(!e.starts_word(U' '));
  }
  {
    hodion::Config cfg;
    cfg.method = hodion::InputMethod::Vni;
    Engine e(cfg);
    EXPECT_TRUE(!e.starts_word(U'1'));   // số không mở từ mới
    EXPECT_TRUE(!e.starts_word(U'['));   // VNI không dùng ngoặc
  }
  {
    hodion::Config cfg;
    cfg.telex_brackets = false;
    Engine e(cfg);
    EXPECT_TRUE(!e.starts_word(U'['));
  }

  // ---- Chuỗi rác không được làm engine hỏng -------------------------------
  EXPECT_EQ(telex("zzz"), "zzz");
  EXPECT_EQ(telex("sss"), "sss");
  EXPECT_EQ(telex("wwww"), "www");   // ư → w → w → w
  EXPECT_EQ(telex("aaaa"), "aaa");   // â → aa (hỏng) → phím sau là chữ thường
  // dd → đ → dd → phím d cuối lại thành đ theo luật viết tắt (như UniKey).
  EXPECT_EQ(telex("dddd"), "dđ");

  // ---- VNI: các ca khó -----------------------------------------------------
  EXPECT_EQ(vni("nguye64n"), "nguyễn");
  EXPECT_EQ(vni("NGUYE6N4"), "NGUYỄN");
  EXPECT_EQ(vni("uo7"), "ươ");
  EXPECT_EQ(vni("uo7i"), "ươi");
  EXPECT_EQ(vni("ngu7o7i2"), "người");
  EXPECT_EQ(vni("truo7ng2"), "trường");
  EXPECT_EQ(vni("quye6t1"), "quyết");
  EXPECT_EQ(vni("gie6ng1"), "giếng");
  EXPECT_EQ(vni("ba1ch"), "bách");
  EXPECT_EQ(vni("hoc5"), "học");
  EXPECT_EQ(vni("hoc2"), "hoc2");   // coda tắc không nhận huyền
  EXPECT_EQ(vni("ma1t"), "mát");
  EXPECT_EQ(vni("hoan2"), "hoàn");
  EXPECT_EQ(vni("hoa2n"), "hoàn");  // thanh nhảy khi thêm âm cuối
  EXPECT_EQ(vni("d9i5nh"), "định");

  // ---- Telex: ngoặc ghép với chữ ------------------------------------------
  EXPECT_EQ(telex("m]a"), "mưa");
  EXPECT_EQ(telex("n[i"), "nơi");
  EXPECT_EQ(telex("th]"), "thư");
  EXPECT_EQ(telex("m]ng"), "mưng");
  EXPECT_EQ(telex("l[ns"), "lớn");
  EXPECT_EQ(telex("{"), "Ơ");
  EXPECT_EQ(telex("}ngf"), "Ừng");

  // ---- Luật phụ âm đầu k ---------------------------------------------------
  EXPECT_EQ(telex("kes"), "ké");
  EXPECT_EQ(telex("kis"), "kí");
  EXPECT_EQ(telex("kas"), "kas");   // k không đi với vần dòng sau
  EXPECT_EQ(telex("kos"), "kos");

  // ---- oo → ô giữa từ ------------------------------------------------------
  EXPECT_EQ(telex("boongf"), "bồng");
  EXPECT_EQ(telex("coongj"), "cộng");
  EXPECT_EQ(telex("thuys"), "thúy");
  EXPECT_EQ(telex_modern("thuys"), "thuý");
  EXPECT_EQ(telex("tuyeetj"), "tuyệt");
  EXPECT_EQ(telex("nguwowfi"), "người");
  EXPECT_EQ(telex("truwowngf"), "trường");
  EXPECT_EQ(telex("phuowngr"), "phưởng");

  // ---- Gõ tiếp sau khi Backspace ------------------------------------------
  {
    Engine e;
    feed(e, "hoafn");
    e.process_backspace();
    EXPECT_EQ(u8(e.composition()), "hòa");
    feed(e, "n");
    EXPECT_EQ(u8(e.composition()), "hoàn");  // thanh lại nhảy đúng
  }
  {
    Engine e;
    feed(e, "ddwowngf");   // đường gõ kiểu khác
    EXPECT_EQ(u8(e.composition()), "đường");
  }

  // ---- Luồng nhiều từ ------------------------------------------------------
  EXPECT_EQ(type_sentence("xin chaof cacs banj"), "xin chào các bạn");
  EXPECT_EQ(type_sentence("Hoafng Sa, Truwowngf Sa!"),
            "Hoàng Sa, Trường Sa!");
  // Chữ số không ngắt từ nhưng cũng không mở từ mới; "nheas" hỏng chính tả
  // (vần "ea" không có) nên s giữ nguyên là chữ thường.
  EXPECT_EQ(type_sentence("nawm 2026 nheas"), "năm 2026 nheas");
  EXPECT_EQ(type_sentence("nawm 2026 nhes"), "năm 2026 nhé");
  EXPECT_EQ(type_sentence("email: a@b.com"), "email: a@b.com");
  {
    hodion::Config cfg;
    cfg.method = hodion::InputMethod::Vni;
    EXPECT_EQ(type_sentence("chu1c mu72ng na8m mo71i", cfg),
              "chúc mừng năm mới");
  }
  {
    hodion::Config cfg;
    cfg.restore_non_vn = true;
    EXPECT_EQ(type_sentence("choi game online", cfg), "choi game online");
  }

  // ---- commit() giữa chừng (Enter, mất focus) -----------------------------
  {
    Engine e;
    feed(e, "vieej");
    EXPECT_EQ(u8(e.commit()), "việ");
    EXPECT_TRUE(!e.composing());
    EXPECT_EQ(u8(e.raw()), "");
    feed(e, "t");
    EXPECT_EQ(u8(e.composition()), "t");  // từ mới, không dính từ cũ
  }

  // ---- Engine di chuyển được (host giữ trong container) -------------------
  {
    Engine a;
    feed(a, "nuowcs");
    Engine b(std::move(a));
    EXPECT_EQ(u8(b.composition()), "nước");
    EXPECT_EQ(u8(b.raw()), "nuowcs");
  }

  // ---- C API: cờ tùy chọn -------------------------------------------------
  {
    hodion_engine* e = hodion_engine_create();
    char out[64] = {};
    hodion_engine_set_flag(e, HODION_FLAG_TELEX_BRACKETS, 0);
    // Tắt ngoặc: '[' trở lại là ký tự ngắt từ thường.
    for (const char* p = "ta"; *p; ++p) {
      hodion_engine_key(e, static_cast<uint32_t>(*p), out, sizeof(out));
    }
    const int action = hodion_engine_key(e, '[', out, sizeof(out));
    EXPECT_TRUE(action == HODION_ACTION_COMMIT);
    EXPECT_EQ(std::string(out), "ta[");

    hodion_engine_set_method(e, HODION_METHOD_VNI);
    for (const char* p = "viet65"; *p; ++p) {
      hodion_engine_key(e, static_cast<uint32_t>(*p), out, sizeof(out));
    }
    EXPECT_EQ(std::string(out), "việt");

    // Bộ đệm ra chật thì cắt bớt chứ không tràn.
    char tiny[4] = {};
    hodion_engine_composition(e, tiny, sizeof(tiny));
    EXPECT_TRUE(tiny[3] == '\0');
    hodion_engine_destroy(e);
  }

  // ---- Chuyển đổi UTF (host Windows dùng UTF-16, macOS/Linux dùng UTF-8) --
  {
    using hodion::utf::from_utf8;
    using hodion::utf::to_utf16;
    using hodion::utf::to_utf8;

    // 1, 2, 3 và 4 byte.
    EXPECT_EQ(to_utf8(U"A"), std::string("A"));
    EXPECT_TRUE(to_utf8(U"é").size() == 2);
    EXPECT_TRUE(to_utf8(U"ệ").size() == 3);
    EXPECT_TRUE(to_utf8(U"\U0001D11E").size() == 4);  // ngoài BMP

    // Khứ hồi.
    const std::u32string vn = U"Tiếng Việt: đường phượng bay";
    EXPECT_TRUE(from_utf8(to_utf8(vn)) == vn);
    const std::u32string wide = U"aéệ\U0001D11E";
    EXPECT_TRUE(from_utf8(to_utf8(wide)) == wide);

    // UTF-16: ngoài BMP phải thành cặp thay thế đúng chuẩn.
    const std::u16string u16 = to_utf16(U"\U0001D11E");
    EXPECT_TRUE(u16.size() == 2);
    EXPECT_TRUE(u16[0] == 0xD834 && u16[1] == 0xDD1E);
    EXPECT_TRUE(to_utf16(U"ệ").size() == 1);

    // UTF-16 chiều ngược: khứ hồi, kể cả ngoài BMP.
    using hodion::utf::from_utf16;
    EXPECT_TRUE(from_utf16(to_utf16(vn)) == vn);
    EXPECT_TRUE(from_utf16(to_utf16(wide)) == wide);
    EXPECT_TRUE(from_utf16(std::u16string()).empty());
    // Nửa cao cụt, nửa thấp lạc: bỏ đúng phần hỏng, giữ phần lành.
    EXPECT_TRUE(from_utf16(std::u16string{0xD834}).empty());
    EXPECT_TRUE(from_utf16(std::u16string{0xDD1E}).empty());
    EXPECT_TRUE(from_utf16(std::u16string{u'A', 0xD834, u'B'}) == U"AB");
    EXPECT_TRUE(from_utf16(std::u16string{u'A', 0xDD1E, u'B'}) == U"AB");
    // Nửa cao theo sau bởi nửa cao: cái đầu bị bỏ, cái sau ghép tiếp.
    EXPECT_TRUE(from_utf16(std::u16string{0xD834, 0xD834, 0xDD1E}) ==
                U"\U0001D11E");

    // Đầu vào hỏng thì bỏ qua, không treo và không đọc lố.
    EXPECT_TRUE(from_utf8(std::string("\xC3")).empty());          // cụt
    EXPECT_TRUE(from_utf8(std::string("\xFF\xFE")).empty());      // byte lạ
    // Byte nối sai: bỏ đúng phần hỏng rồi đọc lại — 0x28 là dấu '(' hợp lệ.
    EXPECT_TRUE(from_utf8(std::string("\xE1\x28" "A")) == U"(A");
    EXPECT_TRUE(from_utf8(std::string("A\x80" "B")) == U"AB");    // byte nối lạc
  }

  // Fuzz sâu hơn nằm ở test_fuzz.cpp.
}
