// CLI demo: mỗi dòng stdin là một chuỗi phím, in ra chữ tiếng Việt tương ứng.
//   echo "vieejt nam" | ./hodion_demo            (Telex)
//   echo "vie6t5 nam" | ./hodion_demo --vni
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>

#include "hodion/engine.h"
#include "hodion/utf.h"

int main(int argc, char** argv) {
  hodion::Config cfg;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--vni") == 0) cfg.method = hodion::InputMethod::Vni;
    if (std::strcmp(argv[i], "--modern") == 0) cfg.tone_style = hodion::ToneStyle::Modern;
  }

  hodion::Engine engine(cfg);
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
