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
  mật khẩu hay ô số (hỏi qua `ITfInputScope`, không đoán mò); từ điển
  17.467 từ tiếng Anh trả lại nguyên chữ lúc chốt từ (`meeting`, `server`
  thay vì `mêting`, `sẻver`); và `Ctrl + Backspace` khi đang gõ dở để bỏ
  dấu cho **riêng từ đó**.
- **Sửa dấu cho chữ đã chốt** (`ITfFnReconversion`): bôi đen một chữ — hoặc
  đặt con trỏ vào giữa nó — rồi đổi sang phương án khác, không phải gõ lại
  cả từ (`duong` → dương, đường, duống, đuông…).
- Composition chuẩn TSF: đoạn đang gõ gạch chân, Backspace xóa một ký tự
  (dấu thanh tự lùi đúng chỗ), Esc trả lại chuỗi phím thô, Enter/Tab/chuột
  chốt từ tự nhiên.

## Cấu trúc

```
engine/                   Lõi bộ gõ — C++17 thuần, không phụ thuộc OS + C ABI
wordlist/                 Từ điển tiếng Anh + đoán dấu — tầng riêng, portable
platform/windows/src      TSF text service (COM DLL, không ATL/WRL)
platform/windows/config   Tiến trình nền: khay hệ thống + hộp thoại cấu hình
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

Chất lượng bộ test: độ phủ **98% dòng / 81% nhánh**, điểm **mutation testing
86,9%** (gieo lỗi vào engine rồi xem test có bắt không — thước đo thật, vì
phủ 97% dòng mà mutation vẫn từng tìm ra 130 lỗ hổng), và engine sạch dưới
**AddressSanitizer + UBSan** qua hơn 11 triệu lượt kiểm tra. Chi tiết ở
[docs/mutation-testing.md](docs/mutation-testing.md):

```sh
python3 tools/mutation_test.py --asan                    # mutation testing
gcovr --root . --filter 'engine/src/' --filter 'wordlist/src/' \
      --txt --branches                                   # độ phủ
```

Thử engine ngay trên terminal:

```sh
echo "xin chaof thees giowis" | ./build/hodion_demo
# → xin chào thế giới
echo "vie6t5 nam" | ./build/hodion_demo --vni
# → việt nam
echo "hopj meeting vowis server" | ./build/hodion_demo
# → họp meeting với server        (từ điển tiếng Anh, mặc định bật)
echo "hopj meeting vowis server" | ./build/hodion_demo --no-english
# → họp mêting với sẻver
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

**Từ điển tiếng Anh.** Lúc chốt từ, nếu chuỗi phím vừa gõ là một từ tiếng
Anh đã biết thì HodionKey trả lại nguyên chuỗi đó: `meeting` chứ không phải
`mêting`, `server` chứ không phải `sẻver`. Bảng có 17.467 từ và **đã loại
sạch những từ ghép ra một âm tiết tiếng Việt thật** — `bans`→bán,
`cans`→cán, `bust`→bút, `bits`→bít, `test`→tét. Ở những từ đó không có bằng
chứng nào phân xử được người dùng muốn gì, nên bộ gõ giữ nguyên chữ tiếng
Việt; muốn từ tiếng Anh thì bấm `Ctrl + Backspace`. Nhờ luật đó, từ điển
**không bao giờ ghi đè lên một âm tiết tiếng Việt hợp lệ** — tính chất này
được kiểm lại trên từng từ mỗi lần chạy test.

VNI không cần tính năng này: phím dấu của VNI là chữ số nên không từ tiếng
Anh nào bị biến dạng (đo trên cả 63.072 từ: đúng 0 từ).

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

## Phím tắt

Bốn hành động, **mỗi cái đặt được phím riêng** trong app cấu hình — bấm tổ
hợp vào ô là xong, bấm `Delete` để tắt hẳn phím đó:

| Hành động | Mặc định |
|---|---|
| Chuyển Việt / Anh | `Ctrl + Space` |
| Chuyển Telex / VNI | tắt |
| Bật/tắt tự thêm dấu | tắt |
| Bỏ dấu cho **một** từ đang gõ | `Ctrl + Backspace` |

Mỗi phím **bắt buộc có** `Ctrl`, `Alt` hoặc `Shift`: phím trần sẽ bị bộ gõ
nuốt mất và bạn không gõ được ký tự đó nữa. App cấu hình từ chối lưu nếu
thiếu modifier hoặc nếu hai hành động trùng tổ hợp.

