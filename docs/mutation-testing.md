# Mutation testing — đo chất lượng bộ test

Độ phủ (coverage) chỉ nói dòng code có được *chạy* hay không, chứ không nói
bộ test có *bắt lỗi* hay không. Mutation testing trả lời câu hỏi đúng hơn:
**cố tình gieo lỗi vào code, bộ test có kêu không?**

```sh
python3 tools/mutation_test.py --asan          # con số chuẩn (~25 phút, 4 luồng)
python3 tools/mutation_test.py                 # nhanh hơn ~3 lần, để lặp nhanh
python3 tools/mutation_test.py --limit 60      # lấy mẫu cho nhanh
python3 tools/mutation_test.py --files word.cpp
```

Công cụ gieo lỗi vào `engine/src/*.cpp`: đảo toán tử so sánh (`==`↔`!=`,
`<`↔`<=`, `>`↔`>=`), đảo `&&`↔`||`, lật giá trị trả về, lệch hằng số
(`+1`↔`-1`). Mỗi mutant được dịch lại và chạy **toàn bộ** bộ test; mutant
bị test bắt gọi là "chết", mutant **sống sót** là một thay đổi hành vi mà
không bài test nào nhận ra — tức là một lỗ hổng.

Công cụ chỉ làm việc trong thư mục tạm, không đụng vào cây nguồn.

## Kết quả

563 mutant sinh ra, 88 cái không dịch được (không tính điểm), còn 475 cái
hợp lệ:

| Lần chạy                              | Điểm mutation | Sống sót |
|---------------------------------------|---------------|----------|
| Bộ test ban đầu                       | 73,0%         | 130      |
| + test bịt lỗ hổng mutation chỉ ra    | 83,4%         | 79       |
| + test bịt nhánh coverage chỉ ra      | 84,8%         | 72       |
| + AddressSanitizer & UBSan (`--asan`) | **86,9%**     | **62**   |

Ba dòng đầu chạy không sanitizer; dòng cuối là **cùng bộ test đó** chạy lại
kèm sanitizer, nên chênh lệch 84,8% → 86,9% là phần đóng góp của riêng
sanitizer: đúng 10 mutant, và không mutant nào đang chết lại sống dậy.

## Những lỗ hổng thật đã bịt

Mutation testing chỉ ra các chỗ code có nhánh riêng mà không test nào phân
biệt được — tất cả đều đã có test trong `engine/tests/test_gaps.cpp`:

- **Âm cuối bị bỏ qua khi kiểm tra dấu**: `hoachw` phải giữ nguyên vì vần
  "oă" không đi với âm cuối "ch"; nếu bỏ qua âm cuối thì ra "hoăch".
- **Phụ âm đầu bị bỏ qua khi ghép thêm nguyên âm**: `kius` phải hỏng vì `k`
  không đi với vần "iu".
- **"ch" cũng là âm cuối tắc**: `bachf`, `bachx`, `bachr` không được nhận
  huyền/hỏi/ngã, chỉ `bachj` (nặng) mới được.
- **Chuyển qua lại trong họ vần uo**: `uowo`→uô, `uoow`→ươ, và ngoại lệ
  sau "th" (`thuoow`→thuơ).
- **`z` xóa dấu thanh đang có trên `gi`**: `gifz`→gi.
- **Cứu dấu thanh khi hủy nguyên âm bằng phím ngoặc**: `]s[[` phải cho
  "ứ[" — dấu sắc lùi về ư chứ không biến mất.
- **Ngưỡng độ dài từ** đúng bằng 40 ký tự, không phải 39 hay 41.
- **Khôi phục tự động** chỉ chạy khi thật sự có phím bị biến đổi.
- **Bảng ký tự** với các tổ hợp engine không sinh ra (i + mũ, d + thanh,
  ký tự không phải chữ cái) — đường thoát an toàn của bảng.
- **C API**: đổi kiểu bỏ dấu, bật/tắt cờ theo cả hai chiều, bộ đệm ra cỡ
  0 / chật / vừa khít.
- **Biên UTF**: đúng các mốc đổi số byte (0x7F/0x80, 0x7FF/0x800,
  0xFFFF/0x10000) và cặp thay thế UTF-16.

Nhân tiện cũng lộ ra một điểm yếu của chính engine: bộ giải mã UTF-8 gặp
chuỗi hỏng thì nuốt luôn ký tự hợp lệ đứng ngay sau. Nay nó chỉ bỏ đúng
phần hỏng rồi đọc tiếp.

