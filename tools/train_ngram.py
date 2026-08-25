#!/usr/bin/env python3
"""Train mô hình 3-gram âm tiết để đoán dấu tiếng Việt theo ngữ cảnh.

    cmake --build build --target hodion_wordscan
    python3 tools/train_ngram.py --corpus vi.txt \
        --scan build/engine/hodion_wordscan --out viet-ngram.bin

Rồi đặt viet-ngram.bin CẠNH HodionKeyConfig.exe.

Vì sao là n-gram âm tiết chứ không phải mô hình neural: xem
docs/roadmap-smart-input.md mục 3. Tóm tắt — tỉ lệ giá trị trên chi phí áp
đảo. Không GPU, không runtime ML, train vài phút trên CPU, debug được bằng
mắt, và khoảng cách 94% → 97% không đáng đánh đổi lấy cả chuỗi phụ thuộc
của một runtime neural trong một bộ gõ.

HAI CỠ, VÀ CON SỐ ĐỂ CHỌN

    --preset full    ~46 MB   (mặc định)
    --preset small   ~13 MB

Cắt bớt bằng cách bỏ những n-gram hiếm (min-bigram 4, min-trigram 12).
Đo trên 619.724 âm tiết ngoài dữ liệu train, cùng một bảng âm tiết:

    preset   cỡ       có đổi   đúng khi đổi   đúng chung   Viterbi
    full     46,3 MB  54,0%    96,6%          63,8%        95,1%
    small    12,6 MB  52,8%    96,3%          62,6%        94,2%

Nhỏ đi 3,7 lần mà chỉ mất 0,3 điểm ở con số quan trọng nhất (đúng khi ra
tay). Với một file phải tải về và nằm thường trú trong RAM thì đó là món
hời — nên "small" mới là bản nên phát hành, "full" để cho ai có sẵn chỗ.

GIẤY PHÉP

Mô hình sinh từ Wikipedia tiếng Việt (CC BY-SA) nên nó cũng CC BY-SA:
phát hành lại được, miễn ghi nguồn và giữ nguyên giấy phép. Xem
wordlist/data/GIAY-PHEP-DU-LIEU.txt.

File không nằm trong repo, nhưng vì kỹ thuật chứ không phải vì giấy phép:
một khối nhị phân mấy chục MB đổi trọn gói mỗi lần train thì không thuộc
về git.

Khuôn dạng file (little-endian):

    "HKNG"  u32 phiên bản = 1
    u32 số âm tiết trong từ vựng
      mỗi mục: u8 độ dài + bytes UTF-8   (xếp tăng dần, id = chỉ số)
    f32 [số âm tiết]                     logP(w)
    u32 số bigram ; u32 khoá[]  ; f32 điểm[]   khoá = a<<16 | b
    u32 số trigram; u64 khoá[]  ; f32 điểm[]   khoá = a<<32 | b<<16 | c

Điểm là logP có điều kiện: logP(b|a) cho bigram, logP(c|a,b) cho trigram.
Bộ giải mã tự áp phạt khi phải lùi bậc (stupid backoff).

Từ vựng dùng u16 nên tối đa 65.535 âm tiết — tiếng Việt có khoảng 7.000,
thừa sức. Nhờ vậy một trigram chỉ tốn 10 byte thay vì 16.
"""
import argparse
import collections
import math
import os
import re
import struct
import sys
import subprocess
import unicodedata

# Âm tiết tiếng Việt: chỉ chữ cái trong bảng chữ tiếng Việt (không f/j/w/z).
SYLLABLE = re.compile(
    r"^[aăâbcdđeêghiklmnoôơpqrstuưvxy"
    r"àáảãạằắẳẵặầấẩẫậèéẻẽẹềếểễệìíỉĩị"
    r"òóỏõọồốổỗộờớởỡợùúủũụừứửữựỳýỷỹỵ]+$")

BOS = "<s>"   # đầu câu, cũng là một "âm tiết" trong từ vựng


