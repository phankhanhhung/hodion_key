// Khung test tối giản — không phụ thuộc thư viện ngoài.
#pragma once

#include <cstdio>
#include <string>

#include "hodion/engine.h"
#include "hodion/utf.h"

extern int g_failures;
extern int g_checks;

#define EXPECT_EQ(actual, expected)                                       \
  do {                                                                    \
    ++g_checks;                                                           \
    const auto a_ = (actual);                                             \
    const auto e_ = (expected);                                           \
    if (a_ != e_) {                                                       \
      ++g_failures;                                                       \
      std::printf("FAIL %s:%d\n  expected: %s\n  actual:   %s\n",         \
                  __FILE__, __LINE__, std::string(e_).c_str(),            \
                  std::string(a_).c_str());                               \
    }                                                                     \
  } while (0)

#define EXPECT_TRUE(cond)                                                 \
  do {                                                                    \
    ++g_checks;                                                           \
    if (!(cond)) {                                                        \
      ++g_failures;                                                       \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);         \
    }                                                                     \
  } while (0)

// Gõ chuỗi phím ASCII vào engine mới, trả về composition cuối dạng UTF-8.
inline std::string type_word(const std::string& keys, hodion::Config cfg) {
  hodion::Engine e(cfg);
  for (char c : keys) e.process_char(static_cast<char32_t>(c));
  return hodion::utf::to_utf8(e.composition());
}

inline std::string telex(const std::string& keys) {
  return type_word(keys, hodion::Config{});
}

inline std::string telex_modern(const std::string& keys) {
  hodion::Config cfg;
  cfg.tone_style = hodion::ToneStyle::Modern;
  return type_word(keys, cfg);
}

inline std::string vni(const std::string& keys) {
  hodion::Config cfg;
  cfg.method = hodion::InputMethod::Vni;
  return type_word(keys, cfg);
}
