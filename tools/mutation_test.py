#!/usr/bin/env python3
"""Mutation testing cho lõi bộ gõ.

Gieo lỗi nhỏ vào mã nguồn engine (đảo toán tử so sánh, đảo && / ||, lật giá
trị trả về, lệch hằng số…), build lại rồi chạy bộ test. Mutant bị test bắt
gọi là "chết"; mutant **sống sót** nghĩa là có một thay đổi hành vi mà không
bài test nào nhận ra — đó chính là lỗ hổng của bộ test.

    python3 tools/mutation_test.py                # chạy toàn bộ
    python3 tools/mutation_test.py --limit 60     # lấy mẫu cho nhanh
    python3 tools/mutation_test.py --files word.cpp

Chỉ đọc/ghi trong thư mục tạm, không đụng vào cây nguồn.
"""

import argparse
import concurrent.futures
import os
import random
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ENGINE_SRC = os.path.join(ROOT, "engine", "src")
ENGINE_TESTS = os.path.join(ROOT, "engine", "tests")
INCLUDES = ["-I", os.path.join(ROOT, "engine", "include"), "-I", ENGINE_SRC]
BASE_CXXFLAGS = ["-std=c++17", "-O0", "-w"]

# Bật sanitizer để bắt cả những mutant chỉ gây lỗi bộ nhớ (đọc lố mảng,
# tràn số…) — loại mà so sánh chuỗi ra không thấy được.
SANITIZE_FLAGS = ["-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                  "-fno-omit-frame-pointer", "-g", "-O1"]

# Số vòng fuzz khi chạy mutation: đủ để fuzz phát huy tác dụng mà vẫn nhanh
# (mỗi mutant phải chạy lại toàn bộ bộ test).
FUZZ_ROUNDS = "2000"


CXXFLAGS = list(BASE_CXXFLAGS)
LDFLAGS = []


class Mutation:
    def __init__(self, path, line_no, old_line, new_line, kind):
        self.path = path
        self.line_no = line_no
        self.old_line = old_line
        self.new_line = new_line
        self.kind = kind

    def label(self):
        return "%s:%d [%s]" % (os.path.basename(self.path), self.line_no,
                               self.kind)

    def diff(self):
        return "    - %s\n    + %s" % (self.old_line.strip(),
                                       self.new_line.strip())


# (biểu thức tìm, thay bằng, tên phép gieo lỗi)
TOKEN_MUTATIONS = [
    (r"(?<![<>=!+\-*/&|])==(?!=)", "!=", "== → !="),
    (r"(?<![<>=!+\-*/&|])!=(?!=)", "==", "!= → =="),
    (r"(?<![<>=!\-])<=(?!=)", "<", "<= → <"),
    (r"(?<![<>=!\-])>=(?!=)", ">", ">= → >"),
    (r"(?<![<>=!\-<])<(?![=<])", "<=", "< → <="),
    (r"(?<![<>=!\->])>(?![=>])", ">=", "> → >="),
    (r"&&", "||", "&& → ||"),
    (r"\|\|", "&&", "|| → &&"),
    (r"\+ 1\b", "- 1", "+1 → -1"),
    (r"- 1\b", "+ 1", "-1 → +1"),
    (r"\breturn true;", "return false;", "return true → false"),
    (r"\breturn false;", "return true;", "return false → true"),
    (r"\breturn 0;", "return 1;", "return 0 → 1"),
]

SKIP_LINE_MARKERS = ("#include", "static_assert")


def is_code_line(line):
    stripped = line.strip()
    if not stripped or stripped.startswith("//") or stripped.startswith("*"):
        return False
    if stripped.startswith("/*"):
        return False
    for marker in SKIP_LINE_MARKERS:
        if stripped.startswith(marker):
            return False
    return True


def strip_comment(line):
    """Bỏ phần chú thích để không gieo lỗi vào chữ tiếng Việt trong comment."""
    idx = line.find("//")
    return line if idx < 0 else line[:idx]


def generate_mutations(paths):
    """Bỏ qua các vùng đánh dấu MUTATION-SKIP (mã chỉ phục vụ test: gieo lỗi
    vào đó chỉ làm phép kiểm tra yếu đi chứ không đổi hành vi bộ gõ)."""
    mutations = []
    for path in paths:
        with open(path, encoding="utf-8") as handle:
            lines = handle.read().split("\n")
        skipping = False
        for i, line in enumerate(lines):
            if "MUTATION-SKIP-BEGIN" in line:
                skipping = True
                continue
            if "MUTATION-SKIP-END" in line:
                skipping = False
                continue
            if skipping or not is_code_line(line):
                continue
            code = strip_comment(line)
            for pattern, replacement, kind in TOKEN_MUTATIONS:
                for match in re.finditer(pattern, code):
                    new_code = (code[:match.start()] + replacement +
                                code[match.end():])
                    new_line = new_code + line[len(code):]
                    mutations.append(
                        Mutation(path, i + 1, line, new_line, kind))
    return mutations


def build_baseline(workdir):
    """Dịch sẵn các object không đổi (test và engine gốc) để mutant chỉ phải
    dịch lại đúng một file."""
    obj_dir = os.path.join(workdir, "base")
    os.makedirs(obj_dir, exist_ok=True)

    sources = ([os.path.join(ENGINE_SRC, f) for f in sorted(os.listdir(ENGINE_SRC))
                if f.endswith(".cpp")] +
               [os.path.join(ENGINE_TESTS, f) for f in sorted(os.listdir(ENGINE_TESTS))
                if f.endswith(".cpp")])

    objects = {}
    for src in sources:
        obj = os.path.join(obj_dir, os.path.basename(src) + ".o")
        cmd = ["g++"] + CXXFLAGS + INCLUDES + ["-c", src, "-o", obj]
        subprocess.run(cmd, check=True)
        objects[src] = obj
    return objects


def run_tests(binary, timeout):
    env = dict(os.environ, HODION_FUZZ_ROUNDS=FUZZ_ROUNDS,
               ASAN_OPTIONS="detect_leaks=1:abort_on_error=0",
               UBSAN_OPTIONS="print_stacktrace=0")
    try:
        proc = subprocess.run([binary], capture_output=True, timeout=timeout,
                              env=env)
        return proc.returncode
    except subprocess.TimeoutExpired:
        return "timeout"


def evaluate(mutation, objects, workdir, index, timeout):
    """Trả về 'killed' | 'survived' | 'build_error' | 'timeout'."""
    scratch = os.path.join(workdir, "m%05d" % index)
    os.makedirs(scratch, exist_ok=True)
    try:
        with open(mutation.path, encoding="utf-8") as handle:
            lines = handle.read().split("\n")
        lines[mutation.line_no - 1] = mutation.new_line
        mutated_src = os.path.join(scratch, os.path.basename(mutation.path))
        with open(mutated_src, "w", encoding="utf-8") as handle:
            handle.write("\n".join(lines))

        mutated_obj = os.path.join(scratch, "mutant.o")
        compile_cmd = (["g++"] + CXXFLAGS + INCLUDES +
                       ["-c", mutated_src, "-o", mutated_obj])
        proc = subprocess.run(compile_cmd, capture_output=True, timeout=timeout)
        if proc.returncode != 0:
            return "build_error"

        link_objs = [mutated_obj] + [obj for src, obj in objects.items()
                                     if src != mutation.path]
        binary = os.path.join(scratch, "mutant_tests")
        proc = subprocess.run(["g++"] + link_objs + LDFLAGS + ["-o", binary],
                              capture_output=True, timeout=timeout)
        if proc.returncode != 0:
            return "build_error"

        result = run_tests(binary, timeout)
        if result == "timeout":
            return "timeout"       # treo cũng là bị bắt
        return "killed" if result != 0 else "survived"
    finally:
        shutil.rmtree(scratch, ignore_errors=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--limit", type=int, default=0,
                        help="chỉ chạy N mutant lấy mẫu ngẫu nhiên (0 = tất cả)")
    parser.add_argument("--files", nargs="*", default=None,
                        help="chỉ gieo lỗi vào các file này (tên cơ sở)")
    parser.add_argument("--jobs", type=int, default=os.cpu_count() or 2)
    parser.add_argument("--timeout", type=int, default=120)
    parser.add_argument("--seed", type=int, default=20260823)
    parser.add_argument("--asan", action="store_true",
                        help="dịch kèm AddressSanitizer + UBSan để bắt cả "
                             "mutant chỉ gây lỗi bộ nhớ (chậm hơn ~3 lần)")
    args = parser.parse_args()

    global CXXFLAGS, LDFLAGS
    if args.asan:
        CXXFLAGS = list(BASE_CXXFLAGS) + SANITIZE_FLAGS
        LDFLAGS = ["-fsanitize=address,undefined"]
        print("Bật AddressSanitizer + UBSan (chậm hơn nhiều).")

    paths = [os.path.join(ENGINE_SRC, f) for f in sorted(os.listdir(ENGINE_SRC))
             if f.endswith(".cpp")]
    if args.files:
        wanted = set(args.files)
        paths = [p for p in paths if os.path.basename(p) in wanted]

    mutations = generate_mutations(paths)
    random.Random(args.seed).shuffle(mutations)
    if args.limit:
        mutations = mutations[:args.limit]

    print("Sinh %d mutant từ %d file, chạy %d luồng." %
          (len(mutations), len(paths), args.jobs))

    workdir = tempfile.mkdtemp(prefix="hodion-mutation-")
    try:
        print("Dịch sẵn object nền…")
        objects = build_baseline(workdir)

        baseline_bin = os.path.join(workdir, "baseline_tests")
        subprocess.run(["g++"] + list(objects.values()) + LDFLAGS +
                       ["-o", baseline_bin], check=True)
        if run_tests(baseline_bin, args.timeout) != 0:
            print("LỖI: bộ test đã đỏ khi chưa gieo lỗi — sửa test trước đã.")
            return 2

        results = {}
        survivors = []
        done = 0
        with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
            futures = {
                pool.submit(evaluate, m, objects, workdir, i, args.timeout): m
                for i, m in enumerate(mutations)
            }
            for future in concurrent.futures.as_completed(futures):
                mutation = futures[future]
                outcome = future.result()
                results[outcome] = results.get(outcome, 0) + 1
                if outcome == "survived":
                    survivors.append(mutation)
                done += 1
                if done % 25 == 0 or done == len(mutations):
                    print("  %d/%d…" % (done, len(mutations)), flush=True)

        valid = sum(v for k, v in results.items() if k != "build_error")
        killed = results.get("killed", 0) + results.get("timeout", 0)
        print("\n=== Kết quả ===")
        print("Tổng mutant     : %d" % len(mutations))
        print("Không dịch được : %d (không tính điểm)" %
              results.get("build_error", 0))
        print("Bị test bắt     : %d" % killed)
        print("Sống sót        : %d" % len(survivors))
        if valid:
            print("Điểm mutation   : %.1f%%" % (100.0 * killed / valid))

        if survivors:
            print("\n=== Mutant sống sót (lỗ hổng của bộ test) ===")
            for mutation in sorted(survivors,
                                   key=lambda m: (m.path, m.line_no)):
                print("%s\n%s" % (mutation.label(), mutation.diff()))
        return 0
    finally:
        shutil.rmtree(workdir, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
