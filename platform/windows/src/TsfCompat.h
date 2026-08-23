// Bù đắp các định nghĩa TSF còn thiếu trong header MinGW-w64 (dùng để
// cross-compile/CI trên Linux). Với Windows SDK thật (MSVC) các guard này
// tự vô hiệu và định nghĩa gốc của SDK được dùng.
#pragma once

#include <msctf.h>

#ifndef TF_CLIENTID_NULL
#define TF_CLIENTID_NULL ((TfClientId)0)
#endif

#ifndef TF_INVALID_GUIDATOM
#define TF_INVALID_GUIDATOM ((TfGuidAtom)0)
#endif

#ifndef __ITfTextInputProcessorEx_INTERFACE_DEFINED__
#define __ITfTextInputProcessorEx_INTERFACE_DEFINED__
MIDL_INTERFACE("6e4e2102-f9cd-433d-b496-303ce03a6507")
ITfTextInputProcessorEx : public ITfTextInputProcessor {
 public:
  virtual HRESULT STDMETHODCALLTYPE ActivateEx(ITfThreadMgr* ptim,
                                               TfClientId tid,
                                               DWORD dwFlags) = 0;
};
#endif  // __ITfTextInputProcessorEx_INTERFACE_DEFINED__

#ifndef __ITfDisplayAttributeProvider_INTERFACE_DEFINED__
#define __ITfDisplayAttributeProvider_INTERFACE_DEFINED__
MIDL_INTERFACE("fee47777-163c-4769-996a-6e9c50ad8f54")
ITfDisplayAttributeProvider : public IUnknown {
 public:
  virtual HRESULT STDMETHODCALLTYPE EnumDisplayAttributeInfo(
      IEnumTfDisplayAttributeInfo * *ppEnum) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetDisplayAttributeInfo(
      REFGUID guid, ITfDisplayAttributeInfo * *ppInfo) = 0;
};
#endif  // __ITfDisplayAttributeProvider_INTERFACE_DEFINED__

// IID khai báo cục bộ để không phụ thuộc uuid.lib của từng toolchain.
constexpr IID kIID_ITfTextInputProcessorEx = {
    0x6e4e2102, 0xf9cd, 0x433d, {0xb4, 0x96, 0x30, 0x3c, 0xe0, 0x3a, 0x65, 0x07}};
constexpr IID kIID_ITfDisplayAttributeProvider = {
    0xfee47777, 0x163c, 0x4769, {0x99, 0x6a, 0x6e, 0x9c, 0x50, 0xad, 0x8f, 0x54}};
