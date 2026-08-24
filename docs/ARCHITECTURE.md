# Kiến trúc HodionKey

## Nguyên tắc thiết kế

1. **Engine tách rời tuyệt đối khỏi hệ điều hành.** `engine/` chỉ dùng C++17
   chuẩn — không `windows.h`, không COM, không giả định gì về host. Toàn bộ
   nghiệp vụ tiếng Việt (Telex/VNI, dấu phụ, dấu thanh, luật hủy, backspace)
   nằm ở đây và được phủ unit test chạy trên mọi nền tảng.
2. **Tầng platform càng mỏng càng tốt.** `platform/windows/` chỉ làm ba việc:
   dịch phím → ký tự, chuyển kết quả engine → thao tác composition của TSF,
   và đăng ký COM/profile. Không một luật tiếng Việt nào nằm ở tầng này.
3. **Tương thích hành vi UniKey.** Bộ luật gõ (Telex/VNI, kiểm tra chính tả
   âm tiết, gõ dấu tự do, luật hủy, họ vần uo…) được nghiên cứu từ mã nguồn
   ukengine và cài đặt lại từ đầu — đặc tả đầy đủ ở
   [unikey-rules.md](unikey-rules.md).

```
┌────────────────────────────────────────────────────────┐
│                       Ứng dụng                         │
└──────────────▲─────────────────────────────────────────┘
               │ composition / commit text
┌──────────────┴─────────────────────────────────────────┐
│  Tầng platform (mỏng)                                  │
│  • Windows: TSF text service (COM, ITfKeyEventSink…)   │
│  • macOS:   IMKit input controller        (kế hoạch)   │
│  • Linux:   fcitx5 / ibus addon           (kế hoạch)   │
└──────────────▲─────────────────────────────────────────┘
               │ process_char / backspace → Result{action, text}
┌──────────────┴─────────────────────────────────────────┐
│  hodion_engine (C++17 thuần + C ABI)                   │
│  raw keys ──▶ composer (luật Telex/VNI)                │
│           ──▶ tone placement (qu/gi, ươ, oa/oe/uy…)    │
│           ──▶ chartable (Unicode dựng sẵn NFC)         │
└────────────────────────────────────────────────────────┘
```

## Engine (`engine/`)

### Giao diện

`hodion::Engine` (C++) và `hodion_engine_*` (C ABI trong
`include/hodion/engine_c.h`, phục vụ FFI khi port). Host chỉ cần hiểu ba
hành động:

| Action      | Ý nghĩa với host                                            |
|-------------|-------------------------------------------------------------|
| `None`      | Phím không thuộc engine — cho đi thẳng tới ứng dụng         |
| `Composing` | Cập nhật chuỗi composition đang hiển thị (`text`)           |
| `Commit`    | Chốt `text` (đã gồm ký tự ngắt từ nếu có) và đóng composition |

### Luồng dữ liệu

Engine gồm ba tầng, mirror mô hình của UniKey nhưng viết lại từ đầu:

1. **`vnlexi`** — dữ liệu từ vựng: 69 vần hợp lệ (kèm cờ trọn vẹn/nhận âm
   cuối và đích khi thêm mũ/móc), 30 chuỗi phụ âm, bảng cặp vần–âm cuối,
   luật phụ âm đầu (k/gi/qu) và ngoại lệ (quynh, giêng), hàm vị trí dấu
   thanh. Bảng nguồn viết dạng chuỗi UTF-8 dễ đọc, phân giải lúc khởi tạo.
2. **`word`** — máy trạng thái một từ: mỗi ô (ký tự hiển thị) mang snapshot
   cấu trúc âm tiết của từ tính đến ô đó (dạng từ, id vần/phụ âm, liên
   kết), dấu thanh nằm trên ô và di chuyển khi cấu trúc đổi. Các sự kiện
   `roof/hook/tone/stroke_d/telex_w/map_char/append` cài đủ luật UniKey:
   kiểm tra chính tả đóng băng từ sai, gõ dấu tự do xuyên âm cuối, họ vần
   uo (ươ, ngoại lệ thuơ, tự hoàn thành ưo→ươ), gi/gin mang thanh, hủy khi
   gõ lặp… Backspace chỉ việc bỏ ô cuối (kèm lùi dấu thanh).
3. **`chartable`** tra (chữ gốc, dấu phụ, thanh) → codepoint Unicode dựng
   sẵn (NFC) — dạng chuẩn mọi ứng dụng hiển thị đúng.

`engine.cpp` điều phối: phân loại phím theo kiểu gõ (Telex/VNI), ngắt từ
theo danh sách của UniKey, ghi log phím thô của từ (phục vụ Esc và tùy chọn
khôi phục từ không phải tiếng Việt), chốt từ khi gặp ký tự ngắt.

