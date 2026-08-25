#include "hodion/ngram.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "hodion/utf.h"

namespace hodion {
namespace {

// Phạt mỗi lần phải lùi một bậc (stupid backoff). 0,4 là giá trị quen dùng
// và bài toán này không nhạy với nó — ta chỉ cần thứ tự, không cần xác suất
// đúng chuẩn.
constexpr float kBackoff = -0.916290731874155f;  // log(0.4)

// Âm tiết chưa từng gặp: phải có một giá trị hữu hạn, đủ thấp để không bao
// giờ thắng một âm tiết đã gặp.
constexpr float kUnknown = -20.0f;

class Reader {
 public:
  Reader(const std::string& data) : data_(data) {}

  bool take(void* out, size_t n) {
    if (pos_ + n > data_.size()) return false;
    std::memcpy(out, data_.data() + pos_, n);
    pos_ += n;
    return true;
  }
  bool u32(uint32_t* out) { return take(out, 4); }
  bool u8v(uint8_t* out) { return take(out, 1); }
  bool bytes(std::string* out, size_t n) {
    if (pos_ + n > data_.size()) return false;
    out->assign(data_.data() + pos_, n);
    pos_ += n;
    return true;
  }
  // Đọc một mảng phần tử cố định. Kiểm tra tràn TRƯỚC khi cấp phát: độ dài
  // đọc từ file, mà file thì có thể hỏng hoặc bị sửa.
  template <typename T>
  bool array(std::vector<T>* out, uint32_t count) {
    if (static_cast<uint64_t>(count) * sizeof(T) > data_.size() - pos_) {
      return false;
    }
    out->resize(count);
    return count == 0 || take(out->data(), count * sizeof(T));
  }

 private:
  const std::string& data_;
  size_t pos_ = 0;
};

}  // namespace

bool NgramModel::load(const std::string& bytes) {
  vocab_.clear();
  unigram_.clear();
  bigram_keys_.clear();
  bigram_scores_.clear();
  trigram_keys_.clear();
  trigram_scores_.clear();
  bos_ = kNoWord;

  Reader r(bytes);
  char magic[4];
  uint32_t version = 0, count = 0;
  if (!r.take(magic, 4) || std::memcmp(magic, "HKNG", 4) != 0) return false;
  if (!r.u32(&version) || version != 1) return false;

  if (!r.u32(&count) || count == 0 || count > 65535) return false;
  vocab_.reserve(count);
  for (uint32_t i = 0; i < count; ++i) {
    uint8_t len = 0;
    std::string word;
    if (!r.u8v(&len) || len == 0 || !r.bytes(&word, len)) return false;
    vocab_.push_back(utf::from_utf8(word));
  }
  // Từ vựng phải đã xếp tăng dần — tra cứu dựa vào đó.
  if (!std::is_sorted(vocab_.begin(), vocab_.end())) return false;

  if (!r.array(&unigram_, count)) return false;

  uint32_t n = 0;
  if (!r.u32(&n) || !r.array(&bigram_keys_, n) ||
      !r.array(&bigram_scores_, n)) {
    return false;
  }
  if (!std::is_sorted(bigram_keys_.begin(), bigram_keys_.end())) return false;

  if (!r.u32(&n) || !r.array(&trigram_keys_, n) ||
      !r.array(&trigram_scores_, n)) {
    return false;
  }
  if (!std::is_sorted(trigram_keys_.begin(), trigram_keys_.end())) {
    return false;
  }

  bos_ = id(utf::from_utf8("<s>"));
  return !vocab_.empty();
}

int NgramModel::id(const std::u32string& syllable) const {
  const auto it =
      std::lower_bound(vocab_.begin(), vocab_.end(), syllable);
  if (it == vocab_.end() || *it != syllable) return kNoWord;
  return static_cast<int>(it - vocab_.begin());
}

float NgramModel::unigram(int c) const {
  if (c < 0 || static_cast<size_t>(c) >= unigram_.size()) return kUnknown;
  return unigram_[c];
}

bool NgramModel::lookup_bigram(int b, int c, float* out) const {
  if (b < 0 || c < 0) return false;
  const uint32_t key = (static_cast<uint32_t>(b) << 16) |
                       static_cast<uint32_t>(c);
  const auto it =
      std::lower_bound(bigram_keys_.begin(), bigram_keys_.end(), key);
  if (it == bigram_keys_.end() || *it != key) return false;
  *out = bigram_scores_[it - bigram_keys_.begin()];
  return true;
}

bool NgramModel::lookup_trigram(int a, int b, int c, float* out) const {
  if (a < 0 || b < 0 || c < 0) return false;
  const uint64_t key = (static_cast<uint64_t>(a) << 32) |
                       (static_cast<uint64_t>(b) << 16) |
                       static_cast<uint64_t>(c);
  const auto it =
      std::lower_bound(trigram_keys_.begin(), trigram_keys_.end(), key);
  if (it == trigram_keys_.end() || *it != key) return false;
  *out = trigram_scores_[it - trigram_keys_.begin()];
  return true;
}

float NgramModel::score(int a, int b, int c) const {
  if (c < 0) return kUnknown;
  float value = 0;
  if (lookup_trigram(a, b, c, &value)) return value;
  if (lookup_bigram(b, c, &value)) return kBackoff + value;
  return 2 * kBackoff + unigram(c);
}

}  // namespace hodion
