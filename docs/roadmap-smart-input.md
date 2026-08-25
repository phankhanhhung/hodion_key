# Lộ trình "gõ thông minh" — các quyết định thiết kế

Tài liệu này chốt lại phần **bàn bạc thiết kế** cho các tính năng chưa
làm: nhận diện tiếng Anh khi gõ văn bản trộn, tự thêm dấu cho chữ Việt
không dấu, sửa từ đã chốt, và tự sửa lỗi chính tả.

Mục 1–5 và 6a (gõ trộn Việt–Anh, reconversion, mô hình n-gram thêm dấu,
phím xoay vòng) **đã làm xong** — mục 7 ghi rõ mục nào và những gì rút ra
khi làm. Phần còn lại (chỉ báo độ tin cậy, bám lại từ, mô hình neural) thì
chưa có dòng code nào.

Mục đích của tài liệu là để khi bắt tay vào làm thì không phải cãi lại từ
đầu, và để những quyết định "không làm" cũng có lý do được ghi lại.

Trạng thái hiện tại của dự án (đã xong, đã có test) nằm ở
[ARCHITECTURE.md](ARCHITECTURE.md) và [unikey-rules.md](unikey-rules.md).

---

## 0. Nguyên tắc xuyên suốt

Ba ràng buộc chi phối mọi quyết định bên dưới. Vi phạm cái nào cũng biến
bộ gõ tốt thành bộ gõ khó chịu:

**Đường phím phải tất định và không bao giờ chờ.** Từ lúc `WM_KEYDOWN`
tới lúc chữ hiện lên là vài mili giây, chạy trên thread UI của ứng dụng
đang gõ. Không I/O, không mạng, không khoá, không suy luận mô hình đồng
bộ. Engine hiện tại giữ đúng tính chất này và **không được** đánh mất.

**Không tự ý đổi thứ người dùng đã gõ.** Bộ gõ đoán sai thì người ta sửa
mất một giây. Bộ gõ đổi chữ **sau lưng** người ta thì mất niềm tin, và
mất niềm tin thì họ tắt tính năng vĩnh viễn. Sai sót im lặng đắt hơn sai
sót nhìn thấy được.

**Mô hình nằm ngoài engine.** Bất kỳ thứ gì có trọng số, có từ điển, có
xác suất đều là **tầng riêng, tuỳ chọn, tắt được**. Engine lõi vẫn phải
build và chạy đúng khi không có mô hình nào cả — đó là điều kiện để port
sang macOS/Linux mà không kéo theo một đống phụ thuộc.

---

## 1. Gõ trộn Việt–Anh

### Vấn đề

Gõ `"cai deadline nay"` ra `"cai deadlinee này"` — `ee` biến thành `ê`.
Người dùng phải bấm phím chuyển hai lần cho mỗi từ tiếng Anh. Đây là
phiền toái số một của mọi bộ gõ tiếng Việt.

### Ba tầng, làm theo thứ tự

**Tầng 1 — Input scope (rẻ nhất, hiệu quả ngay).**
TSF cho phép hỏi ngữ cảnh nhập liệu qua `ITfInputScope`
(`ITfContext` → `QueryInterface(IID_ITfInputScope)` → `GetInputScopes`).
Ô nhập URL, email, mật khẩu, tên file, số điện thoại đều khai báo scope
của nó. Ở những ô đó, **tự động tắt tiếng Việt** — không có ai gõ tiếng
Việt vào thanh địa chỉ cả.

Chi phí: khoảng 50 dòng trong `TextService.cpp`, không có mô hình nào,
không có rủi ro đoán sai. Đây là thứ nên làm trước tiên.

Các scope cần chặn: `IS_URL`, `IS_EMAIL_USERNAME`, `IS_EMAIL_SMTPEMAILADDRESS`,
`IS_PASSWORD`, `IS_FILE_FULLFILEPATH`, `IS_FILE_FILENAME`,
`IS_TELEPHONE_*`, `IS_DIGITS`, `IS_NUMBER`.