Ba phím đầu đăng ký với Windows nên chạy cả khi không gõ dở. Phím thứ tư thì
không — nó **chỉ có hiệu lực khi đang gõ dở một từ**, nhờ vậy
`Ctrl + Backspace` (xóa một từ) của ứng dụng vẫn nguyên vẹn lúc bình thường.

Trạng thái Việt/Anh dùng chung cho mọi ứng dụng đang gõ (lưu ở registry),
đồng bộ với chỉ báo IME của Windows và với icon ở khay hệ thống — đổi ở đâu
cũng khớp.

## Sửa dấu cho chữ đã gõ rồi

Bôi đen một chữ, hoặc chỉ đặt con trỏ vào giữa nó, rồi gọi lệnh chuyển đổi
lại của ứng dụng (Word, WordPad và nhiều ứng dụng Office có; ứng dụng nào
không hỗ trợ `ITfFnReconversion` thì chưa dùng được — phím tắt riêng nằm
trong lộ trình).

HodionKey trả về mọi âm tiết tiếng Việt có cùng chuỗi chữ cái gốc, xếp chữ
đang có lên đầu rồi tới những chữ khác nó ít nhất:

```
duong  → duông duống duồng … dương dường … đường đượng
đường  → đường dường đương đướng đưởng … duộng
```

Danh sách chỉ chứa chữ mà **gõ tay cũng ra được** — nó được sinh bằng cách
cho chính engine gõ lại chuỗi chữ cái gốc với mọi tổ hợp phím dấu, chứ
không phải duyệt bảng vần (duyệt bảng sẽ đẻ ra "gía", "quýen", "dưong" —
những chữ bộ gõ không bao giờ sinh ra).

## Tiến trình nền và khay hệ thống

`HodionKeyConfig.exe` là **tiến trình thường trú** của bộ gõ, không phải một
hộp thoại chạy rồi thoát. Nó giữ icon trạng thái ở khay hệ thống:

- **V đỏ** — đang gõ tiếng Việt, **E xám** — đang tắt. Hai trạng thái khác
  nhau ở *chữ* chứ không chỉ ở màu, để phân biệt được cả ở 16×16 lẫn khi
  mù màu.
- **Bấm trái** bật/tắt tiếng Việt; **chuột phải** mở menu (kiểu gõ, tự thêm
  dấu, cấu hình, khởi động cùng Windows, thoát). Menu hiện luôn phím tắt của
  từng mục.
- Icon bám theo phím `Ctrl + Space` bấm trong ứng dụng khác — trạng thái
  đi qua registry nên hai bên luôn khớp.

Nó cũng là chỗ ở của **phần logic nặng**: mô hình 20–50 MB không thể nạp vào
từng ứng dụng đang gõ, nên nó nằm ở một tiến trình duy nhất và DLL hỏi sang
qua named pipe. Kênh này chở chữ người dùng đang gõ, nên pipe mang tên gắn
SID và có DACL chỉ cho chính người đó cộng SYSTEM.

Quan hệ phụ thuộc chỉ có một chiều: **gõ không cần tiến trình này**. Nó
tắt, chưa chạy hay treo thì bộ gõ vẫn gõ y như cũ, không chậm một mili giây
nào.

Đừng chạy nó bằng quyền Administrator: khi đó nó ở integrity level cao hơn
các ứng dụng thường và chúng sẽ không nói chuyện được với nó.

## Tự thêm dấu cho chữ không dấu

Bật trong menu khay hoặc app cấu hình (**mặc định tắt** — nó đổi thứ bạn vừa
gõ, nên phải là lựa chọn tường minh). Lúc chốt từ, DLL hỏi tiến trình nền và
chỉ đổi khi chữ đó có **đúng một** cách viết có dấu tồn tại thật:

```
nguyet  →  nguyệt        (chỉ có một cách viết)
thuo    →  thuở
toan    →  toan          (để nguyên: toan/toàn/toán/toản — không có căn cứ)
duong   →  duong         (để nguyên: 7 cách viết có thật)
```