Đặc tả hành vi đầy đủ (kèm ví dụ vàng): [unikey-rules.md](unikey-rules.md).

### Cách engine được kiểm chứng

Ngoài các ca cụ thể, `tests/test_fuzz.cpp` gõ ngẫu nhiên với Backspace xen
giữa và cấu hình ngẫu nhiên, kiểm tra sau **từng thao tác**:

- `Engine::self_check()` — bất biến nội bộ: chỉ số liên kết hợp lệ, dấu đặt
  đúng chỗ, glyph của vần khớp bảng, mỗi âm tiết nhiều nhất một dấu thanh và
  thanh không nằm ngoài vần, nhật ký phím chỉ tồn tại khi còn tin cậy;
- bất biến quan sát từ ngoài: Backspace xóa đúng một ký tự hiển thị, một
  phím thêm nhiều nhất một ký tự, `Commit` luôn để lại trạng thái sạch, chuỗi
  ra chỉ chứa ASCII hoặc chữ tiếng Việt dựng sẵn;
- tính chất biến hình: gõ cùng chuỗi phím nhưng viết hoa phải cho đúng bản
  viết hoa của kết quả viết thường.

Bộ sinh trộn chuỗi ngẫu nhiên thuần với chuỗi **có hình dạng âm tiết thật**
(phụ âm đầu + vần + âm cuối + dấu, dấu đặt giữa hoặc cuối từ) — nếu chỉ
ngẫu nhiên thuần thì phần lớn lượt chạy hỏng từ ngay ký tự thứ hai và không
chạm được vào các luật thú vị. `HODION_FUZZ_STATS=1` in ra tỉ lệ lượt sinh
được chữ có dấu (~68%) để biết fuzzer không bị thoái hoá.

## Windows TSF (`platform/windows/`)

Text service COM viết tay (không ATL/WRL), một class `CTextService`
implement:

- `ITfTextInputProcessorEx` — vòng đời activate/deactivate theo thread.
- `ITfKeyEventSink` — nhận phím. `OnTestKeyDown` và `OnKeyDown` dùng chung
  một hàm phân loại (`ClassifyKey`) để câu trả lời "nuốt hay không" luôn
  nhất quán:
  - chữ cái (và chữ số ở VNI khi đang ghép) → nuốt, đưa vào engine;
  - ký tự ngắt (space, dấu câu…) khi đang ghép → nuốt, chốt từ + ký tự;
  - Backspace/Esc khi đang ghép → nuốt (hoàn tác / trả chuỗi thô);
  - `Ctrl + Backspace` khi đang ghép → nuốt, hủy biến đổi cho riêng từ đó;
  - Enter/Tab/mũi tên/Ctrl/Alt… khi đang ghép → chốt composition tại chỗ
    rồi **không nuốt** để phím hoạt động bình thường (người dùng không phải
    bấm hai lần);
  - không ghép gì → không đụng phím nào.
- `ITfCompositionSink` — ứng dụng tự kết thúc composition (click chuột…) thì
  engine bỏ trạng thái để không lệch với màn hình.
- `ITfDisplayAttributeProvider` — gạch chân đoạn đang ghép.
- `ITfThreadMgrEventSink` — đổi focus là chốt từ đang gõ dở.
- `ITfCompartmentEventSink` — theo dõi compartment
  `GUID_COMPARTMENT_KEYBOARD_OPENCLOSE` để trạng thái bật/tắt luôn khớp với
  chỉ báo IME của hệ thống (người dùng tắt từ thanh ngôn ngữ cũng ăn).

### Gõ trộn Việt – Anh

Hai cơ chế tất định, không mô hình, không từ điển:

**Ngữ cảnh ô nhập** (`src/InputScope.cpp`). Không đoán ngôn ngữ — hỏi thẳng
ứng dụng qua `ITfInputScope`, lấy theo hai đường: QI trực tiếp trên
`ITfContext` (nhiều ứng dụng để sẵn ở đó), nếu không có thì đọc thuộc tính
`GUID_PROP_INPUTSCOPE` tại vị trí con trỏ trong một edit session **chỉ đọc**.
Hỏi một lần cho mỗi lần đổi focus/context (`OnSetFocus`, `OnPushContext`,
`OnPopContext` làm mất hiệu lực cache), không hỏi mỗi phím.