**Tầng 2 — Quyết định theo từ, lúc chốt (đã có sẵn một nửa).**
Engine đã có `is_non_vn()` và tuỳ chọn `RestoreNonVn`: từ nào không phải
âm tiết tiếng Việt hợp lệ thì trả lại chuỗi phím thô lúc commit. Bổ sung
một **từ điển tiếng Anh nhỏ** (top 20–30k từ, bloom filter, ~200KB) để
quyết định chắc tay hơn:

| Trạng thái từ                      | Xử lý                    |
|------------------------------------|--------------------------|
| Âm tiết Việt hợp lệ                | giữ nguyên dạng đã biến đổi |
| Không phải âm tiết Việt, có trong từ điển Anh | trả lại phím thô |
| Không phải cả hai (tên riêng, viết tắt) | trả lại phím thô (an toàn hơn) |

Điểm hay: quyết định xảy ra **lúc chốt từ**, tức là đã có đủ thông tin,
và vẫn tất định — chỉ là tra bảng.

**Tầng 3 — Phím thoát thủ công (đã có).**
`Ctrl+Space`. Luôn phải giữ, vì không tầng nào đúng 100%. Nên bổ sung
một phím "huỷ biến đổi từ hiện tại" (ví dụ `Ctrl+Backspace` khi đang
composing) — trả về chuỗi phím thô mà không tắt chế độ tiếng Việt.

### Cái không nên làm

Đoán ngôn ngữ **theo từng phím** rồi bật/tắt biến đổi giữa chừng. Chữ sẽ
nhảy qua nhảy lại trong lúc gõ. Quyết định phải xảy ra ở ranh giới từ,
không phải giữa từ.

---

## 2. "Nhúng AI vào có quá lố không?"

Trả lời ngắn: **quá lố nếu hiểu AI là mô hình lớn chạy trong đường phím.
Không quá lố nếu đó là một mô hình thống kê nhỏ chạy ngoài đường phím.**

Phân biệt cho rõ:

| | Chấp nhận được | Quá lố |
|---|---|---|
| Kích thước | 10–50MB | 500MB+ |
| Độ trễ | vài chục µs | 50ms+ |
| Vị trí | tiến trình riêng / tra bảng | trong `OnKeyDown` |
| Khi hỏng | bộ gõ chạy bình thường | bộ gõ đứng |

Một bộ gõ bị treo 200ms mỗi phím là bộ gõ hỏng, dù nó đoán đúng tới đâu.
Nên ràng buộc kỹ thuật là: **mô hình chạy trong tiến trình riêng, giao
tiếp qua pipe, có timeout cứng, hết giờ thì bỏ qua kết quả và gõ như
bình thường.** Không có đường nào để mô hình làm chậm việc gõ.

---

## 3. Tự thêm dấu cho chữ không dấu

### Bài toán

`"toi di an com"` → `"tôi đi ăn cơm"`. Đây là bài toán **gán nhãn chuỗi
có ràng buộc**, không phải sinh văn bản.

### Điều làm bài toán dễ hơn nhiều so với cảm giác

Chuỗi không dấu đã tự cắt không gian rất mạnh. Engine **đã có sẵn** bảng
69 vần hợp lệ và bảng ghép phụ âm–vần — dùng chính nó để sinh tập ứng
viên cho mỗi âm tiết:

- `nghiem` → chỉ một dạng tồn tại thật → không cần mô hình
- `nghieng` → 5 dạng hợp lệ về cấu trúc, ít hơn nhiều sau khi lọc từ điển
- `dduong` → `đường / đướng / đưởng / đưỡng / đượng`, thực tế còn 2

Ước lượng thô: **40–50% âm tiết chỉ có đúng một dạng** hợp lệ và tồn tại.
Mô hình chỉ phải lo nửa còn lại.

