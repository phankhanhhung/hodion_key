# Kiến trúc HodionKey

## Nguyên tắc thiết kế

1. **Engine tách rời tuyệt đối khỏi hệ điều hành.** `engine/` chỉ dùng C++17
   chuẩn — không `windows.h`, không COM, không giả định gì về host. Toàn bộ
   nghiệp vụ tiếng Việt (Telex/VNI, dấu phụ, dấu thanh, luật hủy, backspace)
   nằm ở đây và được phủ unit test chạy trên mọi nền tảng.
2. **Tầng platform càng mỏng càng tốt.** `platform/windows/` chỉ làm ba việc:
   dịch phím → ký tự, chuyển kết quả engine → thao tác composition của TSF,
   và đăng ký COM/profile. Không một luật tiếng Việt nào nằm ở tầng này.
3. **Không trạng thái ngầm.** Engine giữ đúng một thứ: chuỗi phím thô của từ
   đang gõ. Chuỗi hiển thị luôn được tái dựng từ đầu sau mỗi phím — mọi luật
   hủy/hoàn tác vì thế đúng theo cấu trúc, không cần vá từng ca đặc biệt.

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

Mỗi phím chữ được nối vào `raw_` (chuỗi phím thô). Sau đó:

1. **Composer** (`src/composer.cpp`) chạy tuần tự qua `raw_`, dựng
   `ComposeState = {chars[], tone}` trong đó mỗi `VChar` mang chữ gốc +
   dấu phụ (ă/â/ê/ô/ơ/ư/đ) + hoa/thường. Các luật gõ lặp để hủy
   (`aa→â→aa`, `as→á→as`, `dd→đ→dd`, `w→ư→w`…) phát sinh tự nhiên từ
   cách xử lý tuần tự này.
2. **Đặt dấu thanh** (`tone_position`): xác định phụ âm đầu (kể cả `qu`,
   `gi`), cụm nguyên âm, rồi chọn vị trí theo thứ tự ưu tiên:
   nguyên âm có dấu phụ (ê, ơ, â… — cặp `ươ` lấy `ơ`) → một nguyên âm →
   hai nguyên âm có âm cuối lấy nguyên âm sau → `oa/oe/uy` theo tùy chọn
   kiểu cũ/kiểu mới → mặc định nguyên âm đầu → ba nguyên âm lấy nguyên âm giữa.
3. **Chartable** (`src/chartable.cpp`) tra (chữ gốc, dấu phụ, thanh) →
   codepoint Unicode dựng sẵn (NFC) — dạng chuẩn mà mọi ứng dụng Windows
   hiển thị đúng.

Backspace = pop một phím thô rồi tái dựng — hoàn tác đúng theo phím gõ.

### Vì sao tái dựng từ đầu thay vì biến đổi tăng dần?

Buffer một "từ" tối đa ~40 phím nên chi phí O(n²/phím) không đáng kể, đổi
lại: không bao giờ lệch trạng thái, backspace/hủy dấu đúng tuyệt đối, và
test chỉ cần khẳng định `keys → text` thuần túy.

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

- Kiểm tra chính tả âm tiết (bảng phụ âm đầu/vần/âm cuối hợp lệ) với tùy
  chọn khôi phục phím thô cho từ không phải tiếng Việt.
- Telex mở rộng (`[`/`]` → ơ/ư), gõ tắt do người dùng định nghĩa.
- App cấu hình (thay cho sửa registry tay) + phím tắt bật/tắt tiếng Việt
  (compartment input mode).
- Trang trạng thái trên language bar / systray.
