#!/usr/bin/env python3
"""Trích văn bản tiếng Việt từ bãi đổ XML của Wikipedia.

    curl -L https://dumps.wikimedia.org/viwiki/latest/viwiki-latest-pages-articles.xml.bz2 \
      | bzip2 -dc | python3 tools/extract_wikipedia.py 500000000 > vi.txt

Đây là bước ĐẦU TIÊN của chuỗi dựng dữ liệu đoán dấu; hai bước sau là
tools/build_syllables.py và tools/train_ngram.py. Có script này trong repo
thì cả chuỗi mới dựng lại được từ đầu — không có nó, mô hình là một khối
nhị phân không ai kiểm chứng được nó sinh ra từ đâu.

Không cần bộ phân tích wiki markup đầy đủ: hai bước sau chỉ ĐẾM chuỗi âm
tiết, nên bỏ thô mấy thứ gây nhiễu (bảng, tham chiếu, template, liên kết
file) là đủ. Cái cần giữ đúng là **dấu tiếng Việt** và **thứ tự từ**, và
hai thứ đó markup không đụng tới.

Tham số duy nhất là số byte văn bản cần lấy; dừng ngay khi đủ để khỏi
nghiền hết mấy GB. Mặc định 400 MB.

Giấy phép: văn bản Wikipedia là CC BY-SA — xem
wordlist/data/GIAY-PHEP-DU-LIEU.txt.
"""
import re
import sys

LIMIT = int(sys.argv[1]) if len(sys.argv) > 1 else 400 * 1024 * 1024

RE_COMMENT = re.compile(r"<!--.*?-->", re.S)
RE_REF = re.compile(r"<ref[^>]*?/>|<ref.*?</ref>", re.S | re.I)
RE_TAG = re.compile(r"<[^>]+>")
RE_TEMPLATE = re.compile(r"\{\{[^{}]*\}\}")
RE_TABLE = re.compile(r"\{\|.*?\|\}", re.S)
RE_FILE = re.compile(r"\[\[(?:Tập tin|Tệp|File|Image|Hình)\s*:[^\]]*\]\]", re.I)
RE_LINK = re.compile(r"\[\[(?:[^\]|]*\|)?([^\]|]*)\]\]")
RE_EXT = re.compile(r"\[https?://\S+\s([^\]]*)\]")
RE_QUOTE = re.compile(r"'{2,}")
RE_HEAD = re.compile(r"^=+.*?=+$", re.M)
RE_WS = re.compile(r"[ \t]+")

def clean(text):
    text = RE_COMMENT.sub(" ", text)
    text = RE_REF.sub(" ", text)
    text = RE_TABLE.sub(" ", text)
    text = RE_FILE.sub(" ", text)
    for _ in range(4):          # template lồng nhau
        text = RE_TEMPLATE.sub(" ", text)
    text = RE_EXT.sub(r"\1", text)
    text = RE_LINK.sub(r"\1", text)
    text = RE_TAG.sub(" ", text)
    text = RE_QUOTE.sub("", text)
    text = RE_HEAD.sub(" ", text)
    return RE_WS.sub(" ", text)

def main():
    out = sys.stdout
    written = 0
    buf = []
    inside = False
    for line in sys.stdin:
        if not inside:
            i = line.find("<text")
            if i < 0:
                continue
            j = line.find(">", i)
            if j < 0:
                continue
            inside = True
            line = line[j + 1:]
        k = line.find("</text>")
        if k >= 0:
            buf.append(line[:k])
            inside = False
            body = clean("".join(buf))
            buf = []
            for para in body.split("\n"):
                para = para.strip()
                if len(para) > 40:
                    out.write(para + "\n")
                    # Đếm BYTE chứ không đếm ký tự: tiếng Việt có dấu
                    # tốn ~1,3 byte/ký tự, đếm nhầm là ra file to hơn
                    # người ta xin gần 30%.
                    written += len(para.encode("utf-8")) + 1
            if written >= LIMIT:
                break
        else:
            buf.append(line)
    out.flush()

main()
