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
- **Phím chuyển Việt/Anh** (mặc định `Ctrl + Space`) và **app cấu hình** —
  đổi tùy chọn là có hiệu lực ngay, không phải khởi động lại ứng dụng đang gõ.
- **Gõ trộn Việt – Anh**: tự tắt ở ô mà ứng dụng khai là địa chỉ web, email,
  mật khẩu hay ô số (hỏi qua `ITfInputScope`, không đoán mò); và
  `Ctrl + Backspace` khi đang gõ dở để bỏ dấu cho **riêng từ đó**
  (`te` → `Ctrl+Backspace` → gõ tiếp `st` ra `test` thay vì `tét`).
- Composition chuẩn TSF: đoạn đang gõ gạch chân, Backspace xóa một ký tự
  (dấu thanh tự lùi đúng chỗ), Esc trả lại chuỗi phím thô, Enter/Tab/chuột
  chốt từ tự nhiên.

## Cấu trúc

```
engine/                   Lõi bộ gõ — C++17 thuần, không phụ thuộc OS + C ABI
platform/windows/src      TSF text service (COM DLL, không ATL/WRL)
platform/windows/config   App cấu hình (hộp thoại Win32 thuần)
platform/windows/tests    Kiểm thử tầng Windows (registry, watcher)
platform/windows/dist     Script cài/gỡ + hướng dẫn cho gói phát hành
docs/                     Kiến trúc, bộ luật UniKey, mutation testing, lộ trình
tools/                    Công cụ dev (mutation testing)
cmake/                    Toolchain cross-compile MinGW (CI trên Linux)
```

Chi tiết thiết kế: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Build

### Engine + test (mọi hệ điều hành)

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Bộ test gồm các ca cụ thể (Telex, VNI, edge case) và một **fuzzer có hạt
giống cố định**: gõ ngẫu nhiên với Backspace xen giữa, cấu hình ngẫu nhiên,
kiểm tra bất biến sau *từng* thao tác — kể cả bất biến nội bộ của engine
(`Engine::self_check`). Khi vỡ bất biến, test in ra đúng chuỗi thao tác để
tái hiện. Chạy soak lâu hơn và xem thống kê:

```sh
HODION_FUZZ_ROUNDS=200000 HODION_FUZZ_STATS=1 ./build/engine/hodion_engine_tests
```

Chất lượng bộ test: độ phủ **99% dòng / 81% nhánh**, điểm **mutation testing
86,9%** (gieo lỗi vào engine rồi xem test có bắt không — thước đo thật, vì
phủ 97% dòng mà mutation vẫn từng tìm ra 130 lỗ hổng), và engine sạch dưới
**AddressSanitizer + UBSan** qua hơn 11 triệu lượt kiểm tra. Chi tiết ở
[docs/mutation-testing.md](docs/mutation-testing.md):