Đó mới là **nửa dễ** — chỉ 15,6% chuỗi không dấu có đúng một cách viết. Nửa
còn lại cần ngữ cảnh, và đó là việc của **mô hình 3-gram**: khi có nó,
`buoi toi` ra *buổi tối* còn `toi qua duong` ra *tôi qua đường*.

Đo trên 40.324 âm tiết của phần kho văn bản **không dùng để train** (98,2%
số chỗ là nhập nhằng):

| | có đổi chữ | đúng khi đổi | đúng chung |
|---|---|---|---|
| Không mô hình (chỉ chỗ duy nhất) | 1,8% | 100% | 12,5% |
| Có mô hình, ngưỡng 0 (đoán mọi chỗ) | 89,5% | 85,2% | 85,5% |
| Có mô hình, **ngưỡng 2,0 (mặc định)** | 43,3% | **95,6%** | 52,9% |
| Cả câu, Viterbi (cho reconversion) | — | — | **94,5%** |

Mặc định chọn ngưỡng 2,0 chứ không phải 0, dù ngưỡng 0 cho "đúng chung" cao
hơn nhiều. Lý do: với một bộ gõ, **đổi sai tệ hơn là không đổi**. Chữ còn
không dấu thì người dùng nhìn thấy ngay và sửa; chữ sai dấu thì trông như đã
xong và lọt qua. Ngưỡng 0 làm sai 1 trong 7 lần nó ra tay — đủ để mất niềm
tin vào cả tính năng. Chỉnh bằng `PredictMargin` trong registry nếu muốn
đánh đổi khác.

Chênh lệch giữa 52,9% (lúc gõ) và 94,5% (cả câu) là cái giá của việc **chỉ
nhìn sang trái**. Lúc gõ thì chữ bên phải chưa tồn tại, và giải mã lại cả
câu sau mỗi từ sẽ làm chữ đã hiện trên màn hình tự đổi sau lưng người dùng —
khó chịu hơn hẳn đoán sai. Chữ đã chốt là chốt.

### Nếu tiến trình nền chết hoặc treo

Gõ vẫn y như cũ — đó là ràng buộc thiết kế, không phải may mắn:

- Mỗi lần hỏi có **hạn cứng 20 ms**, và chỉ hỏi lúc chốt từ chứ không phải
  mỗi phím.
- Quá hạn hay mất kết nối thì DLL bỏ qua và chốt từ như bình thường.
- **Hỏng rồi thì im một lúc**: 3 giây nếu không kết nối được, **30 giây nếu
  quá hạn**. Không có bước này thì một host đang treo sẽ làm mỗi lần chốt từ
  tốn đúng 20 ms — bộ gõ ì thấy rõ dù kỹ thuật vẫn "không chờ lâu".
- Mở lại tiến trình nền là dùng được ngay, không phải khởi động lại ứng dụng
  đang gõ.

**Thứ tự khởi động không quan trọng.** Bộ gõ nằm trong ứng dụng từ lúc đăng
nhập, tiến trình nền mở lúc nào cũng được — hai bên không cần biết nhau tồn
tại. Mỗi lần chốt từ là một lần nối mới, nên host mở sau, tắt đi mở lại, hay
bị kill rồi chạy lại đều tự nối lại ở lần chốt từ kế tiếp.

Tắt hẳn tính năng thì bỏ dấu tích trong menu khay hoặc app cấu hình; lúc đó
DLL không hỏi gì nữa.

### Cấu hình được lưu lại

Mọi tuỳ chọn nằm ở `HKCU\Software\HodionKey` và được ghi ngay khi đổi (bấm
OK trong hộp thoại, hoặc chọn trong menu khay), nên khởi động lại máy vẫn
nguyên. Test kiểm **mọi trường** đều sống qua một vòng ghi/đọc, để không ai
thêm tuỳ chọn mới mà quên lưu.

### Dữ liệu ngôn ngữ không nằm trong repo

Tính năng này cần hai file, và cả hai **cố ý không được commit** — vì giấy
phép của nguồn dữ liệu, không phải vì kỹ thuật. Từ điển chính tả tiếng Việt
sẵn có (`hunspell-vi` của LibreOffice) là **GPL-2**; Wikipedia tiếng Việt là
**CC BY-SA**. Đưa dữ liệu dẫn xuất vào đây là ràng cả dự án vào giấy phép
đó, và đó là quyết định của chủ dự án chứ không phải của một script. Tự sinh
trên máy mình rồi dùng thì không phát hành lại gì cả.

