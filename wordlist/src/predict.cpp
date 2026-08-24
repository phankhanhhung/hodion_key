#include "hodion/predict.h"

#include <vector>

#include "hodion/reconvert.h"

namespace hodion {

std::u32string restore_diacritics(const std::u32string& word,
                                  const Config& cfg,
                                  const SyllableSet& known) {
  if (word.empty()) return {};
  // Đã có dấu thì người dùng đã nói rõ họ muốn gì.
  if (strip_diacritics(word) != word) return {};

  const std::vector<std::u32string> variants =
      syllable_variants(word, cfg, 128, &known);

  // Chỉ nhận khi có ĐÚNG MỘT âm tiết có thật. Hai cái trở lên là nhập
  // nhằng, và không có ngữ cảnh thì chọn bừa còn tệ hơn để nguyên.
  std::u32string answer;
  int found = 0;
  for (const std::u32string& v : variants) {
    if (!known.contains(v)) continue;
    if (++found > 1) return {};
    answer = v;
  }
  if (found != 1) return {};
  if (answer == word) return {};  // không có gì để đổi
  return answer;
}

}  // namespace hodion
