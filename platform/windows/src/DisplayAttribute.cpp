// Display attribute: gạch chân đoạn văn bản đang ghép vần.
#include <new>

#include "TextService.h"

namespace {

class CDisplayAttributeInfo : public ITfDisplayAttributeInfo {
 public:
  // IUnknown
  STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
    if (!ppv) return E_INVALIDARG;
    if (IsEqualIID(riid, IID_IUnknown) ||
        IsEqualIID(riid, IID_ITfDisplayAttributeInfo)) {
      *ppv = static_cast<ITfDisplayAttributeInfo*>(this);
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

  // ITfDisplayAttributeInfo
  STDMETHODIMP GetGUID(GUID* pguid) override {
    if (!pguid) return E_INVALIDARG;
    *pguid = GUID_HodionKeyDisplayAttributeInput;
    return S_OK;
  }
  STDMETHODIMP GetDescription(BSTR* pbstrDesc) override {
    if (!pbstrDesc) return E_INVALIDARG;
    *pbstrDesc = SysAllocString(L"HodionKey composing text");
    return *pbstrDesc ? S_OK : E_OUTOFMEMORY;
  }
  STDMETHODIMP GetAttributeInfo(TF_DISPLAYATTRIBUTE* pda) override {
    if (!pda) return E_INVALIDARG;
    *pda = TF_DISPLAYATTRIBUTE{};
    pda->crText.type = TF_CT_NONE;
    pda->crBk.type = TF_CT_NONE;
    pda->crLine.type = TF_CT_NONE;
    pda->lsStyle = TF_LS_SOLID;
    pda->fBoldLine = FALSE;
    pda->bAttr = TF_ATTR_INPUT;
    return S_OK;
  }
  STDMETHODIMP SetAttributeInfo(const TF_DISPLAYATTRIBUTE*) override {
    return E_NOTIMPL;
  }
  STDMETHODIMP Reset() override { return S_OK; }

 private:
  virtual ~CDisplayAttributeInfo() = default;

  LONG ref_ = 1;
};

class CEnumDisplayAttributeInfo : public IEnumTfDisplayAttributeInfo {
 public:
  // IUnknown
  STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
    if (!ppv) return E_INVALIDARG;
    if (IsEqualIID(riid, IID_IUnknown) ||
        IsEqualIID(riid, IID_IEnumTfDisplayAttributeInfo)) {
      *ppv = static_cast<IEnumTfDisplayAttributeInfo*>(this);
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

  // IEnumTfDisplayAttributeInfo — danh sách chỉ có một phần tử.
  STDMETHODIMP Clone(IEnumTfDisplayAttributeInfo** ppEnum) override {
    if (!ppEnum) return E_INVALIDARG;
    auto* clone = new (std::nothrow) CEnumDisplayAttributeInfo();
    if (!clone) return E_OUTOFMEMORY;
    clone->index_ = index_;
    *ppEnum = clone;
    return S_OK;
  }
  STDMETHODIMP Next(ULONG ulCount, ITfDisplayAttributeInfo** rgInfo,
                    ULONG* pcFetched) override {
    ULONG fetched = 0;
    if (ulCount > 0 && index_ == 0) {
      auto* info = new (std::nothrow) CDisplayAttributeInfo();
      if (!info) return E_OUTOFMEMORY;
      rgInfo[0] = info;
      fetched = 1;
      index_ = 1;
    }
    if (pcFetched) *pcFetched = fetched;
    return fetched == ulCount ? S_OK : S_FALSE;
  }
  STDMETHODIMP Reset() override {
    index_ = 0;
    return S_OK;
  }
  STDMETHODIMP Skip(ULONG ulCount) override {
    index_ += static_cast<int>(ulCount);
    return index_ <= 1 ? S_OK : S_FALSE;
  }

 private:
  virtual ~CEnumDisplayAttributeInfo() = default;

  LONG ref_ = 1;
  int index_ = 0;
};

}  // namespace

STDMETHODIMP CTextService::EnumDisplayAttributeInfo(
    IEnumTfDisplayAttributeInfo** ppEnum) {
  if (!ppEnum) return E_INVALIDARG;
  auto* e = new (std::nothrow) CEnumDisplayAttributeInfo();
  if (!e) return E_OUTOFMEMORY;
  *ppEnum = e;
  return S_OK;
}

STDMETHODIMP CTextService::GetDisplayAttributeInfo(
    REFGUID guid, ITfDisplayAttributeInfo** ppInfo) {
  if (!ppInfo) return E_INVALIDARG;
  *ppInfo = nullptr;
  if (!IsEqualGUID(guid, GUID_HodionKeyDisplayAttributeInput)) {
    return E_INVALIDARG;
  }
  auto* info = new (std::nothrow) CDisplayAttributeInfo();
  if (!info) return E_OUTOFMEMORY;
  *ppInfo = info;
  return S_OK;
}