### Đợt gõ trộn Việt–Anh và reconversion

Ba lỗ hổng nữa, cùng một kiểu — nhánh có thật mà không test nào phân biệt:

- **Hai đầu bảng chữ cái.** Bộ lọc "chuỗi phím có phải một từ chữ cái
  không" và bộ lọc chữ cái của reconversion đều kiểm `>= 'a' && <= 'z'`;
  không test nào dùng từ chứa `a` hay `z` ở đúng chỗ biên, nên đổi `>=`
  thành `>` không ai thấy. Nay có `cart`, `bozo`, `An`, `AN`.
- **Chặn độ dài của chế độ gõ thẳng** lệch một ký tự so với đường ghép
  bình thường mà không test nào bắt. Nay test so trực tiếp hai chặn với
  nhau thay vì chỉ kiểm "có tự chốt không".
- **Một đoạn code chết.** `syllable_variants` đẩy riêng chữ hiện tại lên
  đầu bằng `std::rotate` — nhưng nó đã luôn ở đầu sau khi sắp xếp theo
  khoảng cách (chỉ nó mới có khoảng cách 0). Mutant đổi `self + 1` thành
  `self - 1` vẫn sống, và đó là dấu hiệu đoạn đó không làm gì. Đã bỏ.

Viết test cho `reconvert.cpp` còn lộ ra một lỗi thật của tính năng: `z` là
phím xoá thanh của Telex nên engine không coi "zan" là từ hỏng, và danh
sách phương án vì thế đề xuất cả "zán", "zàn" — trong khi tiếng Việt không
có chữ z. Nay bộ lọc chữ cái của reconversion loại thẳng f, j, w, z.

## Vì sao không đuổi tới 100%

Số mutant còn sống chủ yếu thuộc ba nhóm **không thể giết bằng test**:

1. **Mutant tương đương** — đổi mã nhưng không đổi hành vi. Ví dụ
   `u8.size() < cap - 1` thành `<=`: khi hai vế bằng nhau thì cả hai nhánh
   cho cùng giá trị.
2. **Mã phòng thủ không tới được** — các chốt chặn cho trường hợp bảng dữ
   liệu bị sai, mà bảng thì luôn đúng (vd `if (n < 3)` khi mọi chuỗi trong
   bảng đều ≤ 3 ký tự). Giữ lại vì rẻ và an toàn.
3. **Mutant chỉ gây lỗi bộ nhớ** — đổi biên vòng lặp hay chỉ số thành đọc/ghi
   lố mảng. Chương trình vẫn cho ra chuỗi đúng nên so sánh kết quả không
   thấy gì. Nhóm này đã xử lý được bằng `--asan` (xem dưới).

`reconvert.cpp` là ví dụ rõ nhất của nhóm 1 và 2: điểm của riêng file này
chỉ **66,7%**, nhưng cả 8 mutant còn sống đều chứng minh được là tương
đương:

- `word.size() >= 8` thay cho `> 8`: âm tiết dài nhất của tiếng Việt là
  "nghiêng" (7 ký tự), nên không đầu vào nào phân biệt được hai vế.
- Ba mutant ở chốt chặn đầu hàm (`||` → `&&`, `max_results == 0`): bỏ chốt
  đi thì các bộ lọc phía sau vẫn cho ra đúng kết quả rỗng. Chốt chỉ để
  thoát sớm cho đầu vào bất thường, không gánh phần đúng/sai.
- `mask <= (1 << 4)` và `i <= kMarkCount`: vòng thừa có `mask` bằng 16, mà
  16 không có bit nào trong 4 bit đầu, nên nó chạy y hệt `mask = 0` và bị
  khử trùng; `kMarkKeys[4]` vì thế cũng không bao giờ bị đọc tới.
- `i <= other.size()` trong hàm đo khoảng cách: `std::u32string::operator[]`
  ở đúng vị trí `size()` là hợp lệ và trả về ký tự 0 — không phải đọc lố,
  và ký tự 0 không đóng góp gì vào khoảng cách.
- `out.size() >= max_results`: cắt khi vừa đúng bằng giới hạn là không cắt
  gì cả.

Đã thử chạy một trong số đó qua 9,4 triệu lượt fuzz để chắc chắn nó thật sự
không đổi hành vi, chứ không phải bộ test yếu.

## Chạy kèm AddressSanitizer

`--asan` dịch cả engine lẫn test với `-fsanitize=address,undefined
-fno-sanitize-recover=all`, nên mutant nào gây đọc/ghi lố mảng hoặc hành vi
không xác định sẽ làm chương trình dừng với mã lỗi — tức là **bị bắt**, dù
chuỗi ra vẫn đúng.