def strip_diacritics(s):
    s = s.replace("đ", "d")
    return "".join(c for c in unicodedata.normalize("NFD", s)
                   if unicodedata.category(c) != "Mn")


def syllables(line):
    """Cắt một dòng thành các câu, mỗi câu là danh sách âm tiết thường."""
    line = unicodedata.normalize("NFC", line.lower())
    for sentence in re.split(r"[.!?;:\n]+", line):
        out = []
        for token in sentence.split():
            token = token.strip("\"'()[]{}«»…,-–—*")
            if SYLLABLE.match(token):
                out.append(token)
            elif out:
                # Gặp thứ không phải âm tiết (số, tên nước ngoài) thì cắt
                # câu ở đó: ghép hai bên lại thành ngữ cảnh giả là sai.
                yield out
                out = []
        if out:
            yield out


def tokenize_pass(corpus_path, tokens_path, min_sentence, limit_bytes):
    """Lượt 1: cắt âm tiết, ghi câu ra file tạm, thu thập các token khác nhau.

    Cắt âm tiết bằng regex là phần đắt nhất, nên chỉ làm MỘT lần; các lượt
    sau đọc lại file tạm (đã sạch, nhỏ hơn hẳn bản gốc).
    """
    seen = set()
    sentences = 0
    read = 0
    with open(corpus_path, encoding="utf-8", errors="ignore") as src, \
            open(tokens_path, "w", encoding="utf-8") as dst:
        for line in src:
            read += len(line)
            if limit_bytes and read > limit_bytes:
                break
            for sent in syllables(line):
                if len(sent) < min_sentence:
                    continue
                sentences += 1
                seen.update(sent)
                dst.write(" ".join(sent))
                dst.write("\n")
    return seen, sentences


def valid_syllables(scan_bin, tokens):
    """Lọc bằng chính engine: cái gì engine không gõ ra được thì không phải
    âm tiết tiếng Việt.

    Cần lọc vì kho văn bản đầy chữ Latin không dấu của ngôn ngữ khác —
    "internet", "quantum", "society" — mà chúng chỉ gồm chữ cái cũng có
    trong bảng chữ tiếng Việt nên regex không phân biệt được. Bỏ qua bước
    này thì từ vựng phình lên hơn 75.000 mục thay vì khoảng 7.000.
    """
    bares = sorted({strip_diacritics(t) for t in tokens})
    proc = subprocess.run([scan_bin, "--variants"],
                          input="\n".join(bares) + "\n",
                          capture_output=True, text=True, check=True)
    valid = set()
    for line in proc.stdout.splitlines():
        valid.update(line.split("\t")[1:])
    return valid


def unigram_pass(tokens_path, valid):
    uni = collections.Counter()
    with open(tokens_path, encoding="utf-8") as f:
        for line in f:
            words = [w for w in line.split() if w in valid]
            if not words:
                continue
            uni.update(words)
            uni[BOS] += 2
    return uni