```sh
python3 tools/mutation_test.py --asan                    # mutation testing
gcovr --root . --filter 'engine/src/' --txt --branches   # độ phủ
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

Kết quả: `build\platform\windows\Release\HodionKey.dll` (text service) và
`HodionKeyConfig.exe` (app cấu hình).

Muốn gõ được cả trong **ứng dụng 32-bit** thì build và đăng ký thêm bản
x86 — IME 64-bit không nạp được vào tiến trình 32-bit:

```bat
cmake -S . -B build32 -A Win32
cmake --build build32 --config Release
```

(Cross-compile từ Linux bằng MinGW: `cmake/mingw-w64-x86_64.cmake` cho x64,
`cmake/mingw-w64-i686.cmake` cho x86.)

## Cài đặt trên Windows

Cách nhanh nhất: vào tab **Actions** → mở lần chạy CI mới nhất → kéo xuống
cuối trang **Summary** → tải artifact **`HodionKey-installer`**. Gói này có
sẵn cả x64 lẫn x86 kèm script; giải nén ra thư mục cố định rồi chuột phải
`CaiDat.bat` → *Run as administrator*.

(Artifacts chỉ hiện ở trang Summary của lần chạy, không hiện trong trang
chi tiết từng job.)

Hoặc làm tay:

1. Mở Command Prompt **quyền Administrator**.
2. Đăng ký text service:

   ```bat
   regsvr32 HodionKey.dll
   ```

3. Vào **Settings → Time & language → Language & region**, thêm ngôn ngữ
   **Tiếng Việt** nếu chưa có — bàn phím **HodionKey** xuất hiện trong danh
   sách bàn phím của tiếng Việt. Chuyển bàn phím bằng `Win + Space`.

Bản 32-bit đăng ký bằng `regsvr32` trong `SysWOW64`:

```bat
%SystemRoot%\SysWOW64\regsvr32.exe HodionKey.dll
```

Lưu ý: `regsvr32` ghi nhớ đường dẫn tới DLL, nên hãy đặt file ở thư mục cố
định trước khi đăng ký.

Gỡ: `regsvr32 /u HodionKey.dll` (và bản `SysWOW64` tương ứng).

## Gõ trộn Việt / Anh

Hai cơ chế, không cần mô hình nào:

**Tự tắt theo ô nhập.** Ứng dụng tự khai kiểu dữ liệu của ô qua
`ITfInputScope` — ô địa chỉ web, ô email, ô mật khẩu, ô số. Ở những ô đó
HodionKey không biến đổi gì cả. Ô nào khai nhiều kiểu cùng lúc mà có ít
nhất một kiểu cho phép văn bản tự do (thanh địa chỉ trình duyệt vừa là URL
vừa là ô tìm kiếm) thì vẫn gõ tiếng Việt được — chặn nhầm ô người ta muốn
gõ tiếng Việt tệ hơn là bỏ sót một ô URL. Tắt cơ chế này trong app cấu hình
nếu không thích.

**`Ctrl + Backspace` khi đang gõ dở** bỏ dấu cho riêng từ đang gõ: chữ quay
về đúng chuỗi phím đã bấm, và phần còn lại của từ được gõ thẳng — không
phím nào là phím dấu nữa.

```
gõ  t e            → te
Ctrl + Backspace   → te   (từ này hết biến đổi)
gõ  s t            → test     (không bấm thì ra "tét")
```

Chốt từ xong là tiếng Việt trở lại ngay, không phải bật/tắt gì. Phím này
chỉ hoạt động khi đang gõ dở một từ, nên `Ctrl + Backspace` "xóa một từ"
của ứng dụng vẫn nguyên vẹn.

## Chuyển Việt / Anh

Bấm **`Ctrl + Space`** (mặc định) khi đang gõ để bật/tắt tiếng Việt — không
cần đổi bàn phím. Phím chuyển đổi được trong app cấu hình (`Ctrl + Space`,
`Ctrl + Shift + Space`, `Alt + Z`, `Ctrl + ` `` `, `Alt + ` `` `).

Trạng thái bật/tắt dùng chung cho mọi ứng dụng đang gõ (lưu ở registry), và
đồng bộ với chỉ báo IME của Windows — tắt từ thanh ngôn ngữ cũng có tác dụng.

## Cấu hình

Chạy **`HodionKeyConfig.exe`** để chỉnh bằng hộp thoại. Bấm OK là các ứng
dụng đang gõ nhận cấu hình mới ngay (text service theo dõi registry), không
phải khởi động lại gì cả.

Tùy chọn nằm ở registry `HKCU\Software\HodionKey` (DWORD) nếu muốn sửa tay
hoặc triển khai theo chính sách. Mặc định trùng với mặc định của UniKey:

| Giá trị         | Mặc định | Ý nghĩa                                          |
|-----------------|----------|--------------------------------------------------|
| `InputMethod`   | `0`      | `0` = Telex, `1` = VNI                           |
| `ToneStyle`     | `0`      | `0` = kiểu cũ (hòa), `1` = kiểu mới (hoà)        |
| `FreeMarking`   | `1`      | Gõ dấu tự do — dấu ở cuối từ (`viete→viêt`)      |
| `SpellCheck`    | `1`      | Kiểm tra chính tả âm tiết (từ sai ngừng biến đổi) |
| `RestoreNonVn`  | `0`      | Tự trả lại phím thô với từ không phải tiếng Việt |
| `WShorthand`    | `1`      | Telex: `w` không áp được móc thì thành `ư`       |
| `TelexBrackets` | `1`      | Telex đầy đủ: `[ ] { }` → `ơ ư Ơ Ư`              |
| `VietnameseOn`  | `1`      | Đang bật gõ tiếng Việt (phím chuyển ghi vào đây) |
| `SkipInputScopes` | `1`    | Tự tắt ở ô URL / email / mật khẩu / ô số        |
| `ToggleKey`     | `0x20`   | Virtual-key của phím chuyển (`0x20` = Space)     |
| `ToggleMods`    | `2`      | Modifier: 1 = Alt, 2 = Ctrl, 4 = Shift (cộng dồn) |

`ToggleMods` bắt buộc khác 0 — phím chuyển không có modifier sẽ nuốt mất
phím đó khi gõ, nên giá trị hỏng sẽ tự quay về mặc định.

## Lộ trình

- [x] Bộ luật Telex/VNI tương thích UniKey (spell-check, gõ dấu tự do,
      khôi phục từ không phải tiếng Việt, `[ ]`, họ vần uo…)
- [x] Phím chuyển Việt/Anh + app cấu hình (áp dụng tức thì)
- [x] Gõ trộn Việt–Anh: tự tắt theo input scope + phím bỏ dấu cho một từ
- [ ] Gõ trộn Việt–Anh: từ điển Anh quyết định lúc chốt từ
- [ ] Tự thêm dấu cho chữ không dấu (n-gram + Viterbi, tiến trình riêng)
- [ ] Reconversion — sửa từ đã chốt không phải gõ lại
- [ ] Port macOS (IMKit) và Linux (fcitx5) trên cùng engine
- [ ] Gõ tắt (macro) người dùng định nghĩa, VIQR
- [ ] Chỉ báo trạng thái trên thanh ngôn ngữ / khay hệ thống

Thiết kế và lý do của các mục chưa làm — kể cả những thứ **quyết định
không làm** (picker trong luồng gõ, tự sửa chính tả theo từ điển) — ở
[docs/roadmap-smart-input.md](docs/roadmap-smart-input.md).
