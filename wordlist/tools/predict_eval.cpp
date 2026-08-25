// Đo độ chính xác của phần đoán dấu trên dữ liệu TÁCH RIÊNG khỏi lúc train.
//
//   predict_eval <viet-syllables.txt> <viet-ngram.bin> < heldout.txt
//
// Mỗi dòng stdin là văn bản tiếng Việt CÓ DẤU. Công cụ bỏ dấu đi, bắt mô
// hình đoán lại, rồi so với bản gốc.
//
// Đo hai chế độ, vì chúng phục vụ hai việc khác nhau:
//
//   ngữ cảnh trái  — mô phỏng đúng lúc đang gõ: chốt từng từ, không được
//                    sửa lại chữ đã chốt, và ngữ cảnh là những gì CHÍNH NÓ
//                    đã đoán (lỗi lan truyền, đúng như thực tế).
//   cả câu         — mô phỏng reconversion: đã có đủ chữ hai bên, Viterbi
//                    tối ưu chung.
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "hodion/ngram.h"
#include "hodion/predict.h"
#include "hodion/reconvert.h"
#include "hodion/utf.h"
#include "hodion/viet_words.h"

namespace {

std::string ReadFile(const char* path) {
  std::FILE* f = std::fopen(path, "rb");
  if (!f) return {};
  std::string out;
  char buf[65536];
  size_t n;
  while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
  std::fclose(f);
  return out;
}

std::vector<std::u32string> Split(const std::u32string& line) {
  std::vector<std::u32string> out;
  std::u32string cur;
  for (char32_t c : line) {
    if (c == U' ' || c == U'\t') {
      if (!cur.empty()) out.push_back(cur);
      cur.clear();
    } else {
      cur.push_back(c);
    }
  }
  if (!cur.empty()) out.push_back(cur);
  return out;
}

std::u32string Lower(const std::u32string& s) {
  return hodion::utf::from_utf8(hodion::utf::to_utf8(s));
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) {
    std::fprintf(stderr, "dùng: predict_eval <syllables.txt> <ngram.bin>\n");
    return 2;
  }

  hodion::SyllableList known;
  if (!known.load(ReadFile(argv[1]))) {
    std::fprintf(stderr, "không nạp được bảng âm tiết\n");
    return 1;
  }
  hodion::NgramModel model;
  if (!model.load(ReadFile(argv[2]))) {
    std::fprintf(stderr, "không nạp được mô hình\n");
    return 1;
  }
  std::fprintf(stderr, "bảng: %zu âm tiết; mô hình: %zu từ vựng, %zu bigram, "
                       "%zu trigram\n",
               known.size(), model.vocab_size(), model.bigram_count(),
               model.trigram_count());

  const hodion::Config cfg;
  const float margin = argc > 3 ? static_cast<float>(std::atof(argv[3])) : 1.0f;
  long total = 0, ambiguous = 0;
  long ctx_right = 0, ctx_answered = 0, model_answered = 0, model_right = 0;
  long sent_right = 0;
  long baseline_right = 0;  // không có mô hình: chỉ sửa được chỗ duy nhất

  std::string line;
  while (std::getline(std::cin, line)) {
    const std::vector<std::u32string> gold =
        Split(Lower(hodion::utf::from_utf8(line)));
    if (gold.size() < 3) continue;

    // Bỏ dấu HẾT (kể cả tên riêng, chữ ngoài từ điển — chúng vẫn là ngữ
    // cảnh), nhưng chỉ TÍNH ĐIỂM ở những vị trí mà từ điển biết. Đòi cả câu
    // phải nằm trong từ điển thì gần như không câu nào của Wikipedia lọt.
    std::vector<std::u32string> bare;
    std::vector<bool> scored;
    int scorable = 0;
    for (const std::u32string& g : gold) {
      bare.push_back(hodion::strip_diacritics(g));
      const bool ok = known.contains(g);
      scored.push_back(ok);
      if (ok) ++scorable;
    }
    if (scorable < 2) continue;

    // --- Ngữ cảnh trái, lỗi lan truyền ---
    std::vector<std::u32string> left;
    for (size_t i = 0; i < bare.size(); ++i) {
      if (scored[i]) ++total;
      const std::u32string unique =
          hodion::restore_diacritics(bare[i], cfg, known);
      const std::u32string guess =
          hodion::restore_in_context(left, bare[i], cfg, known, model, margin);

      const std::u32string base = unique.empty() ? bare[i] : unique;
      if (scored[i] && base == gold[i]) ++baseline_right;
      if (!unique.empty()) {
        // Không nhập nhằng — mô hình không cần vào.
        left.push_back(unique);
        if (scored[i] && unique == gold[i]) ++ctx_right;
        ++ctx_answered;
        continue;
      }
      if (scored[i]) ++ambiguous;
      if (guess.empty()) {
        left.push_back(bare[i]);
        if (scored[i] && bare[i] == gold[i]) ++ctx_right;
      } else {
        ++ctx_answered;
        left.push_back(guess);
        if (scored[i]) {
          ++model_answered;
          if (guess == gold[i]) {
            ++ctx_right;
            ++model_right;
          }
        }
      }
    }

    // --- Cả câu (Viterbi) ---
    const std::vector<std::u32string> whole =
        hodion::restore_sentence(bare, cfg, known, model);
    for (size_t i = 0; i < gold.size(); ++i) {
      if (scored[i] && whole[i] == gold[i]) ++sent_right;
    }
  }

  if (total == 0) {
    std::fprintf(stderr, "không có câu nào dùng được\n");
    return 1;
  }
  std::printf("âm tiết đo: %ld (nhập nhằng: %ld = %.1f%%)\n", total, ambiguous,
              100.0 * ambiguous / total);
  std::printf("  không mô hình (chỉ sửa chỗ duy nhất): %.1f%%\n",
              100.0 * baseline_right / total);
  std::printf("  ngữ cảnh trái, lỗi lan truyền:        %.1f%%\n",
              100.0 * ctx_right / total);
  std::printf("  cả câu (Viterbi, cho reconversion):   %.1f%%\n",
              100.0 * sent_right / total);
  std::printf("  khi mô hình CÓ đổi chữ (margin %.2f):\n", margin);
  std::printf("    đổi %ld lần / %ld chỗ nhập nhằng (%.1f%% số chỗ)\n",
              model_answered, ambiguous,
              ambiguous ? 100.0 * model_answered / ambiguous : 0.0);
  std::printf("    trong đó đúng %.1f%%\n",
              model_answered ? 100.0 * model_right / model_answered : 0.0);
  return 0;
}
