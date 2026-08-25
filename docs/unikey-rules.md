# Đặc tả bộ luật gõ tương thích UniKey

Tài liệu này ghi lại **hành vi** của UniKey (nghiên cứu từ mã nguồn ukengine
của Phạm Kim Long, bản đi kèm ibus-unikey) mà engine HodionKey cài đặt lại
từ đầu. Đây là đặc tả hành vi để đối chiếu khi test và khi port — code của
HodionKey có kiến trúc và cách viết riêng, không sao chép ukengine.

Cấu hình mặc định (trùng UniKey): gõ dấu tự do **bật**, kiểm tra chính tả
**bật**, bỏ dấu kiểu cũ, không tự khôi phục phím.

## 1. Mô hình dữ liệu

- Mỗi **ô** (ký tự hiển thị) lưu: chữ gốc + dấu phụ (ă â ê ô ơ ư đ), dấu
  thanh (0–5: không, sắc, huyền, hỏi, ngã, nặng), hoa/thường, và **snapshot
  cấu trúc từ** tính đến ô đó: dạng từ (NonVn, C, V, CV, VC, CVC), id vần,
  id chuỗi phụ âm, vị trí cuối vần / cuối phụ âm đầu.
- Nhờ snapshot theo ô, **Backspace chỉ việc bỏ ô cuối** — cấu trúc của phần
  còn lại luôn đúng.
- Dấu thanh nằm trên đúng một ô và **di chuyển** khi cấu trúc đổi.

## 2. Bảng từ vựng

- **69 vần hợp lệ** (a → yêu), mỗi vần có cờ:
  - `complete`: vần trọn vẹn (eu, ie, ue, uo, uu, ye, ưo, uơi… là chưa trọn);
  - `coda_ok`: được phép có âm cuối;
  - đích khi **thêm mũ** (a→â, au→âu, ie→iê, uơ→uô…) — mũ có thể *thay*
    dấu đang có (ă+mũ→â, uơ+mũ→uô);
  - đích khi **thêm móc/trăng** (a→ă, ua→ưa, uô→uơ, uu→ưu…).
- **30 chuỗi phụ âm** (b…x, kể cả gi, gin, qu, dz, ngh); cờ `coda_ok` đánh
  dấu âm cuối hợp lệ: c ch m n ng nh p t.
- **Bảng cặp vần + âm cuối hợp lệ** (vd: ơ chỉ đi với m n p t; y chỉ với t;
  uê chỉ với c ch n nh…).
- Luật phụ âm đầu: `k` chỉ đứng trước dòng e/i/y; `gi` không đi với vần
  bắt đầu bằng i; `qu` không đi với vần bắt đầu bằng u.
- Ngoại lệ nguyên âm tiết: **quyn/quynh**, **gien/giêng** hợp lệ.

## 3. Nối ký tự (append)

- Chữ cái tiếng Việt nối vào từ và cập nhật dạng từ qua các bảng trên;
  ký tự không khớp bảng → cả từ thành **NonVn** ("đóng băng": mọi phép biến
  đổi sau đó bị bỏ qua, phím dấu thành chữ thường). Đây chính là cơ chế
  giữ từ tiếng Anh: `vietr` → vietr, `asss` → ass.
- `f, j, w` không thuộc bảng chữ tiếng Việt (ở Telex chúng là phím dấu;
  khi thành chữ thường chúng làm từ thành NonVn). Chữ số và ký hiệu không
  ngắt từ (trừ danh sách ngắt từ) cũng vậy.
- `q` + `u` và `g` + `i`: nguyên âm bị hút vào phụ âm đầu (qu, gi).
  `gi`/`gin` thuần phụ âm vẫn mang được dấu thanh trên i: `gif`→gì,
  `ginf`→gìn; khi có nguyên âm theo sau, thanh chuyển sang vần: `gifa`→già.
- **ưo/uơ tự hoàn thành thành ươ** khi có âm cuối hợp lệ: thuơ+ng→thương.
- Khi thêm âm cuối, thanh di chuyển theo luật vị trí (hòa+n→hoàn, tóa+n→toán).
- Tắt kiểm tra chính tả (hoặc từ bắt đầu bằng đ): ký tự làm hỏng từ sẽ
  **mở âm tiết mới** thay vì đóng băng.

## 4. Vị trí dấu thanh

Trong vần (đã xác định sau phụ âm đầu, kể cả qu/gi):

1. Vần 1 nguyên âm → nguyên âm đó.
2. Có nguyên âm mang mũ (â ê ô) → nguyên âm đó.
3. Có nguyên âm mang móc/trăng → nguyên âm mang dấu đầu tiên,
   **ngoại lệ ươ / ươi / ươu → ơ**.
4. Vần 3 nguyên âm → nguyên âm giữa.
5. Kiểu mới (modern) và vần oa/oe/uy → nguyên âm sau.
6. Còn lại: **chưa có âm cuối → nguyên âm đầu** (hòa, thúy, của);
   **có âm cuối → nguyên âm sau** (hoàn, thuýt).

## 5. Mũ (Telex a/e/o, VNI 6)

