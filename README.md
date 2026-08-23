# HodionKey

Bộ gõ tiếng Việt cho Windows viết from scratch theo chuẩn **TSF (Text
Services Framework)** — không hook bàn phím toàn cục, không giả lập phím —
với **engine gõ tách rời hoàn toàn** để port sang macOS/Linux.

- **Telex** (đầy đủ, kể cả `w→ư`, `[ ] { }`) và **VNI**, bộ luật **tương
  thích hành vi UniKey** — nghiên cứu từ ukengine và cài đặt lại từ đầu,
  đặc tả tại [docs/unikey-rules.md](docs/unikey-rules.md).
- Kiểm tra chính tả cấu trúc âm tiết (từ sai ngừng biến đổi — `vietr` giữ
  nguyên), gõ dấu tự do ở cuối từ (`viete→viêt`, `dund→đun`), luật hủy khi
  gõ lặp (`aa→â→aa`, `as→á→as`, `uoww→uow`…), họ vần uo đầy đủ
  (`thuow→thuơ` cho "thuở", `thuowng→thương`, `tuoiw→tươi`).
- Đặt dấu thanh đúng chính tả (`qu`/`gi`, `ươ`, `uyê`…), kiểu cũ (`hòa`)
  lẫn kiểu mới (`hoà`); tùy chọn tự khôi phục từ không phải tiếng Việt.
- Ký tự Unicode dựng sẵn (NFC) — hiển thị đúng ở mọi ứng dụng.
- Composition chuẩn TSF: đoạn đang gõ gạch chân, Backspace hoàn tác từng
  phím, Esc trả lại chuỗi phím thô, Enter/Tab/chuột chốt từ tự nhiên.

## Cấu trúc

```
engine/            Lõi bộ gõ — C++17 thuần, không phụ thuộc OS + C ABI (FFI)
platform/windows/  TSF text service (COM DLL, không ATL/WRL)
docs/              Kiến trúc & quyết định thiết kế
cmake/             Toolchain cross-compile MinGW (CI trên Linux)
```

Chi tiết thiết kế: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Build

### Engine + test (mọi hệ điều hành)

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Thử engine ngay trên terminal:

```sh
echo "xin chaof thees giowis" | ./build/engine/hodion_demo
# → xin chào thế giới
echo "vie6t5 nam" | ./build/engine/hodion_demo --vni
# → việt nam
```

### DLL Windows (Visual Studio 2019+)

```bat
cmake -S . -B build -A x64
cmake --build build --config Release
```

Kết quả: `build\platform\windows\Release\HodionKey.dll`.

(Cũng cross-compile được từ Linux bằng MinGW:
`cmake -S . -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake`.)

## Cài đặt trên Windows

1. Mở Command Prompt **quyền Administrator**.
2. Đăng ký text service:

   ```bat
   regsvr32 HodionKey.dll
   ```

3. Vào **Settings → Time & language → Language & region**, thêm ngôn ngữ
   **Tiếng Việt** nếu chưa có — bàn phím **HodionKey** xuất hiện trong danh
   sách bàn phím của tiếng Việt. Chuyển bàn phím bằng `Win + Space`.

Gỡ: `regsvr32 /u HodionKey.dll`.

## Cấu hình

Tùy chọn đọc từ registry `HKCU\Software\HodionKey` (DWORD), nạp lại mỗi lần
bàn phím được kích hoạt. Mặc định trùng với mặc định của UniKey:

| Giá trị         | Mặc định | Ý nghĩa                                          |
|-----------------|----------|--------------------------------------------------|
| `InputMethod`   | `0`      | `0` = Telex, `1` = VNI                           |
| `ToneStyle`     | `0`      | `0` = kiểu cũ (hòa), `1` = kiểu mới (hoà)        |
| `FreeMarking`   | `1`      | Gõ dấu tự do — dấu ở cuối từ (`viete→viêt`)      |
| `SpellCheck`    | `1`      | Kiểm tra chính tả âm tiết (từ sai ngừng biến đổi) |
| `RestoreNonVn`  | `0`      | Tự trả lại phím thô với từ không phải tiếng Việt |
| `WShorthand`    | `1`      | Telex: `w` không áp được móc thì thành `ư`       |
| `TelexBrackets` | `1`      | Telex đầy đủ: `[ ] { }` → `ơ ư Ơ Ư`              |

## Lộ trình

- [x] Bộ luật Telex/VNI tương thích UniKey (spell-check, gõ dấu tự do,
      khôi phục từ không phải tiếng Việt, `[ ]`, họ vần uo…)
- [ ] Port macOS (IMKit) và Linux (fcitx5) trên cùng engine
- [ ] App cấu hình + phím tắt bật/tắt tiếng Việt
- [ ] Gõ tắt (macro) người dùng định nghĩa, VIQR
