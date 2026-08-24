#include "TextService.h"

#include <cwchar>

#include "EditSession.h"
#include "hodion/english_words.h"
#include "hodion/utf.h"

namespace {

std::wstring ToWide(const std::u32string& s) {
  const std::u16string u16 = hodion::utf::to_utf16(s);
  return std::wstring(u16.begin(), u16.end());
}

}  // namespace

std::wstring HodionToWide(const std::u32string& s) { return ToWide(s); }

CTextService::CTextService() { DllAddRef(); }

CTextService::~CTextService() { DllRelease(); }

// ---- IUnknown -------------------------------------------------------------

STDMETHODIMP CTextService::QueryInterface(REFIID riid, void** ppv) {
  if (!ppv) return E_INVALIDARG;
  *ppv = nullptr;

  if (IsEqualIID(riid, IID_IUnknown) ||
      IsEqualIID(riid, IID_ITfTextInputProcessor)) {
    *ppv = static_cast<ITfTextInputProcessor*>(this);
  } else if (IsEqualIID(riid, kIID_ITfTextInputProcessorEx)) {
    *ppv = static_cast<ITfTextInputProcessorEx*>(this);
  } else if (IsEqualIID(riid, IID_ITfThreadMgrEventSink)) {
    *ppv = static_cast<ITfThreadMgrEventSink*>(this);
  } else if (IsEqualIID(riid, IID_ITfKeyEventSink)) {
    *ppv = static_cast<ITfKeyEventSink*>(this);
  } else if (IsEqualIID(riid, IID_ITfCompositionSink)) {
    *ppv = static_cast<ITfCompositionSink*>(this);
  } else if (IsEqualIID(riid, kIID_ITfCompartmentEventSink)) {
    *ppv = static_cast<ITfCompartmentEventSink*>(this);
  } else if (IsEqualIID(riid, kIID_ITfDisplayAttributeProvider)) {
    *ppv = static_cast<ITfDisplayAttributeProvider*>(this);
  } else if (IsEqualIID(riid, IID_ITfFunctionProvider)) {
    *ppv = static_cast<ITfFunctionProvider*>(this);
  } else if (IsEqualIID(riid, kIID_ITfFnReconversion)) {
    *ppv = static_cast<ITfFnReconversion*>(this);
  } else {
    return E_NOINTERFACE;
  }
  AddRef();
  return S_OK;
}

STDMETHODIMP_(ULONG) CTextService::AddRef() {
  return static_cast<ULONG>(InterlockedIncrement(&refCount_));
}

STDMETHODIMP_(ULONG) CTextService::Release() {
  const LONG r = InterlockedDecrement(&refCount_);
  if (r == 0) delete this;
  return static_cast<ULONG>(r);
}

// ---- Activate / Deactivate ------------------------------------------------

STDMETHODIMP CTextService::Activate(ITfThreadMgr* ptim, TfClientId tid) {
  return ActivateEx(ptim, tid, 0);
}

STDMETHODIMP CTextService::ActivateEx(ITfThreadMgr* ptim, TfClientId tid,
                                      DWORD /*dwFlags*/) {
  if (!ptim) return E_INVALIDARG;

  threadMgr_.copy_from(ptim);
  clientId_ = tid;

  // Từ điển tiếng Anh: chỉ được tra lúc chốt từ, không nằm trên đường gõ
  // từng phím. Bảng là dữ liệu tĩnh nên sống suốt đời tiến trình.
  engine_.set_foreign_words(&hodion::english_words());

  ApplySettings(LoadHodionSettings());
  watcher_.start();

  // Theo dõi thay đổi focus để chốt composition dở khi người dùng rời ô nhập.
  {
    com_ptr<ITfSource> source;
    if (SUCCEEDED(threadMgr_->QueryInterface(IID_ITfSource,
                                             source.put_void()))) {
      source->AdviseSink(IID_ITfThreadMgrEventSink,
                         static_cast<ITfThreadMgrEventSink*>(this),
                         &threadMgrEventSinkCookie_);
    }
  }

  // Nhận phím ở chế độ foreground.
  {
    com_ptr<ITfKeystrokeMgr> keystrokeMgr;
    if (SUCCEEDED(threadMgr_->QueryInterface(IID_ITfKeystrokeMgr,
                                             keystrokeMgr.put_void()))) {
      if (SUCCEEDED(keystrokeMgr->AdviseKeyEventSink(
              clientId_, static_cast<ITfKeyEventSink*>(this), TRUE))) {
        keySinkAdvised_ = true;
      }
    }
  }

  // Lấy atom cho display attribute (gạch chân đoạn đang ghép).
  {
    com_ptr<ITfCategoryMgr> categoryMgr;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_CategoryMgr, nullptr,
                                   CLSCTX_INPROC_SERVER, IID_ITfCategoryMgr,
                                   categoryMgr.put_void()))) {
      categoryMgr->RegisterGUID(GUID_HodionKeyDisplayAttributeInput,
                                &displayAttributeAtom_);
    }
  }

  // Phím chuyển Việt/Anh + theo dõi compartment bật/tắt của hệ thống.
  RegisterToggleKey();
  {
    com_ptr<ITfCompartment> comp;
    if (SUCCEEDED(GetOpenCloseCompartment(comp.put())) && comp) {
      com_ptr<ITfSource> source;
      if (SUCCEEDED(comp->QueryInterface(IID_ITfSource, source.put_void()))) {
        source->AdviseSink(kIID_ITfCompartmentEventSink,
                           static_cast<ITfCompartmentEventSink*>(this),
                           &openCloseSinkCookie_);
      }
    }
  }
  PushOpenCloseCompartment();

  return S_OK;
}

