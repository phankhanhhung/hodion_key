#include "hodion/viet_words.h"

#include <algorithm>

#include "hodion/utf.h"

namespace hodion {

bool SyllableList::load(const std::string& utf8_contents) {
  items_.clear();

  size_t start = 0;
  while (start <= utf8_contents.size()) {
    size_t end = utf8_contents.find('\n', start);
    if (end == std::string::npos) end = utf8_contents.size();

    std::string line = utf8_contents.substr(start, end - start);
    start = end + 1;
    // File có thể sinh trên Windows: bỏ '\r' cuối dòng.
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
      line.pop_back();
    }
    if (line.empty() || line[0] == '#') continue;
    items_.push_back(utf::from_utf8(line));
  }

  std::sort(items_.begin(), items_.end());
  items_.erase(std::unique(items_.begin(), items_.end()), items_.end());
  return !items_.empty();
}

bool SyllableList::contains(const std::u32string& syllable) const {
  return std::binary_search(items_.begin(), items_.end(), syllable);
}

}  // namespace hodion
