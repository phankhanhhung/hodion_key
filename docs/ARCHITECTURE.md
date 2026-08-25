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
- `ITfFunctionProvider` + `ITfFnReconversion` — sửa dấu cho chữ đã chốt
  (xem mục riêng bên dưới).
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

### Sửa dấu cho chữ đã chốt (reconversion)

Người dùng bôi đen một chữ — hoặc chỉ đặt con trỏ vào giữa nó — và đổi sang
phương án khác, không phải gõ lại cả từ. Ứng dụng hỏi qua
`ITfFunctionProvider::GetFunction(GUID_NULL, IID_ITfFnReconversion)`;
`CTextService` trả về chính nó.

- `QueryRange` tìm đúng đoạn sẽ đổi rồi trả về; chỉ nhận khi có **ít nhất
  hai** phương án, để ứng dụng không hiện một mục chọn vô nghĩa.
- `GetReconversion` trả về `ITfCandidateList` (danh sách + enumerator + từng
  chuỗi, cài tay trong `Reconversion.cpp`) lấy từ `hodion::syllable_variants`.
- `Reconvert` — dành cho ứng dụng không có giao diện chọn riêng — xoay sang
  phương án kế tiếp; gọi lại nhiều lần thì đi hết vòng.

Danh sách phương án do **engine** sinh (`engine/src/reconvert.cpp`), nên
macOS/Linux dùng lại nguyên vẹn.

**Ranh giới từ và chuyện đọc lại trước khi ghi.** Range là do ứng dụng cấp,
còn ta tính toán trên một bản chụp văn bản. Ghi nhầm chỗ ở đây nghĩa là
xóa/sửa chữ của người dùng, nên có ba lớp chặn:

1. Đoạn bôi đen dài hơn một âm tiết (8 ký tự) bị từ chối thẳng — thay một
   đoạn dài bằng một âm tiết là xóa mất văn bản.
2. Sau khi dịch `ITfRange` để bao đúng từ, **đọc lại** đoạn đó và so với
   chuỗi ta định lấy; lệch một ký tự là bỏ, không ghi gì.
3. Lúc người dùng chọn (`SetResult`), **đọc lại lần nữa** và chỉ ghi khi văn
   bản vẫn đúng như lúc dựng danh sách — họ có thể đã gõ tiếp trong lúc
   danh sách đang mở.

Phần dò ranh giới từ tách ra `WordScan.cpp` để test được mà không cần một
ứng dụng TSF thật — nó là chỗ logic thuần và dễ sai nhất; số học trên
`ITfRange` thì chỉ chạy thật mới kiểm được, nên mới có ba lớp chặn ở trên.

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

## Tiến trình nền (`platform/windows/config/`)

`HodionKeyConfig.exe` không còn là hộp thoại chạy rồi thoát; nó là tiến
trình thường trú của bộ gõ. Lý do kiến trúc: DLL text service được nạp vào
**từng ứng dụng đang gõ**, nên nó phải nhẹ và không được giữ thứ gì to. Mô
hình đoán dấu 20–50 MB mà nạp vào 30 tiến trình là không chấp nhận được.
Nên mọi thứ nặng sống ở một tiến trình duy nhất cho cả phiên đăng nhập, và
DLL hỏi sang.

Chiều phụ thuộc chỉ có một: **gõ không cần host**. Host tắt, chưa chạy hay
treo thì bộ gõ vẫn chạy y như cũ. Host chỉ thêm những thứ không có cũng
không sao.

| File | Việc |
|---|---|
| `Host.cpp` | vòng đời tiến trình, cửa sổ ẩn, menu, single instance |
| `TrayIcon.cpp` | icon khay hệ thống |
| `ConfigDialog.cpp` | hộp thoại cấu hình (trước đây là cả chương trình) |

Vài chỗ dễ sai đã xử lý:

- **Cửa sổ ẩn phải là cửa sổ cấp cao nhất thật, không phải `HWND_MESSAGE`.**
  Cửa sổ message-only không nhận message quảng bá, mà ta cần đúng hai cái
  đó: `TaskbarCreated` (Explorer khởi động lại thì mọi icon khay bị xoá
  sạch, phải thêm lại) và message đánh thức của phiên bản thứ hai.
- **`TrackPopupMenu` cần `SetForegroundWindow` trước** và một message rỗng
  sau, nếu không menu không tự đóng khi bấm ra ngoài.
