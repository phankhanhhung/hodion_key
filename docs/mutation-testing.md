# Mutation testing — đo chất lượng bộ test

Độ phủ (coverage) chỉ nói dòng code có được *chạy* hay không, chứ không nói
bộ test có *bắt lỗi* hay không. Mutation testing trả lời câu hỏi đúng hơn:
**cố tình gieo lỗi vào code, bộ test có kêu không?**

```sh
python3 tools/mutation_test.py                 # chạy toàn bộ (~10 phút, 4 luồng)
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

| Lần chạy                | Điểm mutation | Sống sót |
|-------------------------|---------------|----------|
| Trước khi bịt lỗ hổng   | 73,0%         | 130      |
| Sau khi thêm test       | 83,4%         | 79       |

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

## Vì sao không đuổi tới 100%

Số mutant còn sống chủ yếu thuộc ba nhóm **không thể giết bằng test**:

1. **Mutant tương đương** — đổi mã nhưng không đổi hành vi. Ví dụ
   `u8.size() < cap - 1` thành `<=`: khi hai vế bằng nhau thì cả hai nhánh
   cho cùng giá trị.
2. **Mã phòng thủ không tới được** — các chốt chặn cho trường hợp bảng dữ
   liệu bị sai, mà bảng thì luôn đúng (vd `if (n < 3)` khi mọi chuỗi trong
   bảng đều ≤ 3 ký tự). Giữ lại vì rẻ và an toàn.
3. **Mutant chỉ gây UB** — đổi biên vòng lặp thành đọc lố mảng một phần tử
   (15 trong số 79 cái còn sống là dạng này). Chương trình thường vẫn chạy
   đúng nên test không thấy; muốn bắt thì phải chạy mutation kèm
   AddressSanitizer — việc còn để lại cho sau.

Mã chỉ phục vụ test (`Word::check_invariants`, `Engine::self_check`) được
đánh dấu `MUTATION-SKIP-BEGIN/END` và loại khỏi phạm vi: gieo lỗi vào đó chỉ
làm phép kiểm tra yếu đi, mà bộ test thì không thể tự phát hiện điều đó.
