#include "test_util.h"

void run_telex_tests() {
  // ---- Từ tiêu biểu --------------------------------------------------------
  EXPECT_EQ(telex("vieejt"), "việt");
  EXPECT_EQ(telex("vieet"), "viêt");
  EXPECT_EQ(telex("vietj"), "viẹt");
  EXPECT_EQ(telex("viejt"), "viẹt");     // thanh di chuyển khi thêm âm cuối
  EXPECT_EQ(telex("ddaay"), "đây");
  EXPECT_EQ(telex("dday"), "đay");
  EXPECT_EQ(telex("toans"), "toán");
  EXPECT_EQ(telex("toasn"), "toán");     // thanh di chuyển o→a khi có coda
  EXPECT_EQ(telex("muoons"), "muốn");
  EXPECT_EQ(telex("nguyeenx"), "nguyễn");
  EXPECT_EQ(telex("khuya"), "khuya");
  EXPECT_EQ(telex("hoacwj"), "hoặc");
  EXPECT_EQ(telex("quyeenr"), "quyển");
  EXPECT_EQ(telex("nghieeng"), "nghiêng");

  // ---- Gõ dấu tự do (freeMarking) — dấu ở cuối từ -------------------------
  EXPECT_EQ(telex("viete"), "viêt");     // mũ đánh xuyên qua âm cuối
  EXPECT_EQ(telex("vietje"), "việt");
  EXPECT_EQ(telex("dund"), "đun");       // d cuối từ → đ đầu từ
  EXPECT_EQ(telex("dduongwf"), "đường");
  EXPECT_EQ(telex("duongwdf"), "đường"); // cả đ lẫn thanh đều gõ cuối từ được

  // ---- Họ vần uo / ưo / uơ / ươ -------------------------------------------
  EXPECT_EQ(telex("nuowcs"), "nước");
  EXPECT_EQ(telex("nuocws"), "nước");    // w sau âm cuối vẫn áp cho uo
  EXPECT_EQ(telex("huow"), "hươ");
  EXPECT_EQ(telex("thuow"), "thuơ");     // ngoại lệ "th" → uơ (cho thuở)
  EXPECT_EQ(telex("thuowr"), "thuở");
  EXPECT_EQ(telex("thuowng"), "thương"); // uơ tự thành ươ khi có âm cuối
  EXPECT_EQ(telex("thuongw"), "thương"); // có coda thì không dính ngoại lệ th
  EXPECT_EQ(telex("tuoiw"), "tươi");
  EXPECT_EQ(telex("ruouwj"), "rượu");
  EXPECT_EQ(telex("uow"), "ươ");
  EXPECT_EQ(telex("uoww"), "uow");       // gõ lặp w hủy cả cụm ươ

  // ---- qu / gi -------------------------------------------------------------
  EXPECT_EQ(telex("quas"), "quá");
  EXPECT_EQ(telex("quys"), "quý");
  EXPECT_EQ(telex("quynhf"), "quỳnh");   // ngoại lệ quyn/quynh
  EXPECT_EQ(telex("gias"), "giá");
  EXPECT_EQ(telex("gif"), "gì");
  EXPECT_EQ(telex("gifa"), "già");       // thanh chuyển từ i sang a
  EXPECT_EQ(telex("ginf"), "gìn");
  EXPECT_EQ(telex("giuwax"), "giữa");
  EXPECT_EQ(telex("gieengs"), "giếng");  // ngoại lệ gien/gieng

  // ---- Vị trí dấu thanh: kiểu cũ / kiểu mới -------------------------------
  EXPECT_EQ(telex("hoaf"), "hòa");
  EXPECT_EQ(telex_modern("hoaf"), "hoà");
  EXPECT_EQ(telex("khoer"), "khỏe");
  EXPECT_EQ(telex_modern("khoer"), "khoẻ");
  EXPECT_EQ(telex("tuyf"), "tùy");
  EXPECT_EQ(telex_modern("tuyf"), "tuỳ");
  EXPECT_EQ(telex("hoafn"), "hoàn");     // có âm cuối: hai kiểu như nhau
  EXPECT_EQ(telex_modern("hoafn"), "hoàn");
  EXPECT_EQ(telex("mais"), "mái");
  EXPECT_EQ(telex("cuar"), "của");
  EXPECT_EQ(telex("ngoaif"), "ngoài");
  EXPECT_EQ(telex("chiuj"), "chịu");

  // ---- Luật hủy khi gõ lặp -------------------------------------------------
  EXPECT_EQ(telex("aa"), "â");
  EXPECT_EQ(telex("aaa"), "aa");
  EXPECT_EQ(telex("aas"), "ấ");
  EXPECT_EQ(telex("as"), "á");
  EXPECT_EQ(telex("ass"), "as");
  EXPECT_EQ(telex("asss"), "ass");       // từ đã hỏng → s thứ ba là chữ thường
  EXPECT_EQ(telex("aw"), "ă");
  EXPECT_EQ(telex("aww"), "aw");
  EXPECT_EQ(telex("awa"), "â");          // mũ thay trăng
  EXPECT_EQ(telex("dd"), "đ");
  EXPECT_EQ(telex("ddd"), "dd");
  EXPECT_EQ(telex("xooong"), "xoong");   // ooo hủy mũ → gõ được xoong
  EXPECT_EQ(telex("xoong"), "xông");     // oo luôn thành ô (như UniKey)

  // ---- w ------------------------------------------------------------------
  EXPECT_EQ(telex("w"), "ư");
  EXPECT_EQ(telex("ww"), "w");
  EXPECT_EQ(telex("www"), "ww");
  EXPECT_EQ(telex("tw"), "tư");          // w sau phụ âm → ư
  EXPECT_EQ(telex("tww"), "tw");
  EXPECT_EQ(telex("thw"), "thư");
  EXPECT_EQ(telex("wow"), "ươ");         // ư + o + w → ươ
  EXPECT_EQ(telex("wa"), "ưa");
  EXPECT_EQ(telex("uw"), "ư");
  EXPECT_EQ(telex("uww"), "uw");
  EXPECT_EQ(telex("mua"), "mua");
  EXPECT_EQ(telex("muaw"), "mưa");       // móc đánh vào u trong "ua"

  // ---- Telex đầy đủ: [ ] { } ----------------------------------------------
  EXPECT_EQ(telex("t["), "tơ");
  EXPECT_EQ(telex("]"), "ư");
  EXPECT_EQ(telex("m]ngf"), "mừng");
  EXPECT_EQ(telex("[o"), "ô");           // roof thay móc trên ơ

  // ---- Kiểm tra chính tả giữ từ tiếng Anh ---------------------------------
  // Lưu ý: freeMarking (mặc định bật, như UniKey) cho phép d cuối từ → đ,
  // nên "did" thành "đi". Tắt freeMarking thì giữ nguyên.
  EXPECT_EQ(telex("did"), "đi");
  EXPECT_EQ(telex("dodo"), "đô");
  {
    hodion::Config cfg;
    cfg.free_marking = false;
    EXPECT_EQ(type_word("did", cfg), "did");
    EXPECT_EQ(type_word("dd", cfg), "đ");  // dd liền nhau vẫn hoạt động
  }
  EXPECT_EQ(telex("case"), "cáe");       // s vẫn ăn thanh trước khi hỏng (như UniKey)
  EXPECT_EQ(telex("vietr"), "vietr");    // coda tắc (t) không nhận hỏi
  EXPECT_EQ(telex("vietf"), "vietf");    // ... không nhận huyền
  EXPECT_EQ(telex("boxing"), "bõing");   // x ăn ngã trước khi từ hỏng (như UniKey;
                                         // bật restore_non_vn sẽ trả lại "boxing")
  EXPECT_EQ(telex("add"), "add");

  // ---- đ trong viết tắt ----------------------------------------------------
  EXPECT_EQ(telex("hdd"), "hđ");         // đ sau chuỗi không phải tiếng Việt

  // ---- Thanh trực tiếp, xóa thanh -----------------------------------------
  EXPECT_EQ(telex("asf"), "à");
  EXPECT_EQ(telex("asz"), "a");
  EXPECT_EQ(telex("az"), "az");

  // ---- Chữ hoa -------------------------------------------------------------
  EXPECT_EQ(telex("VIEEJT"), "VIỆT");
  EXPECT_EQ(telex("DDoongf"), "Đồng");
  EXPECT_EQ(telex("Vieejt"), "Việt");
  EXPECT_EQ(telex("W"), "Ư");
}
