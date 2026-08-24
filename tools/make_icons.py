#!/usr/bin/env python3
"""Sinh icon cho HodionKey.

    python3 tools/make_icons.py

Kết quả (.ico) được commit nên build bình thường không cần ImageMagick;
chỉ chạy lại khi muốn đổi hình.

Hai điểm đáng lưu ý:

1. Hai trạng thái dùng CHỮ khác nhau (V / E) chứ không chỉ khác màu. Ở
   16×16, và với người mù màu, khác màu thôi là không phân biệt được.

2. Mỗi cỡ được VẼ RIÊNG ở đúng kích thước của nó, và file .ico được ghép
   tay. `convert ... out.ico` nghe thì tiện nhưng nó thu nhỏ tất cả từ
   frame lớn nhất, làm nét chéo của chữ V nhòe hẳn ở 16×16 — đúng cỡ mà
   khay hệ thống hay dùng nhất. Ghép tay để giữ nguyên bản vẽ từng cỡ.
"""
import os
import struct
import subprocess
import sys
import tempfile

FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
SIZES = [16, 20, 24, 32, 48, 64, 128, 256]
OUT_DIR = "platform/windows/config/icons"

# Đỏ = đang gõ tiếng Việt. Xám = đang tắt (gõ thẳng).
STATES = [("V", "#c62828", "hodionkey-vi.ico"),
          ("E", "#5a6472", "hodionkey-en.ico")]


def render(size, letter, bg, path):
    """Vẽ một cỡ ở đúng kích thước của nó."""
    if size <= 24:
        # Cỡ nhỏ: tràn viền, bo nhẹ, chữ chiếm gần hết ô.
        pad, radius, pt, dy = 0, max(2, size // 5), int(size * 0.92), 0
    else:
        pad, radius = size // 32, int(size * 0.22)
        pt, dy = int(size * 0.72), max(1, size // 40)
    subprocess.run([
        "convert", "-size", "%dx%d" % (size, size), "xc:none",
        "-fill", bg,
        "-draw", "roundrectangle %d,%d %d,%d %d,%d" %
                 (pad, pad, size - 1 - pad, size - 1 - pad, radius, radius),
        "-font", FONT, "-pointsize", str(pt), "-fill", "white",
        "-gravity", "center", "-annotate", "+0+%d" % dy, letter,
        "-depth", "8", "PNG32:" + path,
    ], check=True)


def raw_rgba(path, size):
    out = subprocess.run(["convert", path, "-depth", "8", "RGBA:-"],
                         check=True, capture_output=True).stdout
    if len(out) != size * size * 4:
        sys.exit("kích thước pixel bất ngờ cho %s" % path)
    return out


def bmp_frame(rgba, size):
    """Một frame kiểu DIB 32-bit: BITMAPINFOHEADER + BGRA lộn ngược + mặt nạ AND."""
    header = struct.pack("<IiiHHIIiiII", 40, size, size * 2, 1, 32, 0, 0,
                         0, 0, 0, 0)
    rows = []
    for y in range(size - 1, -1, -1):  # DIB xếp từ dưới lên
        row = bytearray()
        for x in range(size):
            r, g, b, a = rgba[(y * size + x) * 4:(y * size + x) * 4 + 4]
            row += bytes((b, g, r, a))
        rows.append(bytes(row))
    # Mặt nạ AND không dùng tới khi đã có kênh alpha, nhưng vẫn phải có mặt.
    mask_stride = ((size + 31) // 32) * 4
    mask = b"\x00" * (mask_stride * size)
    return header + b"".join(rows) + mask


def write_ico(frames, path):
    """frames: danh sách (size, dữ liệu, là_png)."""
    out = [struct.pack("<HHH", 0, 1, len(frames))]
    offset = 6 + 16 * len(frames)
    entries, blobs = [], []
    for size, data, _is_png in frames:
        dim = 0 if size >= 256 else size
        entries.append(struct.pack("<BBBBHHII", dim, dim, 0, 0, 1, 32,
                                   len(data), offset))
        blobs.append(data)
        offset += len(data)
    with open(path, "wb") as f:
        f.write(out[0])
        for e in entries:
            f.write(e)
        for b in blobs:
            f.write(b)


def main():
    if not os.path.exists(FONT):
        sys.exit("thiếu font: %s" % FONT)
    os.makedirs(OUT_DIR, exist_ok=True)

    with tempfile.TemporaryDirectory() as tmp:
        for letter, bg, name in STATES:
            frames = []
            for size in SIZES:
                png = os.path.join(tmp, "%s-%d.png" % (letter, size))
                render(size, letter, bg, png)
                if size >= 128:
                    # Cỡ lớn nén PNG cho nhẹ file; Windows Vista trở lên đọc được.
                    with open(png, "rb") as f:
                        frames.append((size, f.read(), True))
                else:
                    frames.append((size, bmp_frame(raw_rgba(png, size), size),
                                   False))
            write_ico(frames, os.path.join(OUT_DIR, name))
            print("%s: %d frame" % (name, len(frames)))

    # Icon của ứng dụng dùng lại trạng thái tiếng Việt.
    with open(os.path.join(OUT_DIR, "hodionkey-vi.ico"), "rb") as src, \
            open(os.path.join(OUT_DIR, "hodionkey.ico"), "wb") as dst:
        dst.write(src.read())


if __name__ == "__main__":
    main()