STDMETHODIMP CTextService::Deactivate() {
  FinalizeComposition();
  watcher_.stop();
  UnregisterToggleKey();

  if (threadMgr_) {
    if (openCloseSinkCookie_ != TF_INVALID_COOKIE) {
      com_ptr<ITfCompartment> comp;
      if (SUCCEEDED(GetOpenCloseCompartment(comp.put())) && comp) {
        com_ptr<ITfSource> source;
        if (SUCCEEDED(comp->QueryInterface(IID_ITfSource,
                                           source.put_void()))) {
          source->UnadviseSink(openCloseSinkCookie_);
        }
      }
      openCloseSinkCookie_ = TF_INVALID_COOKIE;
    }
    if (keySinkAdvised_) {
      com_ptr<ITfKeystrokeMgr> keystrokeMgr;
      if (SUCCEEDED(threadMgr_->QueryInterface(IID_ITfKeystrokeMgr,
                                               keystrokeMgr.put_void()))) {
        keystrokeMgr->UnadviseKeyEventSink(clientId_);
      }
      keySinkAdvised_ = false;
    }
    if (threadMgrEventSinkCookie_ != TF_INVALID_COOKIE) {
      com_ptr<ITfSource> source;
      if (SUCCEEDED(threadMgr_->QueryInterface(IID_ITfSource,
                                               source.put_void()))) {
        source->UnadviseSink(threadMgrEventSinkCookie_);
      }
      threadMgrEventSinkCookie_ = TF_INVALID_COOKIE;
    }
  }

  threadMgr_.reset();
  clientId_ = TF_CLIENTID_NULL;
  return S_OK;
}

// ---- ITfThreadMgrEventSink ------------------------------------------------

STDMETHODIMP CTextService::OnInitDocumentMgr(ITfDocumentMgr*) { return S_OK; }

STDMETHODIMP CTextService::OnUninitDocumentMgr(ITfDocumentMgr*) {
  return S_OK;
}

STDMETHODIMP CTextService::OnSetFocus(ITfDocumentMgr* /*pdimFocus*/,
                                      ITfDocumentMgr* /*pdimPrevFocus*/) {
  // Chuyển ô nhập/tài liệu: từ đang gõ dở được chốt tại chỗ.
  FinalizeComposition();
  ReloadSettingsIfChanged();
  // Ô mới có thể là URL/mật khẩu — phải hỏi lại kiểu ô.
  InvalidateInputScope();
  return S_OK;
}

STDMETHODIMP CTextService::OnPushContext(ITfContext*) {
  InvalidateInputScope();
  return S_OK;
}

STDMETHODIMP CTextService::OnPopContext(ITfContext*) {
  InvalidateInputScope();
  return S_OK;
}

// ---- ITfCompositionSink ---------------------------------------------------

STDMETHODIMP CTextService::OnCompositionTerminated(
    TfEditCookie /*ecWrite*/, ITfComposition* /*pComposition*/) {
  // Ứng dụng chủ động kết thúc composition (click chuột, v.v.):
  // buông tham chiếu và quên trạng thái gõ dở.
  AbandonComposition();
  return S_OK;
}

// ---- Composition helpers --------------------------------------------------

HRESULT CTextService::RequestSyncEdit(
    ITfContext* pic, DWORD flags,
    const std::function<HRESULT(TfEditCookie)>& fn) {
  if (!pic) return E_INVALIDARG;
  CEditSessionLambda* session = new (std::nothrow) CEditSessionLambda(fn);
  if (!session) return E_OUTOFMEMORY;

  HRESULT hrSession = E_FAIL;
  const HRESULT hr =
      pic->RequestEditSession(clientId_, session, flags, &hrSession);
  session->Release();
  return FAILED(hr) ? hr : hrSession;
}