```sh
cmake --build build --target hodion_wordscan

# 1. Bảng âm tiết có thật (~6.500 mục, 100 KB)
sudo apt-get install hunspell-vi
python3 tools/build_syllables.py \
    --dic  /usr/share/hunspell/vi_VN.dic \
    --scan build/engine/hodion_wordscan \
    --out  viet-syllables.txt

# 2. Mô hình 3-gram (~46 MB) — cần một kho văn bản tiếng Việt có dấu
python3 tools/train_ngram.py \
    --corpus vi.txt \
    --scan   build/engine/hodion_wordscan \
    --out    viet-ngram.bin
```

Đặt cả hai cạnh `HodionKeyConfig.exe`. Thiếu bảng âm tiết thì mục menu bị
làm mờ kèm lý do; có bảng mà thiếu mô hình thì vẫn đoán được những chữ chỉ
có một cách viết. Mô hình dùng cho bản đo ở trên train từ 250 MB Wikipedia
tiếng Việt: 7.137 âm tiết trong từ vựng, 3,39 triệu trigram, nạp mất 119 ms
và mỗi lần hỏi tốn 108 µs.

## Cấu hình

Chuột phải icon khay → **Cấu hình…** (hoặc chạy `HodionKeyConfig.exe`). Bấm
OK là các ứng dụng đang gõ nhận cấu hình mới ngay (text service theo dõi
registry), không phải khởi động lại gì cả.

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
| `EnglishDetect` | `1`      | Trả lại nguyên chữ với từ tiếng Anh đã biết      |
| `VietnameseOn`  | `1`      | Đang bật gõ tiếng Việt (phím chuyển ghi vào đây) |
| `SkipInputScopes` | `1`    | Tự tắt ở ô URL / email / mật khẩu / ô số        |
| `AutoDiacritics` | `0`     | Tự thêm dấu cho chữ không dấu (cần tiến trình nền) |
| `PredictMargin` | `20`     | Ngưỡng tin cậy ×10; cao hơn = ít đổi hơn nhưng đúng hơn |
| `ToggleKey` / `ToggleMods` | `0x20` / `2` | Chuyển Việt / Anh |
| `MethodKey` / `MethodMods` | `0` / `0` | Chuyển Telex / VNI (0 = tắt) |
| `PredictKey` / `PredictMods` | `0` / `0` | Bật/tắt tự thêm dấu (0 = tắt) |
| `CancelKey` / `CancelMods` | `0x08` / `2` | Bỏ dấu cho một từ đang gõ |

Modifier: 1 = Alt, 2 = Ctrl, 4 = Shift (cộng dồn). Virtual-key `0` nghĩa là
**tắt hẳn** phím đó. Có virtual-key mà modifier bằng 0 là giá trị hỏng —
phím trần sẽ nuốt mất phím đó khi gõ — nên nó tự quay về mặc định.

## Lộ trình

- [x] Bộ luật Telex/VNI tương thích UniKey (spell-check, gõ dấu tự do,
      khôi phục từ không phải tiếng Việt, `[ ]`, họ vần uo…)
- [x] Phím chuyển Việt/Anh + app cấu hình (áp dụng tức thì)
- [x] Gõ trộn Việt–Anh: tự tắt theo input scope + phím bỏ dấu cho một từ
- [x] Gõ trộn Việt–Anh: từ điển Anh quyết định lúc chốt từ
- [x] Tiến trình nền làm host cho logic nặng (named pipe, hạn cứng 20 ms)
- [x] Tự thêm dấu cho chữ không dấu: mô hình 3-gram, 95,6% đúng khi ra tay
- [ ] Phím xoay vòng đổi dấu tại chỗ + chỉ báo độ tin cậy
- [x] Reconversion — sửa từ đã chốt không phải gõ lại
- [ ] Port macOS (IMKit) và Linux (fcitx5) trên cùng engine
- [ ] Gõ tắt (macro) người dùng định nghĩa, VIQR
- [x] Icon trạng thái + menu ở khay hệ thống, khởi động cùng Windows

Thiết kế và lý do của các mục chưa làm — kể cả những thứ **quyết định
không làm** (picker trong luồng gõ, tự sửa chính tả theo từ điển) — ở
[docs/roadmap-smart-input.md](docs/roadmap-smart-input.md).
