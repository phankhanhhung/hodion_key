#include <new>

#include "TextService.h"

class CClassFactory : public IClassFactory {
 public:
  // IUnknown — factory là singleton tĩnh, không đếm tham chiếu theo object.
  STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
    if (!ppv) return E_INVALIDARG;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory)) {
      *ppv = static_cast<IClassFactory*>(this);
      DllAddRef();
      return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
  }
  STDMETHODIMP_(ULONG) AddRef() override {
    DllAddRef();
    return 2;
  }
  STDMETHODIMP_(ULONG) Release() override {
    DllRelease();
    return 1;
  }

  // IClassFactory
  STDMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid,
                              void** ppv) override {
    if (!ppv) return E_INVALIDARG;
    *ppv = nullptr;
    if (pUnkOuter) return CLASS_E_NOAGGREGATION;

    CTextService* service = new (std::nothrow) CTextService();
    if (!service) return E_OUTOFMEMORY;

    const HRESULT hr = service->QueryInterface(riid, ppv);
    service->Release();
    return hr;
  }
  STDMETHODIMP LockServer(BOOL fLock) override {
    if (fLock) {
      DllAddRef();
    } else {
      DllRelease();
    }
    return S_OK;
  }
};

static CClassFactory g_classFactory;

IClassFactory* GetHodionClassFactory() { return &g_classFactory; }
