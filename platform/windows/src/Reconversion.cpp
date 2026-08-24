// Sửa dấu cho chữ ĐÃ chốt, không phải gõ lại cả từ (ITfFnReconversion).
//
// Ứng dụng đưa cho ta một range — đoạn người dùng bôi đen, hoặc một range
// rỗng ở vị trí con trỏ — và hỏi "chuyển được không". Ta trả về đúng đoạn
// sẽ đổi cùng danh sách phương án lấy từ engine (hodion::syllable_variants),
// rồi ghi lại khi ứng dụng báo người dùng đã chọn.
//
// Nguyên tắc xuyên suốt file này: **đọc lại trước khi ghi**. Range do ứng
// dụng cấp, còn ta thì tính toán trên một bản chụp văn bản; nếu văn bản đã
// đổi kể từ lúc đó thì thà không làm gì còn hơn ghi đè nhầm chỗ.
#include <cwchar>
#include <string>
#include <vector>

#include "TextService.h"
#include "WordScan.h"
#include "hodion/reconvert.h"
#include "hodion/utf.h"

namespace {

// Âm tiết tiếng Việt dài nhất là 8 ký tự ("nghiêng"), lấy dư một chút để
// còn nhận ra ranh giới từ.
constexpr size_t kMaxWordChars = 8;
constexpr LONG kLookAround = 16;
constexpr ULONG kMaxCandidates = 32;

std::u32string ToU32(const std::wstring& s) {
  const std::u16string u16(s.begin(), s.end());
  return hodion::utf::from_utf16(u16);
}

std::wstring FromU32(const std::u32string& s) {
  const std::u16string u16 = hodion::utf::to_utf16(s);
  return std::wstring(u16.begin(), u16.end());
}

// Ký tự có thuộc về một từ tiếng Việt không? Dùng để dò ranh giới từ khi
// người dùng chỉ đặt con trỏ mà không bôi đen.
bool IsWordChar(wchar_t c) {
  if ((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z')) return true;
  if (c < 0x80) return false;
  // Chữ tiếng Việt dựng sẵn: đủ để hỏi engine xem nó có bỏ dấu được không.
  // Chữ tiếng Việt dựng sẵn thì bỏ dấu ra sẽ khác chính nó — kể cả đ (→ d).
  const std::u32string one(1, static_cast<char32_t>(c));
  return hodion::strip_diacritics(one) != one;
}

HRESULT ReadRange(TfEditCookie ec, ITfRange* range, std::wstring* out) {
  out->clear();
  WCHAR buf[kLookAround + 1];
  ULONG cch = 0;
  const HRESULT hr = range->GetText(ec, 0, buf, kLookAround, &cch);
  if (FAILED(hr)) return hr;
  out->assign(buf, cch);
  return S_OK;
}

}  // namespace

// Tìm đúng đoạn sẽ đổi. Bôi đen sẵn thì dùng luôn đoạn đó; con trỏ trống
// thì mở rộng ra từ đang đứng trong đó.
HRESULT CTextService::FindReconvertRange(TfEditCookie ec, ITfRange* pRange,
                                         ITfRange** ppWord,
                                         std::wstring* outText) {
  *ppWord = nullptr;
  outText->clear();

  com_ptr<ITfRange> selected;
  HRESULT hr = pRange->Clone(selected.put());
  if (FAILED(hr)) return hr;

  std::wstring text;
  if (FAILED(ReadRange(ec, selected.get(), &text))) return E_FAIL;

  if (!text.empty()) {
    // Chặn dài: bôi đen một đoạn dài rồi thay bằng một âm tiết là XÓA mất
    // văn bản. kLookAround > kMaxWordChars nên đoạn nào dài hơn một âm tiết
    // cũng đọc ra dài hơn và bị loại ở đây — kể cả đoạn dài hơn bộ đệm.
    if (text.size() > kMaxWordChars) return E_FAIL;
    *outText = text;
    *ppWord = selected.detach();
    return S_OK;
  }

  // Range rỗng: đọc quanh con trỏ rồi cắt theo ranh giới từ.
  com_ptr<ITfRange> left;
  if (FAILED(pRange->Clone(left.put()))) return E_FAIL;
  left->Collapse(ec, TF_ANCHOR_START);
  LONG moved = 0;
  if (FAILED(left->ShiftStart(ec, -kLookAround, &moved, nullptr))) {
    return E_FAIL;
  }
  std::wstring before;
  if (FAILED(ReadRange(ec, left.get(), &before))) return E_FAIL;

  com_ptr<ITfRange> right;
  if (FAILED(pRange->Clone(right.put()))) return E_FAIL;
  right->Collapse(ec, TF_ANCHOR_START);
  LONG movedEnd = 0;
  if (FAILED(right->ShiftEnd(ec, kLookAround, &movedEnd, nullptr))) {
    return E_FAIL;
  }
  std::wstring after;
  if (FAILED(ReadRange(ec, right.get(), &after))) return E_FAIL;

  size_t start = 0, end = 0;
  HodionWordAround(before, after, &start, &end);
  if (start == before.size() && end == 0) return E_FAIL;  // không có từ nào

  com_ptr<ITfRange> word;
  if (FAILED(left->Clone(word.put()))) return E_FAIL;
  LONG shifted = 0;
  if (FAILED(word->ShiftStart(ec, static_cast<LONG>(start), &shifted,
                              nullptr)) ||
      shifted != static_cast<LONG>(start)) {
    return E_FAIL;
  }
  if (FAILED(word->ShiftEnd(ec, static_cast<LONG>(end), &shifted, nullptr)) ||
      shifted != static_cast<LONG>(end)) {
    return E_FAIL;
  }

  // Đọc lại đoạn vừa dựng: nếu nó không đúng bằng từ ta định lấy thì phép
  // dịch range đã sai ở đâu đó và tuyệt đối không được ghi vào đó.
  const std::wstring want = before.substr(start) + after.substr(0, end);
  if (want.size() > kMaxWordChars) return E_FAIL;
  std::wstring got;
  if (FAILED(ReadRange(ec, word.get(), &got)) || got != want) return E_FAIL;

  *outText = want;
  *ppWord = word.detach();
  return S_OK;
}

std::vector<std::wstring> CTextService::ReconvertCandidates(
    const std::wstring& word) const {
  std::vector<std::wstring> out;
  for (const std::u32string& v : hodion::syllable_variants(
           ToU32(word), engine_.config(), kMaxCandidates)) {
    out.push_back(FromU32(v));
  }
  return out;
}

HRESULT CTextService::ApplyReconversion(ITfContext* pic, ITfRange* range,
                                        const std::wstring& expect,
                                        const std::wstring& replacement) {
  if (!pic || !range || replacement.empty()) return E_INVALIDARG;

  return RequestSyncEdit(pic, [&](TfEditCookie ec) {
    std::wstring current;
    if (FAILED(ReadRange(ec, range, &current))) return E_FAIL;
    // Văn bản đã đổi kể từ lúc dựng danh sách → bỏ qua, không ghi đè.
    if (current != expect) return S_FALSE;

    const HRESULT hr = range->SetText(
        ec, 0, replacement.c_str(), static_cast<LONG>(replacement.size()));
    if (FAILED(hr)) return hr;

    com_ptr<ITfRange> tail;
    if (SUCCEEDED(range->Clone(tail.put()))) {
      tail->Collapse(ec, TF_ANCHOR_END);
      TF_SELECTION sel;
      sel.range = tail.get();
      sel.style.ase = TF_AE_END;
      sel.style.fInterimChar = FALSE;
      pic->SetSelection(ec, 1, &sel);
    }
    return S_OK;
  });
}

// ---- Danh sách phương án ---------------------------------------------------

namespace {

class CCandidateString final : public ITfCandidateString {
 public:
  CCandidateString(std::wstring text, ULONG index)
      : text_(std::move(text)), index_(index) {}

  STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
    if (!ppv) return E_INVALIDARG;
    if (IsEqualIID(riid, IID_IUnknown) ||
        IsEqualIID(riid, kIID_ITfCandidateString)) {
      *ppv = static_cast<ITfCandidateString*>(this);
      AddRef();
      return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
  }
  STDMETHODIMP_(ULONG) AddRef() override {
    return static_cast<ULONG>(InterlockedIncrement(&ref_));
  }
  STDMETHODIMP_(ULONG) Release() override {
    const LONG r = InterlockedDecrement(&ref_);
    if (r == 0) delete this;
    return static_cast<ULONG>(r);
  }

  STDMETHODIMP GetString(BSTR* pbstr) override {
    if (!pbstr) return E_INVALIDARG;
    *pbstr = SysAllocStringLen(text_.c_str(),
                               static_cast<UINT>(text_.size()));
    return *pbstr ? S_OK : E_OUTOFMEMORY;
  }
  STDMETHODIMP GetIndex(ULONG* pnIndex) override {
    if (!pnIndex) return E_INVALIDARG;
    *pnIndex = index_;
    return S_OK;
  }

 private:
  ~CCandidateString() = default;

  std::wstring text_;
  ULONG index_;
  LONG ref_ = 1;
};

class CEnumCandidates final : public IEnumTfCandidates {
 public:
  CEnumCandidates(std::vector<std::wstring> items, ULONG pos)
      : items_(std::move(items)), pos_(pos) {}

  STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
    if (!ppv) return E_INVALIDARG;
    if (IsEqualIID(riid, IID_IUnknown) ||
        IsEqualIID(riid, kIID_IEnumTfCandidates)) {
      *ppv = static_cast<IEnumTfCandidates*>(this);
      AddRef();
      return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
  }
  STDMETHODIMP_(ULONG) AddRef() override {
    return static_cast<ULONG>(InterlockedIncrement(&ref_));
  }
  STDMETHODIMP_(ULONG) Release() override {
    const LONG r = InterlockedDecrement(&ref_);
    if (r == 0) delete this;
    return static_cast<ULONG>(r);
  }

  STDMETHODIMP Clone(IEnumTfCandidates** ppEnum) override {
    if (!ppEnum) return E_INVALIDARG;
    *ppEnum = new (std::nothrow) CEnumCandidates(items_, pos_);
    return *ppEnum ? S_OK : E_OUTOFMEMORY;
  }
  STDMETHODIMP Next(ULONG ulCount, ITfCandidateString** ppCand,
                    ULONG* pcFetched) override {
    if (!ppCand) return E_INVALIDARG;
    ULONG fetched = 0;
    while (fetched < ulCount && pos_ < items_.size()) {
      ppCand[fetched] = new (std::nothrow) CCandidateString(items_[pos_], pos_);
      if (!ppCand[fetched]) break;
      ++fetched;
      ++pos_;
    }
    if (pcFetched) *pcFetched = fetched;
    return fetched == ulCount ? S_OK : S_FALSE;
  }
  STDMETHODIMP Reset() override {
    pos_ = 0;
    return S_OK;
  }
  STDMETHODIMP Skip(ULONG ulCount) override {
    const ULONG left = static_cast<ULONG>(items_.size()) - pos_;
    pos_ += ulCount < left ? ulCount : left;
    return ulCount <= left ? S_OK : S_FALSE;
  }

 private:
  ~CEnumCandidates() = default;

  std::vector<std::wstring> items_;
  ULONG pos_ = 0;
  LONG ref_ = 1;
};

class CCandidateList final : public ITfCandidateList {
 public:
  CCandidateList(CTextService* owner, ITfContext* pic, ITfRange* range,
                 std::wstring original, std::vector<std::wstring> items)
      : owner_(owner),
        original_(std::move(original)),
        items_(std::move(items)) {
    owner_->AddRef();
    context_.copy_from(pic);
    range_.copy_from(range);
  }

  STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
    if (!ppv) return E_INVALIDARG;
    if (IsEqualIID(riid, IID_IUnknown) ||
        IsEqualIID(riid, kIID_ITfCandidateList)) {
      *ppv = static_cast<ITfCandidateList*>(this);
      AddRef();
      return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
  }
  STDMETHODIMP_(ULONG) AddRef() override {
    return static_cast<ULONG>(InterlockedIncrement(&ref_));
  }
  STDMETHODIMP_(ULONG) Release() override {
    const LONG r = InterlockedDecrement(&ref_);
    if (r == 0) delete this;
    return static_cast<ULONG>(r);
  }

  STDMETHODIMP EnumCandidates(IEnumTfCandidates** ppEnum) override {
    if (!ppEnum) return E_INVALIDARG;
    *ppEnum = new (std::nothrow) CEnumCandidates(items_, 0);
    return *ppEnum ? S_OK : E_OUTOFMEMORY;
  }
  STDMETHODIMP GetCandidate(ULONG nIndex,
                            ITfCandidateString** ppCand) override {
    if (!ppCand) return E_INVALIDARG;
    *ppCand = nullptr;
    if (nIndex >= items_.size()) return E_INVALIDARG;
    *ppCand = new (std::nothrow) CCandidateString(items_[nIndex], nIndex);
    return *ppCand ? S_OK : E_OUTOFMEMORY;
  }
  STDMETHODIMP GetCandidateNum(ULONG* pnCnt) override {
    if (!pnCnt) return E_INVALIDARG;
    *pnCnt = static_cast<ULONG>(items_.size());
    return S_OK;
  }
  STDMETHODIMP SetResult(ULONG nIndex, TfCandidateResult imcr) override {
    // CAND_SELECTED chỉ là di chuyển vệt sáng trong danh sách của ứng dụng —
    // chưa phải lựa chọn, không được đụng vào văn bản.
    if (imcr != CAND_FINALIZED) return S_OK;
    if (nIndex >= items_.size()) return E_INVALIDARG;
    return owner_->ApplyReconversion(context_.get(), range_.get(), original_,
                                     items_[nIndex]);
  }

 private:
  ~CCandidateList() { owner_->Release(); }

  CTextService* owner_;
  com_ptr<ITfContext> context_;
  com_ptr<ITfRange> range_;
  std::wstring original_;
  std::vector<std::wstring> items_;
  LONG ref_ = 1;
};

}  // namespace

// ---- ITfFunctionProvider ---------------------------------------------------

STDMETHODIMP CTextService::GetType(GUID* pguid) {
  if (!pguid) return E_INVALIDARG;
  *pguid = CLSID_HodionKeyService;
  return S_OK;
}

STDMETHODIMP CTextService::GetDescription(BSTR* pbstrDesc) {
  if (!pbstrDesc) return E_INVALIDARG;
  *pbstrDesc = SysAllocString(kServiceDescription);
  return *pbstrDesc ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP CTextService::GetFunction(REFGUID rguid, REFIID riid,
                                       IUnknown** ppunk) {
  if (!ppunk) return E_INVALIDARG;
  *ppunk = nullptr;
  // GUID_NULL = hàm chuẩn của TSF; ta chỉ cung cấp reconversion.
  if (!IsEqualGUID(rguid, GUID_NULL)) return E_NOINTERFACE;
  if (!IsEqualIID(riid, kIID_ITfFnReconversion)) return E_NOINTERFACE;

  *ppunk = static_cast<IUnknown*>(static_cast<ITfFnReconversion*>(this));
  AddRef();
  return S_OK;
}

// ---- ITfFnReconversion -----------------------------------------------------

STDMETHODIMP CTextService::GetDisplayName(BSTR* pbstrName) {
  if (!pbstrName) return E_INVALIDARG;
  *pbstrName = SysAllocString(kReconversionName);
  return *pbstrName ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP CTextService::QueryRange(ITfRange* pRange, ITfRange** ppNewRange,
                                      BOOL* pfConvertable) {
  if (ppNewRange) *ppNewRange = nullptr;
  if (pfConvertable) *pfConvertable = FALSE;
  if (!pRange) return E_INVALIDARG;

  com_ptr<ITfContext> pic;
  if (FAILED(pRange->GetContext(pic.put())) || !pic) return S_OK;

  com_ptr<ITfRange> word;
  std::wstring text;
  RequestSyncEdit(pic.get(), TF_ES_SYNC | TF_ES_READ, [&](TfEditCookie ec) {
    return FindReconvertRange(ec, pRange, word.put(), &text);
  });
  if (!word) return S_OK;  // không chuyển được — không phải lỗi

  // Chỉ nhận khi thật sự có phương án KHÁC để đổi sang.
  const std::vector<std::wstring> items = ReconvertCandidates(text);
  if (items.size() < 2) return S_OK;

  if (pfConvertable) *pfConvertable = TRUE;
  if (ppNewRange) *ppNewRange = word.detach();
  return S_OK;
}

STDMETHODIMP CTextService::GetReconversion(ITfRange* pRange,
                                           ITfCandidateList** ppCandList) {
  if (!ppCandList) return E_INVALIDARG;
  *ppCandList = nullptr;
  if (!pRange) return E_INVALIDARG;

  com_ptr<ITfContext> pic;
  if (FAILED(pRange->GetContext(pic.put())) || !pic) return E_FAIL;

  com_ptr<ITfRange> word;
  std::wstring text;
  RequestSyncEdit(pic.get(), TF_ES_SYNC | TF_ES_READ, [&](TfEditCookie ec) {
    return FindReconvertRange(ec, pRange, word.put(), &text);
  });
  if (!word) return E_FAIL;

  std::vector<std::wstring> items = ReconvertCandidates(text);
  if (items.empty()) return E_FAIL;

  *ppCandList = new (std::nothrow) CCandidateList(this, pic.get(), word.get(),
                                                  text, std::move(items));
  return *ppCandList ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP CTextService::Reconvert(ITfRange* pRange) {
  if (!pRange) return E_INVALIDARG;

  com_ptr<ITfContext> pic;
  if (FAILED(pRange->GetContext(pic.put())) || !pic) return E_FAIL;

  com_ptr<ITfRange> word;
  std::wstring text;
  RequestSyncEdit(pic.get(), TF_ES_SYNC | TF_ES_READ, [&](TfEditCookie ec) {
    return FindReconvertRange(ec, pRange, word.put(), &text);
  });
  if (!word) return E_FAIL;

  // Không có cửa sổ chọn riêng, nên Reconvert xoay sang phương án kế tiếp.
  // Gọi lại nhiều lần là đi hết vòng — ứng dụng nào có giao diện chọn thì
  // dùng GetReconversion để lấy cả danh sách.
  const std::vector<std::wstring> items = ReconvertCandidates(text);
  if (items.size() < 2) return E_FAIL;
  size_t next = 0;
  for (size_t i = 0; i < items.size(); ++i) {
    if (items[i] == text) {
      next = (i + 1) % items.size();
      break;
    }
  }
  return ApplyReconversion(pic.get(), word.get(), text, items[next]);
}