- **Icon khay phải nạp đúng cỡ `SM_CXSMICON`** (đổi theo DPI), không phải cỡ
  mặc định, nếu không Windows tự co giãn và nhòe. Đổi DPI thì dựng lại icon.
- **Trạng thái Việt/Anh đi qua registry**, nên phím `Ctrl + Space` bấm trong
  ứng dụng khác cũng đổi icon khay. Vòng lặp thông điệp chờ chung sự kiện
  `RegNotifyChangeKeyValue` (`MsgWaitForMultipleObjects`) thay vì hỏi
  registry theo nhịp.
- **Icon vẽ riêng từng cỡ** (`tools/make_icons.py`, file `.ico` ghép tay).
  `convert ... out.ico` thu nhỏ tất cả từ frame lớn nhất, làm nét chéo chữ V
  nhòe hẳn ở 16×16 — đúng cỡ khay hệ thống hay dùng nhất. Hai trạng thái
  khác nhau ở **chữ** (V/E) chứ không chỉ ở màu.
- **Không chạy bằng quyền Administrator.** Khi đó nó ở integrity level cao
  hơn ứng dụng thường và named pipe của nó sẽ không nhận được kết nối từ
  chúng. Script cài đặt vì thế không tự chạy nó.

### Kênh giữa DLL và host

Named pipe, một yêu cầu một trả lời, khuôn gói tin ở `src/HostChannel.h`
(dùng chung cả hai đầu).

**Bảo mật là chuyện thật ở đây, không phải hình thức: kênh này chở chữ
người dùng đang gõ.** Pipe mang tên gắn SID người dùng (hai người cùng
đăng nhập một máy không đụng nhau) và có DACL tường minh `D:P` chỉ cho
chính người đó cộng SYSTEM. Không dựng được descriptor thì **không mở
kênh** — thà mất tính năng còn hơn để tiến trình khác đọc được, hoặc giả
làm host mà nhét chữ vào.

Hệ quả đã biết: ứng dụng chạy ở integrity level thấp (tab trình duyệt
trong sandbox) không ghi lên được object ở mức trung bình, nên ở đó không
có phần đoán dấu. Đổi lại, host cũng không được chạy bằng quyền
Administrator — khi đó nó ở mức cao và ứng dụng thường mới là bên không
với tới.

**Phía DLL (`HostClient`) không bao giờ chờ lâu.** Hạn cứng 20 ms bằng
overlapped I/O; quá hạn thì `CancelIoEx`, đợi I/O kết thúc thật sự (nếu
không kernel còn đang ghi vào bộ nhớ đã chết), bỏ kết nối và gõ tiếp như
không có gì.

**Hạn cứng thôi thì chưa đủ, và đây là chỗ dễ bỏ sót.** Nếu host TREO mà
mỗi từ ta lại thử lại rồi chờ hết hạn, thì mỗi lần chốt từ tốn đúng 20 ms —
bộ gõ ì thấy rõ dù về lý thuyết vẫn "không chờ lâu". Nên hỏng lần nào là im
một lúc, và im theo hai mức khác nhau:

| Hỏng vì | Im | Vì sao |
|---|---|---|
| Không kết nối được (host chưa chạy) | 3 giây | mỗi lần thử chỉ tốn vài µs; im ngắn để mở host lên là dùng được ngay |
| Quá hạn (host treo) | 30 giây | mỗi lần thử tốn ĐÚNG BẰNG hạn; host treo phải nhanh chóng thành "coi như không có" |
| Đầu kia nói sai giao thức | 3 giây | |

Kết quả: host chết, treo, hay chưa bao giờ chạy đều quy về cùng một hành vi
— gõ y như khi không có tính năng. Test ở `platform/windows/tests` chạy
đúng trong tình huống không có host và kiểm cả thời gian trả về.

**Mỗi yêu cầu là một kết nối riêng, mở rồi đóng ngay.** Giữ kết nối cho cả
phiên thì nhanh hơn, nhưng named pipe có số instance hữu hạn còn DLL này
thì nằm trong **mọi** ứng dụng đang gõ. Nếu mỗi ứng dụng chiếm một instance
vĩnh viễn thì ứng dụng thứ N+1 trở đi không bao giờ nối được — im lặng,
vĩnh viễn, và phụ thuộc vào thứ tự mở ứng dụng. Đổi lại chỉ tốn vài chục
micro giây mỗi lần mở, mà người ta thì chỉ gõ vào một ứng dụng tại một thời
điểm nên gần như không bao giờ có hai yêu cầu chồng nhau.