Tập ứng viên này còn là **thuộc tính an toàn**: mô hình không bao giờ
được sinh ra thứ nằm ngoài tập đó. Nghĩa là nó không thể đẻ ra chữ không
phải tiếng Việt, dù có sai tới đâu.

### Chọn mô hình

| Phương án | Kích thước | Độ trễ | Độ chính xác | Kết luận |
|---|---|---|---|---|
| **4-gram âm tiết + Viterbi** | 20–50MB | µs | ~94–96% | **làm cái này trước** |
| BiLSTM gán nhãn theo âm tiết | 5–15MB | 5–20ms | ~97% | bước hai |
| Transformer nhỏ | 30–80MB | 20–50ms | ~97–98% | lợi ích biên |
| PhoBERT / XLM-R fine-tune | 500MB+ | 100ms+ | ~98–99% | quá lố cho bàn phím |

n-gram thắng ở đây không phải vì chính xác nhất mà vì **tỷ lệ giá trị
trên chi phí** áp đảo: không GPU, không runtime ML, không phụ thuộc, train
trong vài giờ trên CPU, debug được bằng mắt (mở ra xem xác suất là hiểu
tại sao nó đoán vậy). Khoảng cách 94% → 97% không đáng đánh đổi lấy toàn
bộ chuỗi phụ thuộc của một runtime neural, ít nhất là ở phiên bản đầu.

Dữ liệu train: kho văn bản tiếng Việt có dấu (báo chí, Wikipedia tiếng
Việt, sách). Bỏ dấu đi là có ngay cặp (input, output) — **không cần gán
nhãn tay**, đây là điểm khiến bài toán này rẻ bất thường.

### Ngữ cảnh: từ đoán là hàm của cả câu, không chỉ từ trước

Đúng là mỗi từ phụ thuộc từ đứng trước, nhưng **cả hai chiều đều cần**:

```
buoi toi          → buổi tối          (quyết bởi từ TRƯỚC)
toi qua duong     → tôi qua đường
toi qua troi mua  → tối qua trời mưa  (quyết bởi từ SAU)
```

Với n-gram thì xác suất chỉ nhìn ngược `P(w_i | w_{i-2}, w_{i-1})`, nhưng
**decode không tham**: Viterbi tối ưu tổng `Σ log P` của cả chuỗi, nên từ
đứng sau vẫn kéo ngược được lựa chọn của từ đứng trước qua đường đi. Ngữ
cảnh phải chảy vào gián tiếp, hiệu quả tới khoảng 2–3 âm tiết. Đó là lý
do 4-gram là đủ và 5-gram gần như không thêm gì mà lại thưa hẳn dữ liệu.

Với BiLSTM/Transformer thì ngữ cảnh hai chiều là tường minh, không phải
hệ quả của thuật toán decode.

### Ràng buộc thật: lúc gõ thì chưa có ngữ cảnh phải

Đây mới là chỗ khó, và nó là vấn đề **thiết kế tương tác** chứ không phải
vấn đề chọn mô hình:

```
gõ:  toi           → đoán "tôi"
gõ:  toi qua       → vẫn "tôi qua"
gõ:  toi qua troi  → giờ mới biết là "tối qua trời"
```

Nếu cứ decode lại sau mỗi âm tiết thì chữ đã hiện trên màn hình sẽ tự
đổi. Vi phạm thẳng nguyên tắc số 2.

Ba cơ chế xử lý:

1. **Vùng đóng băng.** Chỉ decode lại N âm tiết cuối (N ≈ 3–5). Lùi quá N
   là chốt cứng. Về mặt TSF: phần chưa đóng băng nằm trong composition
   (gạch chân), đóng băng rồi thì commit.