HRESULT CTextService::EnsureComposition(TfEditCookie ec, ITfContext* pic) {
  if (composition_) return S_OK;

  com_ptr<ITfInsertAtSelection> insertAtSelection;
  HRESULT hr = pic->QueryInterface(IID_ITfInsertAtSelection,
                                   insertAtSelection.put_void());
  if (FAILED(hr)) return hr;

  com_ptr<ITfRange> range;
  hr = insertAtSelection->InsertTextAtSelection(ec, TF_IAS_QUERYONLY, nullptr,
                                                0, range.put());
  if (FAILED(hr) || !range) return FAILED(hr) ? hr : E_FAIL;

  com_ptr<ITfContextComposition> contextComposition;
  hr = pic->QueryInterface(IID_ITfContextComposition,
                           contextComposition.put_void());
  if (FAILED(hr)) return hr;

  hr = contextComposition->StartComposition(
      ec, range.get(), static_cast<ITfCompositionSink*>(this),
      composition_.put());
  if (FAILED(hr) || !composition_) return FAILED(hr) ? hr : E_FAIL;

  compositionContext_.copy_from(pic);
  return S_OK;
}

HRESULT CTextService::SetCompositionText(TfEditCookie ec,
                                         const std::wstring& text) {
  if (!composition_ || !compositionContext_) return E_UNEXPECTED;

  com_ptr<ITfRange> range;
  HRESULT hr = composition_->GetRange(range.put());
  if (FAILED(hr)) return hr;

  hr = range->SetText(ec, 0, text.c_str(), static_cast<LONG>(text.size()));
  if (FAILED(hr)) return hr;

  // Gạch chân phần đang ghép.
  if (displayAttributeAtom_ != TF_INVALID_GUIDATOM) {
    com_ptr<ITfProperty> prop;
    if (SUCCEEDED(compositionContext_->GetProperty(GUID_PROP_ATTRIBUTE,
                                                   prop.put()))) {
      VARIANT var;
      VariantInit(&var);
      var.vt = VT_I4;
      var.lVal = static_cast<LONG>(displayAttributeAtom_);
      prop->SetValue(ec, range.get(), &var);
    }
  }

  // Đưa caret về cuối đoạn ghép.
  range->Collapse(ec, TF_ANCHOR_END);
  TF_SELECTION sel;
  sel.range = range.get();
  sel.style.ase = TF_AE_END;
  sel.style.fInterimChar = FALSE;
  compositionContext_->SetSelection(ec, 1, &sel);

  return S_OK;
}

HRESULT CTextService::EndCompositionKeepText(TfEditCookie ec) {
  if (!composition_) return S_OK;

  // Xóa display attribute để chữ đã chốt không còn gạch chân.
  com_ptr<ITfRange> range;
  if (SUCCEEDED(composition_->GetRange(range.put())) && compositionContext_) {
    com_ptr<ITfProperty> prop;
    if (SUCCEEDED(compositionContext_->GetProperty(GUID_PROP_ATTRIBUTE,
                                                   prop.put()))) {
      prop->Clear(ec, range.get());
    }
  }

  const HRESULT hr = composition_->EndComposition(ec);
  composition_.reset();
  compositionContext_.reset();
  return hr;
}

void CTextService::FinalizeComposition() {
  // Chuỗi chốt có thể KHÁC chữ đang hiển thị: engine khôi phục chuỗi phím
  // thô khi từ đó là một từ tiếng Anh đã biết ("mêting" → "meeting") hoặc
  // không phải tiếng Việt hợp lệ. Vì vậy phải hỏi engine chứ không giữ
  // nguyên những gì đang hiện — nếu không, chốt bằng Enter/Tab/mũi tên sẽ
  // bỏ lỡ việc khôi phục mà chốt bằng dấu cách vẫn làm.
  const std::wstring text = HodionToWide(engine_.commit());
  if (!composition_ || !compositionContext_) {
    composition_.reset();
    compositionContext_.reset();
    return;
  }

  // Giữ compositionContext_ sống qua edit session (reset bên trong callback).
  com_ptr<ITfContext> ctx;
  ctx.copy_from(compositionContext_.get());
  RequestSyncEdit(ctx.get(), [&](TfEditCookie ec) {
    HRESULT hr = text.empty() ? S_OK : SetCompositionText(ec, text);
    if (SUCCEEDED(hr)) hr = EndCompositionKeepText(ec);
    return hr;
  });
  // Nếu edit session thất bại (ứng dụng đã biến mất…) thì buông tham chiếu.
  composition_.reset();
  compositionContext_.reset();
}

void CTextService::AbandonComposition() {
  composition_.reset();
  compositionContext_.reset();
  engine_.reset();
}

// ---- Cấu hình & bật/tắt tiếng Việt ---------------------------------------

