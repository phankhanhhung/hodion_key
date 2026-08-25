#!/usr/bin/env python3
"""Dựng bảng âm tiết tiếng Việt CÓ THẬT cho phần đoán dấu.

    cmake --build build --target hodion_wordscan
    python3 tools/build_syllables.py --corpus vi.txt \
        --scan build/engine/hodion_wordscan \
        --out platform/windows/data/viet-syllables.txt

Khác nhau giữa "hợp lệ về cấu trúc" và "có thật" là điểm của cả bảng này.
Engine biết "duông" ghép đúng luật âm tiết, nhưng không biết nó có phải một
chữ người ta dùng hay không — đó là kiến thức từ vựng, không suy ra được từ
bảng vần. Bảng này mang đúng phần kiến thức đó.

NGUỒN: KHO VĂN BẢN, KHÔNG PHẢI TỪ ĐIỂN — và đó là chuyện giấy phép

Bản trước lấy từ từ điển chính tả hunspell-vi, mà nó là **GPL-2**: đưa dữ
liệu dẫn xuất vào đây sẽ kéo GPL-2 lên cả dự án, nên bảng không phát hành
kèm được, nên tính năng đoán dấu không đến tay ai. Một tính năng không giao
được thì coi như chưa làm.

Nay bảng sinh từ hai thứ đều dùng lại được: kho văn bản (Wikipedia tiếng
Việt, CC BY-SA) và chính bộ luật gõ của engine. Kết quả phát hành kèm được
dưới CC BY-SA — xem platform/windows/data/GIAY-PHEP-DU-LIEU.txt.

Ba lớp lọc, mỗi lớp bỏ một loại rác khác nhau:

1. **Cắt âm tiết** — chỉ giữ token toàn chữ cái tiếng Việt.
2. **Engine gõ ra được** — kho văn bản đầy chữ Latin không dấu của ngôn ngữ
   khác ("internet", "quantum") mà chỉ gồm chữ có trong bảng chữ tiếng
   Việt, regex không phân biệt được. Lọc này cũng giết luôn lỗi đánh máy
   sai vị trí dấu ("qúa" cạnh "quá") vì engine không bao giờ sinh ra chúng.
3. **Ngưỡng tần suất** — cái gì xuất hiện vài lần trong nửa tỉ ký tự thì
   nhiều khả năng là lỗi đánh máy vừa vặn hợp lệ, không phải từ.

Mặc định ngưỡng 5 là đo ra chứ không chọn bừa. Trên 619.724 âm tiết ngoài
dữ liệu train, so với chính bảng cũ dẫn xuất từ hunspell:

    ngưỡng   cỡ bảng   đúng khi đổi   Viterbi cả câu
    (cũ)     6.502     96,5%          94,8%
    2        8.506     96,4%          94,5%
    5        7.066     96,6%          95,1%

Ngưỡng 5 hơn bảng cũ ở cả hai con số, mà lại chấm điểm được nhiều chỗ hơn
(619.724 so với 615.578 âm tiết) — tức là nó biết nhiều từ có thật hơn.

Dùng lại nguyên phần cắt âm tiết của train_ngram.py chứ không chép sang:
hai bảng phải nhìn kho văn bản theo ĐÚNG một cách, lệch nhau là mô hình
chấm điểm cho những chữ mà bảng bảo không tồn tại.
"""
import argparse
import collections
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import train_ngram  # noqa: E402


def count_tokens(corpus_path, limit_bytes):
    counts = collections.Counter()
    read = 0
    with open(corpus_path, encoding="utf-8", errors="ignore") as src:
        for line in src:
            read += len(line)
            if limit_bytes and read > limit_bytes:
                break
            for sentence in train_ngram.syllables(line):
                counts.update(sentence)
    return counts


def emit(path, words, stats):
    """Một âm tiết mỗi dòng, UTF-8. Cố ý là file văn bản thường: đổi bảng
    không phải dịch lại gì, và ai cũng mở ra xem được nó chứa gì."""
    lines = [
        "# Bảng âm tiết tiếng Việt cho HodionKey — SINH TỰ ĐỘNG.",
        "# Dựng lại: python3 tools/build_syllables.py (xem chú thích ở đó).",
        "#",
        "# Nguồn: Wikipedia tiếng Việt (CC BY-SA) lọc qua bộ luật gõ của",
        "# engine. Giấy phép và ghi nguồn: GIAY-PHEP-DU-LIEU.txt cạnh file này.",
        "#",
        "#   %d âm tiết" % len(words),
        "#   token khác nhau trong kho: %d" % stats["seen"],
        "#   engine gõ ra được:         %d" % stats["valid"],
        "#   đủ ngưỡng %d lần:%s%d" % (stats["min_count"],
                                       " " * (11 - len(str(stats["min_count"]))),
                                       len(words)),
        "",
    ]
    lines.extend(words)
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--corpus", required=True)
    ap.add_argument("--scan", required=True,
                    help="đường dẫn hodion_wordscan (lọc bằng bộ luật gõ)")
    ap.add_argument("--out", required=True)
    ap.add_argument("--min-count", type=int, default=5,
                    help="số lần tối thiểu trong kho văn bản (mặc định 5)")
    ap.add_argument("--limit-bytes", type=int, default=0)
    args = ap.parse_args()

    counts = count_tokens(args.corpus, args.limit_bytes)
    print("token khác nhau: %d" % len(counts))

    valid = train_ngram.valid_syllables(args.scan, set(counts))
    in_corpus = [w for w in counts if w in valid]
    print("  engine gõ ra được: %d" % len(in_corpus))

    words = sorted(w for w in in_corpus if counts[w] >= args.min_count)
    print("  đủ ngưỡng %d lần:  %d" % (args.min_count, len(words)))
    if not words:
        sys.exit("không còn mục nào — kiểm tra lại kho văn bản")

    emit(args.out, words, {"seen": len(counts), "valid": len(in_corpus),
                           "min_count": args.min_count})
    print("đã ghi %s" % args.out)


if __name__ == "__main__":
    main()
