// CLI demo: mỗi dòng stdin là một chuỗi phím, in ra chữ tiếng Việt tương ứng.
//   echo "vieejt nam" | ./hodion_demo            (Telex)
//   echo "vie6t5 nam" | ./hodion_demo --vni
//   echo "meeting luc 3h" | ./hodion_demo         (từ điển Anh, mặc định bật)
//   echo "meeting luc 3h" | ./hodion_demo --no-english
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>

#include "hodion/engine.h"
#include "hodion/english_words.h"
#include "hodion/utf.h"

int main(int argc, char** argv) {
  hodion::Config cfg;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--vni") == 0) cfg.method = hodion::InputMethod::Vni;
    if (std::strcmp(argv[i], "--modern") == 0) cfg.tone_style = hodion::ToneStyle::Modern;
    if (std::strcmp(argv[i], "--no-english") == 0) cfg.english_detect = false;
    if (std::strcmp(argv[i], "--restore") == 0) cfg.restore_non_vn = true;
  }

  hodion::Engine engine(cfg);
  engine.set_foreign_words(&hodion::english_words());
  std::string line;
  while (std::getline(std::cin, line)) {
    std::string out;
    for (char c : line) {
      const auto r = engine.process_char(static_cast<char32_t>(c));
      if (r.action == hodion::Engine::Result::Action::Commit) {
        out += hodion::utf::to_utf8(r.text);
      } else if (r.action == hodion::Engine::Result::Action::None) {
        out += c;
      }
    }
    out += hodion::utf::to_utf8(engine.commit());
    std::printf("%s\n", out.c_str());
  }
  return 0;
}