2. **Chốt theo độ ổn định, không theo khoảng cách.** Giữ đường đi Viterbi
   giữa các lần decode; âm tiết nào không đổi nhãn qua 2 lần liên tiếp thì
   chốt. Thực tế phần lớn ổn định ngay sau 1 âm tiết kế tiếp.
3. **Người dùng sửa tay thì ghim.** Từ đã sửa trở thành ràng buộc cứng
   trong Viterbi — decode sau không được đổi nó, và còn phải coi nó là
   bằng chứng cho các từ xung quanh. Sửa một chỗ tự kéo theo cả cụm là
   cảm giác rất tốt.

Chi phí decode lại với n-gram gần như bằng 0 (vài chục trạng thái, cửa sổ
5 từ). Với mô hình neural thì chạy lại encoder mỗi phím là quá đắt — phải
cache trạng thái hoặc chỉ chạy khi kết câu.

---

## 4. Có nên làm UI picker chọn phương án?

**Không, ít nhất là không trong luồng gõ.** Ba lý do:

**Tỷ lệ nền khác hẳn tiếng Nhật/Trung.** Ở IME Nhật/Trung gần như *mọi*
từ đều nhập nhằng, nên picker là tương tác cốt lõi và người dùng chấp
nhận. Ở tiếng Việt có ngữ cảnh thì đã đúng ~95%, và sai thì nhìn phát
biết ngay. Bắt xác nhận 100% số từ để xử lý 5% là đánh thuế lên toàn bộ.

**Picker giết chính giá trị của tính năng.** Người ta gõ không dấu **để
nhanh**. Thêm một lần bấm mỗi từ là chậm hơn cả gõ Telex bình thường —
lúc đó thà gõ Telex.

**`ITfContextView::GetTextExt` không đáng tin.** Định vị con trỏ để đặt
cửa sổ picker hoạt động tốt ở Word/Notepad nhưng lệch hoặc trả về rỗng ở
rất nhiều ứng dụng Electron, Java, game, remote desktop. Cửa sổ picker
hiện sai chỗ tệ hơn là không có picker.

### Làm thay bằng gì

- **Áp phương án tốt nhất ngay, không hỏi.**
- **Một phím xoay vòng** (ví dụ `Ctrl+.`) để đổi sang phương án kế tiếp.
  Độ chính xác top-2 cao hơn top-1 đáng kể, nên thường bấm một lần là
  xong — rẻ hơn nhiều so với mở picker.
- **Đưa cả chuỗi không dấu gốc vào vòng xoay**, để lúc nào cũng thoát
  được về đúng thứ đã gõ.
- **Một display attribute thứ hai** (gạch chân nét đứt / màu nhạt) cho từ
  mà mô hình không chắc. Được khoảng 80% giá trị của picker với 5% chi phí
  — người dùng liếc là biết chỗ nào cần kiểm.

Picker để dành cho **chế độ reconversion tường minh** (bôi đen một đoạn
rồi bấm phím yêu cầu chuyển) — ở đó người dùng đã chủ động dừng lại, nên
một cửa sổ chọn là hợp lý.

---

## 5. Sửa từ đã chốt, hay bắt gõ lại cả từ?

**Cho sửa.** Bắt gõ lại là thiết kế thù địch, và TSF có sẵn cơ chế cho
việc này.

Hai đường:

**Reconversion (`ITfFnReconversion`).** Bôi đen chữ đã có trong tài liệu,
gọi hàm reconversion, text service trả về danh sách phương án. Đây là
đường chuẩn của TSF, ứng dụng nào hỗ trợ tử tế thì chạy đúng. Dùng cho
"thêm dấu cho đoạn đã gõ" và cho sửa từ đoán sai.

**Bám lại từ khi con trỏ đứng cuối từ.** Người dùng bấm Backspace vào
giữa một từ vừa chốt → engine đọc ngược text quanh con trỏ, dựng lại
trạng thái âm tiết, rồi tiếp tục như đang composing. Trải nghiệm rất mượt
khi chạy được.

