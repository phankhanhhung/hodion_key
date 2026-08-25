// Gói một lambda thành ITfEditSession dùng một lần.
#pragma once

#include <functional>

#include "HodionTsf.h"

class CEditSessionLambda : public ITfEditSession {
 public:
  explicit CEditSessionLambda(std::function<HRESULT(TfEditCookie)> fn)
      : fn_(std::move(fn)) {}

  // IUnknown
  STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
    if (!ppv) return E_INVALIDARG;
    if (IsEqualIID(riid, IID_IUnknown) ||
        IsEqualIID(riid, IID_ITfEditSession)) {
      *ppv = static_cast<ITfEditSession*>(this);
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

  // ITfEditSession
  STDMETHODIMP DoEditSession(TfEditCookie ec) override { return fn_(ec); }

 private:
  ~CEditSessionLambda() = default;

  std::function<HRESULT(TfEditCookie)> fn_;
  LONG ref_ = 1;
};