**Phía host (`HostServer`)** giữ 4 instance pipe, mỗi cái một luồng, lặp
nối → đọc → trả lời → ngắt rồi mở lại instance mới. Bốn là để dư cho các
yêu cầu chồng nhau, không phải cho số ứng dụng đang mở.

**Thứ tự khởi động không quan trọng.** DLL được nạp vào ứng dụng từ lúc
đăng nhập, còn tiến trình nền thì người dùng mở lúc nào tuỳ; hai bên không
cần biết nhau tồn tại. DLL chỉ hỏi khi chốt từ, và mỗi lần hỏi là một lần
nối mới — nên host mở sau, tắt đi mở lại, hay bị kill rồi chạy lại đều
được nối lại ở lần chốt từ kế tiếp, chậm nhất là sau khoảng lùi bước đang
áp dụng. Không có bước bắt tay nào phải làm lại, và không ứng dụng nào phải
khởi động lại.

Gói tin sai magic/phiên bản/độ dài là ngắt kết nối, không đoán.

## Từ điển tiếng Anh và đoán dấu (`wordlist/`)

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

### Sinh phương án dấu (`engine/src/reconvert.cpp`)

`syllable_variants("duong")` trả về mọi âm tiết tiếng Việt cùng chuỗi chữ
cái gốc: dương, đường, duống, đuông…

Cách sinh **không phải** duyệt bảng vần, mà **gõ thử**. Duyệt bảng cho ra cả
những thứ bảng cho phép nhưng bộ luật gõ không bao giờ sinh ra — "gía" bên
cạnh "giá", "quýen" bên cạnh "quyến", "dưong" bên cạnh "dương". Một danh
sách chọn chứa chữ mà người dùng gõ tay không ra được là danh sách sai. Nên
ta lấy chuỗi chữ cái gốc rồi cho chính engine gõ lại nó với mọi tổ hợp phím
dấu (`a e o w`) và phím thanh (`s f r x j`), giữ lại đúng những gì engine
cho ra: 16 × 6 = 96 lượt gõ, vài chục micro giây, và hợp đồng đúng **theo
định nghĩa** chứ không theo lời hứa.

Một ngoại lệ có lý do: **đ**. Engine cố ý bỏ kiểm chính tả cho từ có đ (luật
UniKey để gõ viết tắt "đt", "đc"), nên gõ "ddoait" ra "đoait" mà không bị
chặn. Lấy đường gõ làm nguồn thì danh sách sẽ đầy chữ như vậy. Nên đ được
**suy ra**: sinh với `d` (được kiểm chính tả đầy đủ) rồi đổi `d` đầu từ
thành `đ`. Đổi được vì luật ghép phụ âm đầu chỉ phân biệt k, gi, qu — với
mọi vần, `đ` hợp lệ đúng ở chỗ `d` hợp lệ; và đ trong tiếng Việt chỉ đứng
đầu âm tiết.

Thứ tự: chữ đang có đứng đầu, rồi tới những chữ **khác nó ít nhất** (đếm số
dấu phụ và dấu thanh lệch). Không xếp theo tần suất vì engine không có dữ
liệu tần suất — và một thứ tự đoán mò còn tệ hơn một thứ tự học thuộc được.
Xếp theo tần suất là việc của mô hình n-gram trong lộ trình.

Hai tính chất được test kiểm trực tiếp:

- **Đủ** — gõ ngẫu nhiên 12.000 chuỗi phím; mọi âm tiết engine cho ra được
  đều phải nằm trong danh sách biến thể của chính nó.
- **Đúng** — mọi biến thể cùng chuỗi chữ cái gốc với đầu vào, và sinh lại
  từ bất kỳ biến thể nào cũng ra đúng cùng một họ.

### Đoán dấu cho chữ không dấu (`predict.cpp`, `ngram.cpp`)

Ba tầng, dùng đúng thứ rẻ nhất giải được việc:

1. **Không nhập nhằng** — `restore_diacritics` giao tập biến thể với bảng âm
   tiết có thật; còn đúng một thì trả lời chắc chắn. Chỉ 15,6% chuỗi không
   dấu rơi vào đây, nhưng khi rơi vào thì không cần mô hình gì.
2. **Lúc gõ** — `restore_in_context` chấm mọi phương án bằng mô hình 3-gram
   với ngữ cảnh là hai âm tiết đã chốt ngay trước, rồi lấy phương án đầu
   *nếu nó hơn phương án nhì đủ xa*.
3. **Reconversion** — `restore_sentence` chạy Viterbi trên cả đoạn, vì ở đó
   chữ hai bên đã có sẵn.