Cảnh báo: cách thứ hai phải **đọc văn bản của tài liệu** qua
`ITfRange::GetText`, mà không phải ứng dụng nào cũng trả về đúng — nhiều
ứng dụng Electron/Java chỉ hỗ trợ TSF ở mức tối thiểu. Nên coi nó là
tối ưu hoá có điều kiện: thử đọc, đọc không được thì im lặng bỏ qua và
hành xử như hiện tại. **Không bao giờ** để việc đọc thất bại làm hỏng
văn bản.

---

## 6. Có nên tự sửa lỗi chính tả?

**Không, hoặc rất dè dặt.** Cần phân biệt hai thứ hay bị gộp làm một:

**Kiểm tra chính tả cấu trúc âm tiết** — engine đã có, và nó **an toàn**
vì nó tất định, dựa trên quy tắc cấu tạo âm tiết tiếng Việt, và nó chỉ
*ngừng biến đổi* chứ không *đổi* thứ người dùng gõ (`vietr` giữ nguyên,
không tự thành `viết`).

**Tự sửa theo từ điển** — `"cam ơn"` → `"cảm ơn"`. Cái này nguy hiểm:

- Tiếng Việt có rất nhiều âm tiết hợp lệ mà không nằm trong từ điển: tên
  riêng, tên địa danh, từ mượn, tiếng lóng, thuật ngữ. Tự sửa tên người
  là lỗi không thể tha thứ.
- Sửa sai xảy ra **im lặng**, người dùng chỉ phát hiện sau khi đã gửi.
- Ranh giới giữa "gõ nhầm" và "cố tình" thì máy không biết.

Nếu vẫn muốn làm, giới hạn cứng:
- Chỉ với **từ ghép hai âm tiết** mà cả cụm sai nhưng có đúng một sửa đổi
  ở khoảng cách chỉnh sửa 1, và cụm đúng đó có tần suất cao hơn vài bậc.
- **Đánh dấu, không sửa** ở mặc định. Bật tự sửa là lựa chọn tường minh.
- Luôn có Undo trong một phím.

---

## 7. Thứ tự triển khai đề xuất

| # | Việc | Chi phí | Rủi ro | Giá trị | |
|---|---|---|---|---|---|
| 1 | Input scope → tự tắt ở ô URL/email/mật khẩu | thấp | ~0 | cao | **xong** |
| 2 | Phím huỷ biến đổi từ hiện tại | thấp | ~0 | vừa | **xong** |
| 3 | Từ điển Anh cho quyết định lúc chốt | vừa | thấp | cao | **xong** |
| 4 | Reconversion (`ITfFnReconversion`) | vừa | vừa | vừa | **xong** |
| 5 | n-gram + Viterbi thêm dấu, tiến trình riêng | cao | vừa | cao | **xong** |
| 6a | Phím xoay vòng qua các phương án dấu | vừa | thấp | cao | **xong** |
| 6b | Display attribute cho từ mô hình không chắc | vừa | thấp | vừa | |
| 7 | Bám lại từ khi con trỏ đứng cuối từ | vừa | cao | vừa | |
| 8 | Mô hình neural thay n-gram | cao | vừa | thấp | |
| — | Tự sửa chính tả theo từ điển | vừa | **cao** | **âm** | không làm |
| — | Picker trong luồng gõ | cao | cao | **âm** | không làm |

Mục 1–5 và 6a đã làm xong (chi tiết ở [ARCHITECTURE.md](ARCHITECTURE.md)).
Mục 6a giữ đúng ba điều đã bàn ở mục 6: áp phương án tốt nhất không hỏi,
một phím xoay vòng để đổi, và chuỗi không dấu gốc luôn có trong vòng —
phần còn thiếu chỉ là chỉ báo trực quan cho từ mô hình không chắc (6b).