def count_pass(tokens_path, index):
    """Lượt 2: đếm bigram/trigram theo ID đã đóng gói thành số nguyên.

    Khoá là số nguyên chứ không phải tuple chuỗi: cùng một lượng n-gram thì
    dict số nguyên nhẹ hơn nhiều lần, và đó là khác biệt giữa vừa RAM và
    không vừa.
    """
    bi = collections.Counter()
    tri = collections.Counter()
    bos = index[BOS]
    with open(tokens_path, encoding="utf-8") as f:
        for line in f:
            ids = [bos, bos]
            for w in line.split():
                i = index.get(w)
                if i is not None:
                    ids.append(i)
            for k in range(2, len(ids)):
                a, b, c = ids[k - 2], ids[k - 1], ids[k]
                bi[(b << 16) | c] += 1
                tri[(a << 32) | (b << 16) | c] += 1
            bi[(bos << 16) | bos] += 1
    return bi, tri


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--corpus", required=True)
    ap.add_argument("--scan", required=True,
                    help="đường dẫn hodion_wordscan (lọc âm tiết thật)")
    ap.add_argument("--out", required=True)
    ap.add_argument("--preset", choices=("full", "small"), default="full",
                    help="full = đầy đủ (~46 MB); small = cắt bớt (~13 MB)")
    ap.add_argument("--min-unigram", type=int, default=3)
    ap.add_argument("--min-bigram", type=int, default=None)
    ap.add_argument("--min-trigram", type=int, default=None)
    ap.add_argument("--min-sentence", type=int, default=3)
    ap.add_argument("--limit-bytes", type=int, default=0,
                    help="chỉ đọc ngần này byte đầu của kho văn bản")
    args = ap.parse_args()

    # Ngưỡng của preset "small" là đo ra: xem chú thích đầu file.
    defaults = {"full": (2, 2), "small": (4, 12)}[args.preset]
    if args.min_bigram is None:
        args.min_bigram = defaults[0]
    if args.min_trigram is None:
        args.min_trigram = defaults[1]

    tokens_path = args.out + ".tokens"
    seen, sentences = tokenize_pass(args.corpus, tokens_path,
                                    args.min_sentence, args.limit_bytes)
    print("câu: %d, token khác nhau: %d" % (sentences, len(seen)))

    valid = valid_syllables(args.scan, seen)
    print("trong đó là âm tiết tiếng Việt thật: %d" % len(seen & valid))

    uni = unigram_pass(tokens_path, valid)
    print("âm tiết khác nhau sau lọc: %d" % (len(uni) - 1))

    # Từ vựng: âm tiết đủ phổ biến. Âm tiết hiếm chỉ làm nhiễu và phình file.
    vocab = sorted(w for w, c in uni.items() if c >= args.min_unigram)
    if len(vocab) > 65535:
        sys.exit("từ vựng quá lớn cho id 16-bit: %d" % len(vocab))
    index = {w: i for i, w in enumerate(vocab)}
    print("từ vựng: %d âm tiết" % len(vocab))

    total = sum(uni[w] for w in vocab)
    uni_scores = [math.log(uni[w] / total) for w in vocab]
    uni_by_id = [uni[w] for w in vocab]

    bi, tri = count_pass(tokens_path, index)
    os.remove(tokens_path)

    bigrams = []
    for key, c in bi.items():
        if c < args.min_bigram:
            continue
        bigrams.append((key, math.log(c / uni_by_id[key >> 16])))
    bigrams.sort()
    print("bigram: %d" % len(bigrams))

    trigrams = []
    for key, n in tri.items():
        if n < args.min_trigram:
            continue
        prefix = bi.get(key >> 16, 0)   # đếm của (a, b)
        if prefix == 0:
            continue
        trigrams.append((key, math.log(n / prefix)))
    trigrams.sort()
    print("trigram: %d" % len(trigrams))

    with open(args.out, "wb") as f:
        f.write(b"HKNG")
        f.write(struct.pack("<I", 1))
        f.write(struct.pack("<I", len(vocab)))
        for w in vocab:
            raw = w.encode("utf-8")
            f.write(struct.pack("<B", len(raw)))
            f.write(raw)
        f.write(struct.pack("<%df" % len(vocab), *uni_scores))
        f.write(struct.pack("<I", len(bigrams)))
        f.write(struct.pack("<%dI" % len(bigrams), *(k for k, _ in bigrams)))
        f.write(struct.pack("<%df" % len(bigrams), *(s for _, s in bigrams)))
        f.write(struct.pack("<I", len(trigrams)))
        f.write(struct.pack("<%dQ" % len(trigrams), *(k for k, _ in trigrams)))
        f.write(struct.pack("<%df" % len(trigrams), *(s for _, s in trigrams)))

    print("đã ghi %s (%.1f MB)" % (args.out,
                                   os.path.getsize(args.out) / 1024 / 1024))


if __name__ == "__main__":
    main()