**Chỉ nhìn sang trái khi đang gõ, và đó là quyết định giao diện chứ không
phải giới hạn kỹ thuật.** Giải mã lại cả câu sau mỗi từ sẽ cho kết quả tốt
hơn nhiều (94,5% so với 52,9%), nhưng nó làm chữ ĐÃ hiện trên màn hình tự
đổi sau lưng người dùng. Cái đó khó chịu hơn hẳn đoán sai — nên chữ đã chốt
là chốt, và ta chịu mất phần chính xác đó.

**Ngưỡng tin cậy.** Đo trên 40.324 âm tiết của phần kho văn bản không dùng
để train:

| ngưỡng | có đổi chữ | đúng khi đổi | đúng chung |
|---|---|---|---|
| 0 (đoán mọi chỗ) | 89,5% | 85,2% | 85,5% |
| 1,0 | 60,3% | 90,8% | 65,4% |
| **2,0 (mặc định)** | 43,3% | **95,6%** | 52,9% |

Mặc định là 2,0 chứ không phải 0, dù 0 cho "đúng chung" cao hơn hẳn. Với
một bộ gõ, **đổi sai tệ hơn không đổi**: chữ còn không dấu thì người dùng
nhìn thấy ngay và sửa, chữ sai dấu thì trông như đã xong và lọt qua. Ngưỡng
0 sai 1 trong 7 lần nó ra tay — đủ để mất niềm tin vào cả tính năng.

### Mô hình 3-gram (`ngram.cpp`)

Từ vựng là âm tiết, nên nó nhỏ bất thường: 7.137 mục. Nhờ vậy id vừa 16
bit và một trigram chỉ tốn 10 byte thay vì 16. Mô hình dùng để đo ở trên
train từ 250 MB Wikipedia tiếng Việt: 974 nghìn bigram, 3,39 triệu trigram,
46 MB, nạp mất 119 ms, mỗi lần hỏi tốn 108 µs — thoải mái trong ngân sách
20 ms của kênh IPC.

Điểm là logP có điều kiện, lùi bậc kiểu *stupid backoff* (phạt log 0,4 mỗi
bậc). Không cần xác suất đúng chuẩn vì ta chỉ cần **thứ tự**.

Bộ nạp coi file là **dữ liệu không tin được**: kiểm magic, phiên bản, thứ
tự sắp xếp, và kiểm tràn TRƯỚC khi cấp phát theo độ dài đọc từ file. Test
thử nạp file cắt cụt ở mọi độ dài và file có số mục bịa.

Test kiểm hai tính chất của phần đoán trên từng âm tiết: thứ đoán ra phải
**có thật**, và phải đúng là chữ vừa gõ đã thêm dấu chứ không phải một chữ
khác. Vi phạm cái thứ hai nghĩa là bộ gõ tự ý thay từ của người dùng.

### Dữ liệu ngôn ngữ: nạp lúc chạy, không nằm trong mã nguồn

Khác hẳn bảng tiếng Anh, và vì lý do **giấy phép** chứ không phải kỹ thuật:

- Từ điển chính tả tiếng Việt (`hunspell-vi` của LibreOffice) là **GPL-2** —
  mục `dictionaries/vi/*` trong file copyright; dòng MPL-2.0 ở đầu file là
  cho các từ điển khác.
- Kho văn bản để train mô hình (Wikipedia tiếng Việt) là **CC BY-SA**.

Đưa dữ liệu dẫn xuất từ hai nguồn đó vào mã nguồn là ràng cả dự án vào giấy
phép của chúng, và đó là quyết định của chủ dự án chứ không phải của một
script. Nên cả hai là file rời đặt cạnh exe (`viet-syllables.txt`,
`viet-ngram.bin`), sinh bằng `tools/build_syllables.py` và
`tools/train_ngram.py`.

Thiếu bảng âm tiết thì mục menu bị làm mờ kèm lý do; có bảng mà thiếu mô
hình thì vẫn đoán được những chữ chỉ có một cách viết. Không bao giờ hỏng,
chỉ là làm được ít hơn.

Cách này còn hợp với hướng đi sau: **đổi mô hình không phải dịch lại gì.**

`SyllableList::load` và `NgramModel::load` nhận NỘI DUNG chứ không nhận
đường dẫn — đọc file là việc của tầng host (đường dẫn Windows cần API
riêng), còn tầng này portable.

### Cách lưu bảng tiếng Anh

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