Về mục 5, đo được sau khi làm xong (40.324 âm tiết ngoài dữ liệu train):
Viterbi cả câu **94,5%** — đúng khoảng dự đoán 94–96% ở mục 3 bên trên. Còn
lúc đang gõ, vì chỉ nhìn được sang trái, con số là **52,9%** ở ngưỡng mặc
định, và **95,6%** trong số những lần nó thật sự ra tay. Chênh lệch giữa
94,5% và 52,9% chính là cái giá của ràng buộc "không đổi chữ sau lưng người
dùng" đã bàn ở mục 3 — nó có thật và nó lớn.

Ba điều rút ra khi làm, đáng ghi lại vì chúng đổi cách nghĩ về mục 5–6:

**Từ điển tiếng Anh không giải được ca khó nhất, và đó là kết luận đúng.**
889 trong 18.356 từ tiếng Anh bị Telex biến thành một âm tiết tiếng Việt
*hợp lệ và phổ biến* — `bans`→bán, `bust`→bút, `test`→tét. Ở đó không có
bằng chứng nào phân xử được, nên bộ gõ phải im lặng giữ nguyên chữ tiếng
Việt. Chính vì vậy mục 2 (phím huỷ biến đổi) không phải phụ kiện: nó là
đường thoát cho đúng cái 5% mà không dữ liệu tĩnh nào cứu được. Ngữ cảnh
câu — tức mục 5 — mới là thứ giải được lớp này.

**Danh sách phương án phải sinh bằng cách gõ thử, không phải duyệt bảng.**
Duyệt bảng vần cho ra cả những chữ bộ luật gõ không bao giờ sinh ("gía",
"quýen", "dưong"). Bài học chung cho mục 6: mọi thứ đề xuất cho người dùng
phải là thứ họ gõ tay cũng ra được, nếu không họ sẽ mất niềm tin vào cả
tính năng.

**Chưa có mô hình thì đừng giả vờ có.** Danh sách phương án xếp theo *gần
với chữ đang có nhất*, không phải theo "hay gặp" — vì engine không có dữ
liệu tần suất. Một thứ tự đoán mò tệ hơn một thứ tự học thuộc được. Xếp
theo tần suất là việc của mô hình n-gram ở mục 5, và khi có nó thì chỗ cắm
vào đã sẵn: chỉ đổi hàm so sánh trong `syllable_variants`.

---

## 8. Trạng thái hiện tại để đối chiếu

Những thứ ở trên xây trên nền đã có:

- Engine C++17 thuần, tách rời OS, luật Telex/VNI tương thích hành vi
  UniKey (viết lại từ đầu, không sao chép code).
- Bảng 69 vần và bảng ghép phụ âm–vần — **chính là thứ sẽ dùng để sinh
  tập ứng viên** cho việc thêm dấu ở mục 3.
- `is_non_vn()` và `RestoreNonVn` — nền cho luật cấu trúc lúc chốt từ.
- Từ điển tiếng Anh 17.467 từ (`wordlist/`) và bộ sinh phương án dấu
  (`engine/src/reconvert.cpp`) — **chỗ cắm sẵn** cho mô hình ở mục 5.
- TSF text service với composition, display attribute, phím chuyển đồng
  bộ với chỉ báo IME của Windows, cấu hình áp dụng tức thì qua registry.
- Chất lượng test: phủ 98% dòng / 81% nhánh, mutation score 86,9%, sạch
  dưới ASan+UBSan qua hơn 11 triệu lượt kiểm tra.

Rủi ro còn lại: tầng TSF **đã** chạy thử được trên Windows thật, nhưng mới
là một lần thử. Bản 32-bit chưa từng được thực thi ở đâu, và phần
reconversion (số học trên `ITfRange`) chỉ kiểm được bằng cách chạy trong
một ứng dụng thật — ở đây mới chỉ tách được phần logic thuần ra để test,
còn lại dựa vào ba lớp đọc-lại-trước-khi-ghi.
