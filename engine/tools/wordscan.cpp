// Công cụ dev: soi một danh sách từ qua engine để dựng/kiểm tra từ điển
// ngoại lai. Mỗi dòng stdin là một từ; in ra
//
//   <từ> <TAB> <chữ ghép ra> <TAB> vn|nonvn <TAB> same|diff
//
// "vn" = chữ ghép ra vẫn là âm tiết tiếng Việt hợp lệ (ca duy nhất mà luật
// cấu trúc không phát hiện được gì sai — chỉ từ điển mới cứu được).
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>

#include "hodion/engine.h"
#include "hodion/utf.h"

int main(int argc, char** argv) {
  hodion::Config cfg;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--vni") == 0) {
      cfg.method = hodion::InputMethod::Vni;
    }
    if (std::strcmp(argv[i], "--no-w") == 0) cfg.w_shorthand = false;
    if (std::strcmp(argv[i], "--no-free") == 0) cfg.free_marking = false;
  }

  std::string line;
  while (std::getline(std::cin, line)) {
    if (line.empty()) continue;
    hodion::Engine e(cfg);
    for (char32_t c : hodion::utf::from_utf8(line)) e.process_char(c);
    const std::string out = hodion::utf::to_utf8(e.composition());
    std::printf("%s\t%s\t%s\t%s\n", line.c_str(), out.c_str(),
                e.composing_is_vietnamese() ? "vn" : "nonvn",
                out == line ? "same" : "diff");
  }
  return 0;
}
