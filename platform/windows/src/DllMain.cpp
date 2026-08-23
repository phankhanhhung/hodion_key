#include <olectl.h>

#include "HodionTsf.h"

HINSTANCE g_hInst = nullptr;
static LONG g_dllRefCount = 0;

IClassFactory* GetHodionClassFactory();

void DllAddRef() { InterlockedIncrement(&g_dllRefCount); }
void DllRelease() { InterlockedDecrement(&g_dllRefCount); }

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID /*reserved*/) {
  if (dwReason == DLL_PROCESS_ATTACH) {
    g_hInst = hInstance;
    DisableThreadLibraryCalls(hInstance);
  }
  return TRUE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv) {
  if (!ppv) return E_INVALIDARG;
  *ppv = nullptr;
  if (!IsEqualCLSID(rclsid, CLSID_HodionKeyService)) {
    return CLASS_E_CLASSNOTAVAILABLE;
  }
  return GetHodionClassFactory()->QueryInterface(riid, ppv);
}

STDAPI DllCanUnloadNow() { return g_dllRefCount == 0 ? S_OK : S_FALSE; }

STDAPI DllRegisterServer() {
  HRESULT hr = RegisterComServer();
  if (SUCCEEDED(hr)) hr = RegisterProfiles();
  if (SUCCEEDED(hr)) hr = RegisterCategories();
  if (FAILED(hr)) DllUnregisterServer();
  return SUCCEEDED(hr) ? S_OK : SELFREG_E_CLASS;
}

STDAPI DllUnregisterServer() {
  UnregisterCategories();
  UnregisterProfiles();
  UnregisterComServer();
  return S_OK;
}
