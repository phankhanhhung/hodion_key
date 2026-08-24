#!/usr/bin/env python3
"""Dựng bảng từ tiếng Anh cho HodionKey (wordlist/data/english_telex.inc).

Chỉ chạy khi muốn dựng lại bảng; kết quả được commit vào repo nên build
bình thường không cần script này, không cần mạng, không cần từ điển hệ thống.

Đầu vào là một danh sách từ tiếng Anh, ví dụ gói Debian/Ubuntu `wamerican`
(SCOWL của Kevin Atkinson, giấy phép cho phép phát hành lại — xem
wordlist/data/SCOWL-COPYRIGHT.txt):

    apt-get install wamerican
    cmake --build build --target hodion_wordscan
    python3 tools/build_wordlist.py \
        --words /usr/share/dict/american-english \
        --scan  build/engine/hodion_wordscan \
        --out   wordlist/data/english_telex.inc

Hai luật lọc, và luật thứ hai mới là điều quan trọng:

1. Bỏ từ nào gõ Telex ra đúng chính nó — không có gì để khôi phục
   ("deadline", "email", "project" chiếm phần lớn danh sách gốc).

2. Bỏ từ nào ghép ra một âm tiết tiếng Việt HỢP LỆ, dù chỉ ở một cấu hình.
   Đây là tập nguy hiểm: "bans"→bán, "cans"→cán, "bust"→bút, "bits"→bít,
   "test"→tét. Ở những từ đó không có bằng chứng nào phân xử được người
   dùng muốn gì, nên bộ gõ không được tự quyết — giữ nguyên chữ tiếng Việt
   và để người dùng bấm Ctrl+Backspace nếu họ muốn từ tiếng Anh.

Nhờ luật 2, bảng này KHÔNG BAO GIỜ ghi đè lên một âm tiết tiếng Việt hợp
lệ. wordlist/tests kiểm lại tính chất đó trên từng từ mỗi lần chạy test.
"""
import argparse
import re
import subprocess
import sys

# Các biến thể cấu hình Telex làm đổi chữ ghép ra. Kiểu bỏ dấu (cũ/mới) chỉ
# đổi VỊ TRÍ dấu chứ không đổi tính hợp lệ nên không cần quét. VNI không
# biến đổi từ tiếng Anh nào (phím dấu của VNI là chữ số) nên cũng không cần.
SCAN_VARIANTS = [[], ["--no-w"], ["--no-free"], ["--no-w", "--no-free"]]

MIN_LEN = 4      # "as", "is", "or" vừa là từ Anh vừa là chuỗi gõ á/í/ỏ
MAX_LEN = 24     # chặn trên cho bộ đệm cố định phía C++

WORD_RE = re.compile(r"^[a-z]+$")


def load_words(path):
    out = set()
    with open(path, encoding="utf-8", errors="ignore") as f:
        for line in f:
            w = line.strip()
            if WORD_RE.match(w) and MIN_LEN <= len(w) <= MAX_LEN:
                out.add(w)
    return sorted(out)


def scan(scan_bin, words, extra_args):
    """Trả về {từ: (chữ ghép ra, có phải âm tiết tiếng Việt hợp lệ không)}."""
    proc = subprocess.run([scan_bin] + extra_args,
                          input="\n".join(words) + "\n",
                          capture_output=True, text=True, check=True)
    result = {}
    for line in proc.stdout.splitlines():
        parts = line.split("\t")
        if len(parts) != 4:
            continue
        result[parts[0]] = (parts[1], parts[2] == "vn")
    return result


def emit(out_path, words, stats):
    by_len = {}
    for w in words:
        by_len.setdefault(len(w), []).append(w)
    lengths = sorted(by_len)
    lo, hi = lengths[0], lengths[-1]

    lines = []
    add = lines.append
    add("// Bảng từ tiếng Anh — SINH TỰ ĐỘNG, đừng sửa tay.")
    add("// Dựng lại bằng: python3 tools/build_wordlist.py (xem chú thích ở đó).")
    add("//")
    add("// Nguồn: SCOWL (Spell Checker Oriented Word Lists), Copyright")
    add("// 2000-2011 by Kevin Atkinson — giấy phép đầy đủ ở")
    add("// wordlist/data/SCOWL-COPYRIGHT.txt, bắt buộc đi kèm bản phát hành.")
    add("//")
    add("// Mỗi khối là các bản ghi độ dài CỐ ĐỊNH, xếp tăng dần, không có ký")
    add("// tự ngăn cách — tra bằng tìm nhị phân, không cần bảng chỉ mục, không")
    add("// cấp phát, không khởi tạo lúc nạp DLL.")
    add("//")
    add("//   %d từ, %d byte dữ liệu" % (stats["kept"], sum(map(len, words))))
    add("//   loại %d từ vì gõ ra đúng chính nó," % stats["unchanged"])
    add("//   loại %d từ vì ghép ra âm tiết tiếng Việt hợp lệ" % stats["collide"])
    add("")
    add("constexpr size_t kMinLen = %d;" % lo)
    add("constexpr size_t kMaxLen = %d;" % hi)
    add("")

    for n in range(lo, hi + 1):
        group = by_len.get(n, [])
        if not group:
            continue
        add("static const char kWords%d[] =" % n)
        # 6 bản ghi mỗi dòng cho dễ đọc diff; trình biên dịch tự nối lại.
        for i in range(0, len(group), 6):
            chunk = " ".join('"%s"' % w for w in group[i:i + 6])
            add("    " + chunk + (";" if i + 6 >= len(group) else ""))
        add("")

    add("struct Block {")
    add("  const char* data;")
    add("  unsigned count;")
    add("};")
    add("")
    add("static const Block kBlocks[] = {")
    for n in range(lo, hi + 1):
        group = by_len.get(n, [])
        if group:
            add("    {kWords%d, %d},  // %d ký tự" % (n, len(group), n))
        else:
            add("    {nullptr, 0},  // %d ký tự" % n)
    add("};")
    add("")

    with open(out_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--words", required=True)
    ap.add_argument("--scan", required=True)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    words = load_words(args.words)
    print("từ vào (%d–%d ký tự, chỉ a–z): %d" % (MIN_LEN, MAX_LEN, len(words)))

    changed, collide = set(), set()
    for variant in SCAN_VARIANTS:
        res = scan(args.scan, words, variant)
        if len(res) != len(words):
            sys.exit("hodion_wordscan trả thiếu dòng")
        for w, (composed, is_vn) in res.items():
            if composed != w:
                changed.add(w)
            if is_vn:
                collide.add(w)

    kept = sorted(changed - collide)
    stats = {
        "kept": len(kept),
        "unchanged": len(words) - len(changed),
        "collide": len(changed & collide),
    }
    print("  bị Telex biến đổi: %d" % len(changed))
    print("  loại vì va chạm âm tiết tiếng Việt: %d" % stats["collide"])
    print("  vào bảng: %d (%d byte)" % (len(kept), sum(map(len, kept))))

    emit(args.out, kept, stats)
    print("đã ghi %s" % args.out)


if __name__ == "__main__":
    main()
