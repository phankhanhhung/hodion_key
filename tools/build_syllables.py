#!/usr/bin/env python3
"""Dựng bảng âm tiết tiếng Việt CÓ THẬT cho phần đoán dấu.

    apt-get install hunspell-vi        (hoặc lấy .dic tương đương)
    cmake --build build --target hodion_wordscan
    python3 tools/build_syllables.py \
        --dic  /usr/share/hunspell/vi_VN.dic \
        --scan build/engine/hodion_wordscan \
        --out  viet-syllables.txt

Rồi đặt viet-syllables.txt CẠNH HodionKeyConfig.exe.

⚠ GIẤY PHÉP — đọc trước khi dùng

File sinh ra KHÔNG được commit vào repo này, và đó là chủ ý. Từ điển chính
tả tiếng Việt của LibreOffice (gói hunspell-vi) mang giấy phép **GPL-2**
(mục `dictionaries/vi/*` trong file copyright — dòng MPL-2.0 ở đầu là cho
các từ điển khác). Đưa dữ liệu dẫn xuất từ nó vào mã nguồn sẽ kéo GPL-2 lên
cả dự án, và đó là quyết định của chủ dự án chứ không phải của script này.

Bạn tự sinh file trên máy mình thì không phát hành lại gì cả, nên không có
vấn đề gì. Nếu muốn PHÁT HÀNH kèm bảng này thì phải hoặc chấp nhận GPL-2,
hoặc tìm một nguồn từ vựng có giấy phép cho phép.

Không có file thì phần đoán dấu đơn giản là không bật được; mọi thứ còn lại
của bộ gõ chạy như thường.

Khác nhau giữa "hợp lệ về cấu trúc" và "có thật" là điểm của cả bảng này.
Engine biết "duông" ghép đúng luật âm tiết, nhưng không biết nó có phải một
chữ người ta dùng hay không — đó là kiến thức từ vựng, không suy ra được từ
bảng vần. Bảng này mang đúng phần kiến thức đó.

Hai lớp lọc:

1. Chỉ lấy mục là MỘT âm tiết thường, toàn chữ cái tiếng Việt.
2. Bỏ mục nào chính engine không sinh ra được — từ điển chính tả có sẵn
   những dạng engine không bao giờ gõ ra, giữ lại chỉ làm hai bên lệch nhau.
"""
import argparse
import re
import subprocess
import sys
import unicodedata

VN_LETTERS = re.compile(
    r"^[a-zàáâãèéêìíòóôõùúýăđĩũơưạảấầẩẫậắằẳẵặẹẻẽếềểễệỉịọỏốồổỗộớờởỡợụủứừửữựỳỵỷỹ]+$")


def strip_diacritics(s):
    s = s.replace("đ", "d")
    return "".join(c for c in unicodedata.normalize("NFD", s)
                   if unicodedata.category(c) != "Mn")


def load(path):
    out = set()
    with open(path, encoding="utf-8", errors="ignore") as f:
        next(f, None)  # dòng đầu của .dic là số lượng mục
        for line in f:
            word = line.strip().split("/")[0].strip()
            if word and VN_LETTERS.match(word):
                out.add(word)
    return sorted(out)


def keep_engine_reachable(scan_bin, words):
    """Giữ lại những âm tiết chính engine sinh ra được."""
    bares = sorted({strip_diacritics(w) for w in words})
    proc = subprocess.run([scan_bin, "--variants"], input="\n".join(bares) + "\n",
                          capture_output=True, text=True, check=True)
    reachable = set()
    for line in proc.stdout.splitlines():
        parts = line.split("\t")
        reachable.update(parts[1:])
    return [w for w in words if w in reachable]


def emit(path, words, dropped):
    """Một âm tiết mỗi dòng, UTF-8. Cố ý là file văn bản thường: đổi bảng
    không phải dịch lại gì, và sau này thay bằng mô hình tốt hơn cũng vậy."""
    lines = [
        "# Bảng âm tiết tiếng Việt cho HodionKey — SINH TỰ ĐỘNG.",
        "# Dựng lại: python3 tools/build_syllables.py (xem chú thích ở đó).",
        "#",
        "# Nguồn: từ điển chính tả tiếng Việt của LibreOffice (hunspell-vi),",
        "# giấy phép GPL-2. File này KHÔNG nằm trong mã nguồn HodionKey —",
        "# nếu bạn phát hành lại nó thì phải theo GPL-2.",
        "#",
        "#   %d âm tiết; đã bỏ %d mục engine không sinh ra được" %
        (len(words), dropped),
        "",
    ]
    lines.extend(sorted(words))
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dic", required=True)
    ap.add_argument("--scan", required=True)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    words = load(args.dic)
    print("âm tiết thường trong từ điển: %d" % len(words))
    kept = keep_engine_reachable(args.scan, words)
    dropped = len(words) - len(kept)
    print("  bỏ %d mục engine không sinh ra được" % dropped)
    print("  vào bảng: %d" % len(kept))
    if not kept:
        sys.exit("không còn mục nào — kiểm tra lại đường dẫn từ điển")
    emit(args.out, kept, dropped)
    print("đã ghi %s" % args.out)


if __name__ == "__main__":
    main()