Đúng 10 mutant chỉ sanitizer mới giết được, tất cả đều là lỗi bộ nhớ:

- `vnlexi.cpp:188/203/239/247`, `word.cpp:421/526` — biên vòng lặp
  `i < n` thành `i <= n`: đọc quá phần tử cuối của bảng vần / bảng phụ âm.
- `word.cpp:775` — lệch chỉ số `-1` thành `+1` khi tính đầu vần.
- `word.cpp:281/340/776` — đảo `&&`/`||` làm hỏng chốt chặn chỉ số, dẫn tới
  truy cập ngoài mảng.

Đây chính là loại lỗi nguy hiểm nhất trong C++ mà so sánh chuỗi không bao
giờ thấy: chương trình đọc rác nhưng vẫn tình cờ in ra đúng. Vì vậy CI chạy
toàn bộ test **và** một lượt fuzz 200.000 vòng dưới ASan + UBSan (kể cả dò
rò bộ nhớ) trong job `sanitizers`; engine hiện sạch với hơn 11 triệu lượt
kiểm tra dưới sanitizer.

Mã chỉ phục vụ test (`Word::check_invariants`, `Engine::self_check`) được
đánh dấu `MUTATION-SKIP-BEGIN/END` và loại khỏi phạm vi: gieo lỗi vào đó chỉ
làm phép kiểm tra yếu đi, mà bộ test thì không thể tự phát hiện điều đó.

## Đối chiếu với độ phủ (coverage)

Đo bằng gcovr trên chính bộ test đó:

```sh
cmake -S . -B build-cov -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="--coverage -O0" -DCMAKE_EXE_LINKER_FLAGS="--coverage"
cmake --build build-cov -j
./build-cov/engine/hodion_engine_tests && ./build-cov/wordlist/hodion_wordlist_tests
gcovr --root . --filter 'engine/src/' --filter 'engine/include/' \
      --filter 'wordlist/src/' --txt --branches
```

| Thước đo            | Ban đầu | Sau khi bịt lỗ hổng | Nay (kèm gõ trộn + reconversion) |
|---------------------|---------|---------------------|----------------------------------|
| Dòng (line)         | 97%     | 99%                 | 98%                              |
| Nhánh (branch)      | 80%     | 81%                 | 81%                              |
| Mutation            | 73,0%   | 83,4%               | xem bảng trên                    |

Dòng tụt từ 99% xuống 98% không phải vì mất test: hai "dòng chưa chạy" mới
đều là dấu `}` đóng hàm trong `reconvert.cpp` — gcov tính riêng khối dọn dẹp
ngoại lệ mà chương trình không bao giờ đi vào (`strip_diacritics` chạy
56.174 lần, dòng thân hàm phủ 100%, chỉ dấu `}` là `=====`). File nhỏ nên
hai dòng đó đủ kéo tổng xuống 1%.

Ba con số này nói ba chuyện khác nhau, và đó chính là lý do không nên nhìn
mỗi coverage: **bộ test cũ đã phủ 97% số dòng nhưng mutation vẫn tìm ra 130
lỗ hổng** — dòng code có chạy không có nghĩa là test sẽ kêu khi nó sai.

Coverage vẫn có ích ở chỗ khác: nó chỉ thẳng ra những đoạn *chưa từng chạy*,
rẻ hơn nhiều so với chạy mutation. Nhờ nó mà phát hiện `hodion_engine_backspace`
— một hàm trong C ABI mà bản port macOS/Linux sẽ dùng — chưa có test nào gọi
tới, cùng vài nhánh khác (phím `7` của VNI gặp nguyên âm mang dấu trăng, cứu
dấu thanh trong âm tiết có phụ âm đầu, gõ lặp dấu trên `gi`).

Sáu dòng còn lại chưa phủ đều là **chốt chặn phòng thủ không tới được**
(nhánh xóa ô không phải nguyên âm, `default:` của switch đã liệt kê đủ, lệnh
`return` sau switch vét cạn). Giữ lại vì rẻ và an toàn, không cố nặn test
giả để làm đẹp con số.

Nhánh (branch) thấp hơn dòng nhiều là chuyện bình thường: mỗi điều kiện được
tính cả hai chiều, kể cả các chốt chặn phòng thủ vốn chỉ đi một chiều.

CI có đặt ngưỡng sàn (dòng 97%, nhánh 78%) để độ phủ không tụt dần theo thời
gian.
