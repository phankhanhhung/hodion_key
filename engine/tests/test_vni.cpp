#include "test_util.h"

void run_vni_tests() {
  // ---- Từ tiêu biểu --------------------------------------------------------
  EXPECT_EQ(vni("vie6t5"), "việt");
  EXPECT_EQ(vni("viet65"), "việt");      // dấu ở cuối từ
  EXPECT_EQ(vni("d9a6y"), "đây");
  EXPECT_EQ(vni("day9"), "đay");         // 9 cuối từ → đ đầu từ
  EXPECT_EQ(vni("nu7o7c1"), "nước");
  EXPECT_EQ(vni("nuoc71"), "nước");
  EXPECT_EQ(vni("d9uong72"), "đường");
  EXPECT_EQ(vni("toan1"), "toán");
  EXPECT_EQ(vni("nguye6n4"), "nguyễn");
  EXPECT_EQ(vni("hoa2"), "hòa");
  EXPECT_EQ(vni("qua1"), "quá");
  EXPECT_EQ(vni("gi2"), "gì");
  EXPECT_EQ(vni("gi2a"), "già");
  EXPECT_EQ(vni("hoa8c5"), "hoặc");
  EXPECT_EQ(vni("thuo7"), "thuơ");       // ngoại lệ th → uơ
  EXPECT_EQ(vni("thuo73"), "thuở");
  EXPECT_EQ(vni("tuoi7"), "tươi");

  // ---- Kiểu mới ------------------------------------------------------------
  {
    hodion::Config cfg;
    cfg.method = hodion::InputMethod::Vni;
    cfg.tone_style = hodion::ToneStyle::Modern;
    EXPECT_EQ(type_word("hoa2", cfg), "hoà");
    EXPECT_EQ(type_word("tuy2", cfg), "tuỳ");
  }
  EXPECT_EQ(vni("tuy2"), "tùy");

  // ---- Chữ hoa -------------------------------------------------------------
  EXPECT_EQ(vni("VIE6T5"), "VIỆT");

  // ---- Luật hủy / số thành chữ số thường ----------------------------------
  EXPECT_EQ(vni("a11"), "a1");           // gõ lặp thanh → hủy + số thường
  EXPECT_EQ(vni("a66"), "a6");
  EXPECT_EQ(vni("a88"), "a8");
  EXPECT_EQ(vni("u77"), "u7");
  EXPECT_EQ(vni("d99"), "d9");
  EXPECT_EQ(vni("a7"), "a7");            // 7 không tạo ă (chỉ ơ/ư)
  EXPECT_EQ(vni("a12"), "à");            // đổi thanh trực tiếp
  EXPECT_EQ(vni("a10"), "a");            // 0 xóa thanh
  EXPECT_EQ(vni("a0"), "a0");            // chưa có thanh → 0 là chữ số

  // ---- Số không áp được → chữ số trong từ (từ đóng băng) ------------------
  EXPECT_EQ(vni("b1"), "b1");
  EXPECT_EQ(vni("x6"), "x6");
  EXPECT_EQ(vni("viet3"), "viet3");      // coda tắc không nhận hỏi

  // ---- Ở VNI, các phím Telex là chữ thường --------------------------------
  EXPECT_EQ(vni("as"), "as");
  EXPECT_EQ(vni("aa"), "aa");
  EXPECT_EQ(vni("w"), "w");
}