Chỉ tắt khi **mọi** scope mà ô khai báo đều thuộc nhóm "không phải chỗ gõ
tiếng Việt" (URL, đường dẫn, email, tên đăng nhập, mật khẩu/PIN, điện thoại,
ngày/giờ, tiền tệ, chữ số, công thức). Ô khai nhiều kiểu mà có một kiểu cho
phép văn bản tự do — thanh địa chỉ trình duyệt vừa `IS_URL` vừa `IS_SEARCH`
— thì vẫn gõ tiếng Việt được: chặn nhầm ô người ta muốn gõ tiếng Việt tệ
hơn là bỏ sót một ô URL. `IS_PERSONALNAME_*` và `IS_ADDRESS_*` **không**
nằm trong nhóm chặn — tên người và địa chỉ Việt Nam rất cần dấu.

Hỏi hỏng (ứng dụng không cho edit session đồng bộ, không khai scope) thì
mặc định là **vẫn gõ tiếng Việt** — hỏng theo hướng không đổi hành vi cũ.

**Từ điển tiếng Anh** (`wordlist/`, xem mục riêng bên dưới). Lúc chốt từ,
nếu chuỗi phím thô là một từ tiếng Anh đã biết thì engine trả lại nguyên
chuỗi đó. Chỉ tra **một lần cho mỗi từ**, ở thời điểm chốt — không nằm trên
đường gõ từng phím.

**Hủy biến đổi cho một từ** (`Engine::cancel_transform`, gắn vào
`Ctrl + Backspace`). Engine chuyển sang *chế độ gõ thẳng*: chữ quay về đúng
chuỗi phím đã bấm và mọi phím còn lại của từ chỉ được nối nguyên văn — `s`,
`f`, `w`, `6`… hết là phím dấu. Chốt từ là thoát chế độ đó, không cần bật
tắt gì. Chế độ này nằm **trong engine** chứ không ở tầng Windows, nên macOS
và Linux dùng lại nguyên vẹn.

Phím chỉ bị chiếm khi đang gõ dở một từ, nên `Ctrl + Backspace` "xóa một
từ" của ứng dụng không mất.

### Bật/tắt tiếng Việt và cấu hình

Phím chuyển đăng ký bằng `ITfKeystrokeMgr::PreserveKey` (mặc định
`Ctrl + Space`) nên TSF giao thẳng qua `OnPreservedKey`, không lẫn vào luồng
phím thường; phím chuyển bắt buộc có modifier để không nuốt mất một phím gõ.
Khi tắt, `ClassifyKey` trả `NotOurs` cho mọi phím — ứng dụng nhận phím
nguyên vẹn.

Cấu hình nằm ở `HKCU\Software\HodionKey`, dùng chung giữa text service
(`src/Settings.cpp`) và app cấu hình (`config/`). Text service không đọc
registry mỗi phím: nó đăng ký `RegNotifyChangeKeyValue` một lần rồi chỉ
kiểm tra event (`SettingsWatcher::poll`) lúc đổi focus hoặc giữa hai từ —
nhờ vậy bấm OK trong app cấu hình là mọi ứng dụng đang gõ đổi theo ngay,
kể cả trạng thái bật/tắt.

App cấu hình là hộp thoại Win32 thuần (`DialogBoxParamW`): bố cục trong
`.rc` chỉ dùng ASCII, toàn bộ nhãn tiếng Việt gán lúc chạy bằng
`SetDlgItemTextW` để không lệ thuộc code page của `rc.exe`/`windres`.
`platform/windows/tests/settings_test.cpp` kiểm thử vòng đọc/ghi registry
và cơ chế watcher — chạy được cả trên Windows lẫn dưới Wine.

Mọi thao tác văn bản đi qua **edit session đồng bộ** (`TF_ES_SYNC |
TF_ES_READWRITE`) xin từ key event sink — đúng mô hình luồng TSF. Composition
được mở bằng `ITfContextComposition::StartComposition` trên range lấy từ
`ITfInsertAtSelection(TF_IAS_QUERYONLY)`, cập nhật bằng `ITfRange::SetText`,
caret luôn đưa về cuối.

Đăng ký (`Register.cpp`): CLSID dưới `HKCR\CLSID`, profile tiếng Việt
(LANGID `0x042A`) qua `ITfInputProcessorProfileMgr::RegisterProfile`, và các
category TSF (keyboard TIP, display attribute provider, immersive/systray
support, UI-less, comless, secure mode) — đủ điều kiện chạy trên Win10/11
kể cả ứng dụng UWP và màn hình khóa.

`TsfCompat.h` bù vài định nghĩa thiếu trong header MinGW để có thể
cross-compile/CI từ Linux; trên MSVC các guard tự vô hiệu.

## Từ điển tiếng Anh (`wordlist/`)

Tầng **riêng, tùy chọn**: lõi engine không chứa dữ liệu nào và vẫn build,
chạy, pass test đầy đủ khi không có thư viện này. Engine chỉ khai một giao
diện để hỏi:

```cpp
class ForeignWords {
 public:
  virtual bool contains(const std::u32string& key) const = 0;
};
void Engine::set_foreign_words(const ForeignWords*);
```

Nhờ vậy chiều phụ thuộc luôn là `wordlist → engine`, không bao giờ ngược
lại, và bản port macOS/Linux tự chọn có nạp hay không.

### Luật chốt từ

Có hai lý do độc lập để trả lại chuỗi phím thô thay vì chữ đã ghép:

1. **Từ điển nhận ra một từ tiếng Anh.** Đây là bằng chứng *duy nhất* có
   được khi chữ ghép ra vẫn là âm tiết tiếng Việt hợp lệ.
2. **Chữ ghép ra không phải âm tiết tiếng Việt hợp lệ** (`restore_non_vn`,
   mirror `autoNonVnRestore` của UniKey) — luật thuần cấu trúc, không cần
   dữ liệu.

Cả hai chỉ chạy khi nhật ký phím còn tin được (chưa Backspace) và thật sự
có phím gây biến đổi. Chuỗi chốt vì thế **luôn là một trong hai thứ**: chữ
đã ghép, hoặc đúng chuỗi phím đã gõ — không bao giờ là thứ ba. Đây là một
tính chất được test kiểm trực tiếp.

### Bảng dữ liệu và luật lọc

Nguồn: SCOWL (gói `wamerican`), giấy phép cho phép phát hành lại —
`wordlist/data/SCOWL-COPYRIGHT.txt` phải đi kèm bản phát hành.
`tools/build_wordlist.py` sinh ra `wordlist/data/english_telex.inc`; kết quả
được commit nên build bình thường không cần script, không cần mạng.

Từ 63.072 từ tiếng Anh (4–24 ký tự, chỉ a–z):

| Bước                                          | Còn lại |
|-----------------------------------------------|---------|
| gõ Telex ra đúng chính nó → bỏ (không có gì để khôi phục) | 18.356 |
| ghép ra một âm tiết tiếng Việt **hợp lệ** → bỏ | 17.467 |

Luật thứ hai mới là điều quan trọng. 889 từ bị loại gồm `bans`→bán,
`cans`→cán, `bust`→bút, `bits`→bít, `bốn`, `cáp`, và cả `test`→tét. Ở những
từ đó không có bằng chứng nào phân xử được người dùng muốn gì — và những
âm tiết ấy (bán, bút, bít, bốn) phổ biến hơn hẳn. Nên bộ gõ **không được tự
quyết**: giữ nguyên chữ tiếng Việt, ai muốn từ tiếng Anh thì bấm
`Ctrl + Backspace`. Đó chính là lý do phím kia tồn tại.

Hệ quả là một tính chất mạnh: **bảng không bao giờ ghi đè lên một âm tiết
tiếng Việt hợp lệ.** `wordlist/tests` kiểm lại nó trên từng từ ở cả 4 biến
thể cấu hình Telex (17.467 × 4 lượt) mỗi lần chạy CI — không tin script.

VNI không cần tính năng này: phím dấu của VNI là chữ số nên không từ tiếng
Anh nào bị biến dạng (đo trên cả 63.072 từ: đúng 0 từ). App cấu hình làm mờ
tùy chọn khi chọn VNI.

### Cách lưu

Các bản ghi xếp theo độ dài thành từng khối **độ dài cố định, không ký tự
ngăn cách**, mỗi khối sắp tăng dần. Tra bằng tìm nhị phân thẳng trên mảng:
không bảng chỉ mục, không con trỏ, không cấp phát, không khởi tạo lúc nạp
DLL — 149 KB dữ liệu thuần, dùng chung được cho mọi thread. (Một mảng
`const char*` sẽ tốn thêm 140 KB con trỏ; một blob có `\0` ngăn cách thì
không tìm nhị phân được.)

## Kế hoạch port

- **macOS**: target IMKit (`IMKInputController`). Swift gọi engine qua C ABI
  (`engine_c.h`); phần việc tương đương `KeyHandler.cpp` + composition của
  NSTextInputClient.
- **Linux**: addon fcitx5 (C++ dùng thẳng API C++ của engine) hoặc ibus.
- Hai port này chỉ cần viết lại tầng mỏng; engine + test giữ nguyên.

## Lộ trình tính năng

- Gõ tắt (macro) do người dùng định nghĩa; kiểu gõ VIQR, Simple Telex.
- App cấu hình (thay cho sửa registry tay) + phím tắt bật/tắt tiếng Việt
  (compartment input mode).
- Trang trạng thái trên language bar / systray.
