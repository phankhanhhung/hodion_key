// Mô hình 3-gram âm tiết cho việc đoán dấu theo ngữ cảnh.
//
// Vì sao n-gram chứ không phải mô hình neural: xem
// docs/roadmap-smart-input.md mục 3. Tóm tắt — không GPU, không runtime ML,
// train vài phút trên CPU, debug được bằng mắt, và khoảng cách vài phần
// trăm không đáng đánh đổi lấy cả chuỗi phụ thuộc của một runtime neural
// nằm trong bộ gõ.
//
// Mô hình nạp lúc chạy từ file cạnh exe, KHÔNG biên dịch vào: dữ liệu huấn
// luyện có giấy phép riêng (xem tools/train_ngram.py), và để file rời thì
// đổi mô hình không phải dịch lại gì.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "hodion/engine.h"

namespace hodion {

class NgramModel {
 public:
  static constexpr int kNoWord = -1;

  // Nạp từ nội dung file nhị phân. Nhận NỘI DUNG chứ không nhận đường dẫn:
  // đọc file là việc của tầng host.
  bool load(const std::string& bytes);
  bool empty() const { return vocab_.empty(); }
  size_t vocab_size() const { return vocab_.size(); }
  size_t bigram_count() const { return bigram_keys_.size(); }
  size_t trigram_count() const { return trigram_keys_.size(); }

  // Id của một âm tiết (chữ thường), hoặc kNoWord.
  int id(const std::u32string& syllable) const;
  // Id của mốc đầu câu.
  int bos() const { return bos_; }

  // logP(c | a, b), tự lùi bậc khi thiếu dữ liệu. a/b có thể là kNoWord.
  float score(int a, int b, int c) const;

 private:
  float unigram(int c) const;
  bool lookup_bigram(int b, int c, float* out) const;
  bool lookup_trigram(int a, int b, int c, float* out) const;

  std::vector<std::u32string> vocab_;  // xếp tăng dần, id = chỉ số
  std::vector<float> unigram_;
  std::vector<uint32_t> bigram_keys_;
  std::vector<float> bigram_scores_;
  std::vector<uint64_t> trigram_keys_;
  std::vector<float> trigram_scores_;
  int bos_ = kNoWord;
};

}  // namespace hodion
