// Đăng ký / gỡ đăng ký: COM server (HKCR\CLSID), language profile tiếng Việt
// và các category TSF. Chạy qua regsvr32 (cần quyền admin).
#include <cwchar>
#include <string>

#include "HodionTsf.h"

namespace {

// Các category GUID được định nghĩa tại chỗ để không phụ thuộc phiên bản SDK.
constexpr GUID kCatTipKeyboard = {
    0x34745c63, 0xb2f0, 0x4784, {0x8b, 0x67, 0x5e, 0x12, 0xc8, 0x70, 0x1a, 0x31}};
constexpr GUID kCatDisplayAttributeProvider = {
    0x046b8c80, 0x1647, 0x40f7, {0x9b, 0x21, 0xb9, 0x3b, 0x81, 0xaa, 0xbc, 0x1b}};
constexpr GUID kCatTipcapSecureMode = {
    0x49d2f9ce, 0x1f5e, 0x11d7, {0xa6, 0xd3, 0x00, 0x06, 0x5b, 0x84, 0x43, 0x5c}};
constexpr GUID kCatTipcapUiElementEnabled = {
    0x49d2f9cf, 0x1f5e, 0x11d7, {0xa6, 0xd3, 0x00, 0x06, 0x5b, 0x84, 0x43, 0x5c}};
constexpr GUID kCatTipcapInputModeCompartment = {
    0xccf05dd8, 0x4a87, 0x11d7, {0xa6, 0xe2, 0x00, 0x06, 0x5b, 0x84, 0x43, 0x5c}};
constexpr GUID kCatTipcapComless = {
    0x364215d9, 0x75bc, 0x11d7, {0xa6, 0xef, 0x00, 0x06, 0x5b, 0x84, 0x43, 0x5c}};
constexpr GUID kCatTipcapImmersiveSupport = {
    0x13a016df, 0x560b, 0x46cd, {0x94, 0x7a, 0x4c, 0x3a, 0xf1, 0xe0, 0xe3, 0x5d}};
constexpr GUID kCatTipcapSystraySupport = {
    0x25504fb4, 0x7bab, 0x4bc1, {0x9c, 0x69, 0xcf, 0x81, 0x89, 0x0f, 0x0e, 0xf5}};

constexpr const GUID* kCategories[] = {
    &kCatTipKeyboard,
    &kCatDisplayAttributeProvider,
    &kCatTipcapSecureMode,
    &kCatTipcapUiElementEnabled,
    &kCatTipcapInputModeCompartment,
    &kCatTipcapComless,
    &kCatTipcapImmersiveSupport,
    &kCatTipcapSystraySupport,
};

std::wstring ClsidKeyPath() {
  WCHAR clsid[64] = {};
  StringFromGUID2(CLSID_HodionKeyService, clsid, ARRAYSIZE(clsid));
  std::wstring path = L"CLSID\\";
  path += clsid;
  return path;
}

LSTATUS SetKeyString(HKEY parent, const WCHAR* subKey, const WCHAR* valueName,
                     const std::wstring& data, HKEY* outKey = nullptr) {
  HKEY key = nullptr;
  LSTATUS st = RegCreateKeyExW(parent, subKey, 0, nullptr,
                               REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr,
                               &key, nullptr);
  if (st != ERROR_SUCCESS) return st;
  st = RegSetValueExW(
      key, valueName, 0, REG_SZ, reinterpret_cast<const BYTE*>(data.c_str()),
      static_cast<DWORD>((data.size() + 1) * sizeof(WCHAR)));
  if (outKey) {
    *outKey = key;
  } else {
    RegCloseKey(key);
  }
  return st;
}

std::wstring ModulePath() {
  WCHAR path[MAX_PATH] = {};
  GetModuleFileNameW(g_hInst, path, ARRAYSIZE(path));
  return path;
}

}  // namespace

HRESULT RegisterComServer() {
  const std::wstring keyPath = ClsidKeyPath();

  if (SetKeyString(HKEY_CLASSES_ROOT, keyPath.c_str(), nullptr,
                   kServiceDescription) != ERROR_SUCCESS) {
    return E_FAIL;
  }

  HKEY inproc = nullptr;
  const std::wstring inprocPath = keyPath + L"\\InprocServer32";
  if (SetKeyString(HKEY_CLASSES_ROOT, inprocPath.c_str(), nullptr,
                   ModulePath(), &inproc) != ERROR_SUCCESS) {
    return E_FAIL;
  }
  const WCHAR threading[] = L"Apartment";
  const LSTATUS st = RegSetValueExW(
      inproc, L"ThreadingModel", 0, REG_SZ,
      reinterpret_cast<const BYTE*>(threading), sizeof(threading));
  RegCloseKey(inproc);
  return st == ERROR_SUCCESS ? S_OK : E_FAIL;
}

void UnregisterComServer() {
  RegDeleteTreeW(HKEY_CLASSES_ROOT, ClsidKeyPath().c_str());
}

HRESULT RegisterProfiles() {
  com_ptr<ITfInputProcessorProfileMgr> profileMgr;
  HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr,
                                CLSCTX_INPROC_SERVER,
                                IID_ITfInputProcessorProfileMgr,
                                profileMgr.put_void());
  if (FAILED(hr)) return hr;

  const std::wstring icon = ModulePath();
  return profileMgr->RegisterProfile(
      CLSID_HodionKeyService, kHodionLangId, GUID_HodionKeyProfile,
      kServiceDescription,
      static_cast<ULONG>(std::wcslen(kServiceDescription)), icon.c_str(),
      static_cast<ULONG>(icon.size()), 0 /*icon index*/, nullptr /*hkl*/,
      0 /*preferred layout*/, TRUE /*enabled by default*/, 0 /*flags*/);
}

void UnregisterProfiles() {
  com_ptr<ITfInputProcessorProfileMgr> profileMgr;
  if (SUCCEEDED(CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr,
                                 CLSCTX_INPROC_SERVER,
                                 IID_ITfInputProcessorProfileMgr,
                                 profileMgr.put_void()))) {
    profileMgr->UnregisterProfile(CLSID_HodionKeyService, kHodionLangId,
                                  GUID_HodionKeyProfile, 0);
  }
}

HRESULT RegisterCategories() {
  com_ptr<ITfCategoryMgr> categoryMgr;
  HRESULT hr =
      CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER,
                       IID_ITfCategoryMgr, categoryMgr.put_void());
  if (FAILED(hr)) return hr;

  for (const GUID* cat : kCategories) {
    hr = categoryMgr->RegisterCategory(CLSID_HodionKeyService, *cat,
                                       CLSID_HodionKeyService);
    if (FAILED(hr)) return hr;
  }
  return S_OK;
}

void UnregisterCategories() {
  com_ptr<ITfCategoryMgr> categoryMgr;
  if (SUCCEEDED(CoCreateInstance(CLSID_TF_CategoryMgr, nullptr,
                                 CLSCTX_INPROC_SERVER, IID_ITfCategoryMgr,
                                 categoryMgr.put_void()))) {
    for (const GUID* cat : kCategories) {
      categoryMgr->UnregisterCategory(CLSID_HodionKeyService, *cat,
                                      CLSID_HodionKeyService);
    }
  }
}
