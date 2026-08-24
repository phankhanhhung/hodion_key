#include "hodion/english_words.h"

#include <cstddef>
#include <cstring>

namespace hodion {
namespace {

#include "english_telex.inc"

constexpr size_t kBlockCount = sizeof(kBlocks) / sizeof(kBlocks[0]);
static_assert(kBlockCount == kMaxLen - kMinLen + 1, "bảng khối lệch độ dài");

class EnglishWords final : public ForeignWords {
 public:
  bool contains(const std::u32string& key) const override {
    const size_t n = key.size();
    if (n < kMinLen || n > kMaxLen) return false;

    char buf[kMaxLen];
    for (size_t i = 0; i < n; ++i) {
      const char32_t c = key[i];
      if (c < U'a' || c > U'z') return false;  // gọi sai hợp đồng
      buf[i] = static_cast<char>(c);
    }

    // Bản ghi trong khối dài đúng n byte và đã xếp tăng dần, nên tìm nhị
    // phân chạy thẳng trên mảng, không cần bảng chỉ mục nào.
    const Block& block = kBlocks[n - kMinLen];
    size_t lo = 0, hi = block.count;
    while (lo < hi) {
      const size_t mid = lo + (hi - lo) / 2;
      const int cmp = std::memcmp(buf, block.data + mid * n, n);
      if (cmp == 0) return true;
      if (cmp < 0) {
        hi = mid;
      } else {
        lo = mid + 1;
      }
    }
    return false;
  }
};

const EnglishWords kInstance;

}  // namespace

const ForeignWords& english_words() { return kInstance; }

size_t english_words_count() {
  size_t total = 0;
  for (const Block& b : kBlocks) total += b.count;
  return total;
}

}  // namespace hodion
