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