- Áp lên vần hiện tại (xuyên qua âm cuối — gõ dấu tự do): `viete`→viêt.
- Telex chỉ áp khi đúng nguyên âm đích (phím `a` không đổi e/o).
- Kết quả phải là âm tiết hợp lệ (kiểm tra phụ âm đầu + vần + âm cuối).
- **ưo/ươ/ưoi/ươi + mũ → uô/uôi** (mũ thắng móc cả cụm).
- Đã có mũ đúng đích → gõ lặp **hủy mũ** và phím thành chữ thường
  (aa→â, aaa→aa; xooong→xoong).

## 6. Móc/trăng (Telex w, VNI 7/8)

- VNI 7 chỉ tạo ơ/ư; VNI 8 chỉ tạo ă; Telex w tạo cả ba.
- Vần họ uo — (u|ư)(o|ô|ơ):
  - `uo|uô` + móc → **ươ** (cả cụm): nuowc→nươc, tuoi+w→tươi;
  - ngoại lệ: sau phụ âm đầu **th**, ở cuối từ → **uơ**: thuow→thuơ
    (để gõ "thuở"); có âm cuối thì vẫn ra ươ: thuongw→thương;
  - `ươ` + móc → hủy về uo, phím thành chữ thường (uoww→uow).
- Vần khác: tra đích móc trong bảng vần (ua→ưa: muaw→mưa; ui→ưi; uu→ưu;
  ô→ơ…), kiểm tra hợp lệ, gõ lặp thì hủy (aww→aw, u77→u7, a88→a8).

## 7. Phím w của Telex và [ ] { }

- `w`: thử móc trước; không áp được thì thành **ư** nếu nối được hợp lệ
  (w→ư, tw→tư, wa→ưa, wow→ươ); không thì là chữ w thường.
- `ww` khi ư sinh từ w đơn → trả lại "w". (ư sinh từ `uw` thì w thứ hai
  hủy móc: uww→uw.)
- Telex đầy đủ: `[`→ơ, `]`→ư, `{`→Ơ, `}`→Ư, cùng luật nối/hủy; khi không
  áp được thì các phím này là ký tự **ngắt từ** như bình thường.

## 8. đ (Telex dd, VNI 9)

- Biến `d` của **phụ âm đầu** thành đ, kể cả gõ ở cuối từ (dund→đun,
  did→đi — hành vi UniKey khi bật gõ dấu tự do).
- Từ bắt đầu bằng đ được miễn kiểm tra chính tả (phục vụ viết tắt).
- Trong chuỗi NonVn, `d` đứng sau ký tự không phải nguyên âm vẫn thành đ
  (viết tắt: hdd→hđ).
- Gõ lặp hủy: ddd→dd, d99→d9.

## 9. Dấu thanh (Telex z s f r x j, VNI 0–5)

- Cần vần (hoặc gi/gin); từ NonVn → phím thành chữ thường.
- **Âm cuối tắc c/ch/p/t chỉ nhận sắc/nặng** — huyền/hỏi/ngã thành chữ
  thường (vietr→vietr).
- Thanh mới thay thanh cũ (asf→à); gõ lặp cùng thanh → bỏ thanh + chữ
  thường (ass→as); z/0 bỏ thanh (asz→a), không có thanh thì là chữ thường.

## 10. Backspace, ngắt từ, khôi phục

- Backspace xóa **một ký tự hiển thị**; thanh trên ký tự bị xóa mất theo,
  thanh của phần còn lại lùi về đúng vị trí (hoàn⌫→hòa; hoà⌫→ho).
- Ký tự ngắt từ: khoảng trắng, điều khiển và
  `, ; : . " ' ! ? < > = + - * / \ _ @ # $ % & ( ) { } [ ] |`
  (chú ý: `~ \` ^` và chữ số KHÔNG ngắt từ — chúng nằm trong từ và làm từ
  thành NonVn).
- Tùy chọn **khôi phục từ không phải tiếng Việt** (mặc định tắt): khi ngắt
  từ, nếu từ không hợp lệ (dạng NonVn, vần chưa trọn, tổ hợp sai, thanh sai
  với âm cuối tắc) và đã có phím bị biến đổi → trả lại đúng chuỗi phím thô
  (boxing → bõing → "boxing " khi bật).
- Nhật ký phím thô chỉ đáng tin **khi từ chưa bị Backspace**: một ký tự hiển
  thị có thể do nhiều phím tạo ra (vieejt = 6 phím → 4 ký tự), nên sau khi
  xóa thì không dựng lại được chuỗi đã gõ. Khi đó `raw()` trả về chính chữ
  đang hiển thị (Esc chỉ kết thúc composition) và khôi phục tự động không
  chạy — thà không khôi phục còn hơn khôi phục ra chuỗi cụt. Xóa hết từ thì
  nhật ký sạch và dùng lại được cho từ kế tiếp.

## Khác biệt có chủ đích so với UniKey

- Chữ số/ký hiệu ở **đầu từ** không mở composition (UniKey không có
  composition nên "1as" bị đóng băng thành "1as"; HodionKey cho số đi thẳng
  và từ mới bắt đầu ở chữ cái — khác biệt chỉ lộ ra trong ca hiếm này).
- Chưa hỗ trợ: VIQR, Microsoft layout, gõ tắt (macro), bảng mã ngoài
  Unicode dựng sẵn — xem lộ trình.
