# HodionKey

Bộ gõ tiếng Việt cho Windows viết from scratch theo chuẩn **TSF (Text
Services Framework)** — không hook bàn phím toàn cục, không giả lập phím —
với **engine gõ tách rời hoàn toàn** để port sang macOS/Linux.

- **Telex** và **VNI**, đủ luật dấu phụ/dấu thanh, luật hủy khi gõ lặp
  (`aa→â→aa`, `as→á→as`, `dd→đ→dd`…), gõ dấu ở cuối từ.
- Đặt dấu thanh đúng chính tả (`qu`/`gi`, `ươ`, `uyê`…), hỗ trợ cả kiểu cũ
  (`hòa`) lẫn kiểu mới (`hoà`).
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
bàn phím được kích hoạt:

| Giá trị       | Mặc định | Ý nghĩa                                        |
|---------------|----------|------------------------------------------------|
| `InputMethod` | `0`      | `0` = Telex, `1` = VNI                         |
| `ToneStyle`   | `0`      | `0` = kiểu cũ (hòa), `1` = kiểu mới (hoà)      |
| `WShorthand`  | `1`      | Telex: phím `w` đơn → `ư`                      |
| `DelayedD`    | `1`      | Telex: `d` cuối từ → `đ` (`dun` + `d` → `đun`) |

## Lộ trình

- [ ] Port macOS (IMKit) và Linux (fcitx5) trên cùng engine
- [ ] Kiểm tra chính tả âm tiết + khôi phục từ không phải tiếng Việt
- [ ] App cấu hình + phím tắt bật/tắt tiếng Việt
- [ ] Telex mở rộng (`[` `]`), gõ tắt người dùng định nghĩa