void CTextService::ApplySettings(const HodionSettings& s) {
  engine_.set_config(s.engine);
  vietnamese_ = s.vietnamese_on;
  skipInputScopes_ = s.skip_input_scopes;
  InvalidateInputScope();

  if (!(s.toggle == toggleKey_)) {
    UnregisterToggleKey();
    toggleKey_ = s.toggle;
    if (threadMgr_) RegisterToggleKey();
  }
}

void CTextService::ReloadSettingsIfChanged() {
  if (!watcher_.poll()) return;

  const HodionSettings s = LoadHodionSettings();
  const bool was_on = vietnamese_;
  // Đang gõ dở mà cấu hình đổi: chốt chữ trên màn hình trước khi nạp luật mới.
  FinalizeComposition();
  ApplySettings(s);
  if (vietnamese_ != was_on) PushOpenCloseCompartment();
}

HRESULT CTextService::GetOpenCloseCompartment(ITfCompartment** out) const {
  if (!out) return E_INVALIDARG;
  *out = nullptr;
  if (!threadMgr_) return E_UNEXPECTED;

  com_ptr<ITfCompartmentMgr> mgr;
  HRESULT hr = threadMgr_->QueryInterface(kIID_ITfCompartmentMgr,
                                          mgr.put_void());
  if (FAILED(hr)) return hr;
  return mgr->GetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE, out);
}

void CTextService::PushOpenCloseCompartment() {
  com_ptr<ITfCompartment> comp;
  if (FAILED(GetOpenCloseCompartment(comp.put())) || !comp) return;

  VARIANT var;
  VariantInit(&var);
  var.vt = VT_I4;
  var.lVal = vietnamese_ ? 1 : 0;

  updatingCompartment_ = true;
  comp->SetValue(clientId_, &var);
  updatingCompartment_ = false;
}

void CTextService::SetVietnamese(bool on, bool persist) {
  if (vietnamese_ == on) return;
  FinalizeComposition();
  vietnamese_ = on;
  PushOpenCloseCompartment();
  if (persist) {
    SaveHodionVietnameseOn(on);
    // Ghi registry sẽ kích hoạt watcher của chính ta. Nuốt lượt báo ngay khi
    // có thể; registry báo bất đồng bộ nên đôi lúc vẫn lọt một lần nạp lại —
    // vô hại vì giá trị nạp về đúng bằng giá trị vừa ghi.
    watcher_.poll();
  }
}

void CTextService::RegisterToggleKey() {
  // Không bao giờ chiếm một phím trần — nó sẽ biến mất khỏi bàn phím.
  if (toggleRegistered_ || !threadMgr_ || !toggleKey_.valid()) return;

  com_ptr<ITfKeystrokeMgr> keystrokeMgr;
  if (FAILED(threadMgr_->QueryInterface(IID_ITfKeystrokeMgr,
                                        keystrokeMgr.put_void()))) {
    return;
  }

  TF_PRESERVEDKEY key;
  key.uVKey = toggleKey_.vk;
  key.uModifiers = toggleKey_.mods;
  if (SUCCEEDED(keystrokeMgr->PreserveKey(
          clientId_, GUID_HodionKeyToggle, &key, kToggleKeyDescription,
          static_cast<ULONG>(wcslen(kToggleKeyDescription))))) {
    toggleRegistered_ = true;
  }
}

void CTextService::UnregisterToggleKey() {
  if (!toggleRegistered_ || !threadMgr_) {
    toggleRegistered_ = false;
    return;
  }
  com_ptr<ITfKeystrokeMgr> keystrokeMgr;
  if (SUCCEEDED(threadMgr_->QueryInterface(IID_ITfKeystrokeMgr,
                                           keystrokeMgr.put_void()))) {
    TF_PRESERVEDKEY key;
    key.uVKey = toggleKey_.vk;
    key.uModifiers = toggleKey_.mods;
    keystrokeMgr->UnpreserveKey(GUID_HodionKeyToggle, &key);
  }
  toggleRegistered_ = false;
}

// ---- ITfCompartmentEventSink ----------------------------------------------

STDMETHODIMP CTextService::OnChange(REFGUID rguid) {
  if (updatingCompartment_) return S_OK;
  if (!IsEqualGUID(rguid, GUID_COMPARTMENT_KEYBOARD_OPENCLOSE)) return S_OK;

  com_ptr<ITfCompartment> comp;
  if (FAILED(GetOpenCloseCompartment(comp.put())) || !comp) return S_OK;

  VARIANT var;
  VariantInit(&var);
  if (SUCCEEDED(comp->GetValue(&var)) && var.vt == VT_I4) {
    // Hệ thống (thanh ngôn ngữ, ứng dụng) bật/tắt IME — theo trạng thái đó
    // nhưng không ghi đè lựa chọn đã lưu của người dùng.
    SetVietnamese(var.lVal != 0, /*persist=*/false);
  }
  VariantClear(&var);
  return S_OK;
}
