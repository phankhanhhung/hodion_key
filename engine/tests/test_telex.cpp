#include "test_util.h"

void run_telex_tests() {
  // Từ tiêu biểu.
  EXPECT_EQ(telex("vieejt"), "việt");
  EXPECT_EQ(telex("vietj"), "viẹt");
  EXPECT_EQ(telex("ddaay"), "đây");
  EXPECT_EQ(telex("dday"), "đay");
  EXPECT_EQ(telex("nuowcs"), "nước");
  EXPECT_EQ(telex("nuocws"), "nước");
  EXPECT_EQ(telex("thuowng"), "thương");
  EXPECT_EQ(telex("dduowngf"), "đường");
  EXPECT_EQ(telex("ruowuj"), "rượu");
  EXPECT_EQ(telex("nguyeenx"), "nguyễn");
  EXPECT_EQ(telex("khuya"), "khuya");
  EXPECT_EQ(telex("toans"), "toán");
  EXPECT_EQ(telex("muoons"), "muốn");
  EXPECT_EQ(telex("hoacwj"), "hoặc");

  // Chữ hoa.
  EXPECT_EQ(telex("VIEEJT"), "VIỆT");
  EXPECT_EQ(telex("DDoongf"), "Đồng");
  EXPECT_EQ(telex("Vieejt"), "Việt");

  // qu / gi.
  EXPECT_EQ(telex("quas"), "quá");
  EXPECT_EQ(telex("quyeens"), "quyến");
  EXPECT_EQ(telex("quys"), "quý");
  EXPECT_EQ(telex("gias"), "giá");
  EXPECT_EQ(telex("gif"), "gì");
  EXPECT_EQ(telex("giuwax"), "giữa");
  EXPECT_EQ(telex("gieengs"), "giếng");

  // Vị trí dấu thanh: kiểu cũ / kiểu mới.
  EXPECT_EQ(telex("hoaf"), "hòa");
  EXPECT_EQ(telex_modern("hoaf"), "hoà");
  EXPECT_EQ(telex("khoer"), "khỏe");
  EXPECT_EQ(telex_modern("khoer"), "khoẻ");
  EXPECT_EQ(telex("tuyf"), "tùy");
  EXPECT_EQ(telex_modern("tuyf"), "tuỳ");
  // Có âm cuối thì hai kiểu như nhau.
  EXPECT_EQ(telex("toafn"), "toàn");
  EXPECT_EQ(telex_modern("toans"), "toán");

  // Nguyên âm đôi/ba không dấu phụ.
  EXPECT_EQ(telex("mais"), "mái");
  EXPECT_EQ(telex("maus"), "máu");
  EXPECT_EQ(telex("cuar"), "của");
  EXPECT_EQ(telex("nguoif"), "nguòi");  // chưa có ươ thì thanh theo luật ba nguyên âm
  EXPECT_EQ(telex("ngoaif"), "ngoài");
  EXPECT_EQ(telex("chiuj"), "chịu");

  // Luật hủy khi gõ lặp.
  EXPECT_EQ(telex("aa"), "â");
  EXPECT_EQ(telex("aaa"), "aa");
  EXPECT_EQ(telex("aas"), "ấ");
  EXPECT_EQ(telex("as"), "á");
  EXPECT_EQ(telex("ass"), "as");
  EXPECT_EQ(telex("aw"), "ă");
  EXPECT_EQ(telex("aww"), "aw");
  EXPECT_EQ(telex("w"), "ư");
  EXPECT_EQ(telex("ww"), "w");
  EXPECT_EQ(telex("uw"), "ư");
  EXPECT_EQ(telex("uww"), "uw");
  EXPECT_EQ(telex("dd"), "đ");
  EXPECT_EQ(telex("ddd"), "dd");
  EXPECT_EQ(telex("xooong"), "xoong");  // ooo hủy mũ → gõ được "xoong"

  // Đổi thanh trực tiếp, xóa thanh bằng z.
  EXPECT_EQ(telex("asf"), "à");
  EXPECT_EQ(telex("asz"), "a");
  EXPECT_EQ(telex("az"), "az");

  // d cuối từ → đ (delayed).
  EXPECT_EQ(telex("dund"), "đun");
  EXPECT_EQ(telex("add"), "add");  // đ chỉ áp cho d đầu từ — từ tiếng Anh không bị phá

  // Phím thanh khi chưa có nguyên âm → chữ thường.
  EXPECT_EQ(telex("s"), "s");
  EXPECT_EQ(telex("str"), "str");
}
