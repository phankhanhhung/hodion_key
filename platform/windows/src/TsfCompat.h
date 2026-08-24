// Bù đắp các định nghĩa TSF còn thiếu trong header MinGW-w64 (dùng để
// cross-compile/CI trên Linux). Với Windows SDK thật (MSVC) các guard này
// tự vô hiệu và định nghĩa gốc của SDK được dùng.
#pragma once

#include <msctf.h>

// ctffunc.h (reconversion + danh sách phương án) có trong Windows SDK nhưng
// KHÔNG có trong MinGW-w64. Dùng bản của SDK khi có, tự khai khi không.
#if defined(__has_include)
#  if __has_include(<ctffunc.h>)
#    include <ctffunc.h>
#    define HODION_HAVE_CTFFUNC 1
#  endif
#endif

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

#ifndef HODION_HAVE_CTFFUNC

typedef enum {
  CAND_FINALIZED = 0,
  CAND_SELECTED = 1,
  CAND_CANCELED = 2
} TfCandidateResult;

#ifndef __ITfFunction_INTERFACE_DEFINED__
#define __ITfFunction_INTERFACE_DEFINED__
MIDL_INTERFACE("db593490-098f-11d3-8df0-00105a2799b5")
ITfFunction : public IUnknown {
 public:
  virtual HRESULT STDMETHODCALLTYPE GetDisplayName(BSTR * pbstrName) = 0;
};
#endif

#ifndef __ITfCandidateString_INTERFACE_DEFINED__
#define __ITfCandidateString_INTERFACE_DEFINED__
MIDL_INTERFACE("581f317e-fd9d-443f-b972-ed00467c5d40")
ITfCandidateString : public IUnknown {
 public:
  virtual HRESULT STDMETHODCALLTYPE GetString(BSTR * pbstr) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetIndex(ULONG * pnIndex) = 0;
};
#endif

#ifndef __IEnumTfCandidates_INTERFACE_DEFINED__
#define __IEnumTfCandidates_INTERFACE_DEFINED__
MIDL_INTERFACE("defb1926-6c80-4ce8-87d1-9b5b7a68e28d")
IEnumTfCandidates : public IUnknown {
 public:
  virtual HRESULT STDMETHODCALLTYPE Clone(IEnumTfCandidates * *ppEnum) = 0;
  virtual HRESULT STDMETHODCALLTYPE Next(ULONG ulCount,
                                         ITfCandidateString * *ppCand,
                                         ULONG * pcFetched) = 0;
  virtual HRESULT STDMETHODCALLTYPE Reset() = 0;
  virtual HRESULT STDMETHODCALLTYPE Skip(ULONG ulCount) = 0;
};
#endif

#ifndef __ITfCandidateList_INTERFACE_DEFINED__
#define __ITfCandidateList_INTERFACE_DEFINED__
MIDL_INTERFACE("a3ad50fb-9bdb-49e3-a843-6c76520fbf5d")
ITfCandidateList : public IUnknown {
 public:
  virtual HRESULT STDMETHODCALLTYPE EnumCandidates(
      IEnumTfCandidates * *ppEnum) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetCandidate(
      ULONG nIndex, ITfCandidateString * *ppCand) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetCandidateNum(ULONG * pnCnt) = 0;
  virtual HRESULT STDMETHODCALLTYPE SetResult(ULONG nIndex,
                                              TfCandidateResult imcr) = 0;
};
#endif

#ifndef __ITfFnReconversion_INTERFACE_DEFINED__
#define __ITfFnReconversion_INTERFACE_DEFINED__
MIDL_INTERFACE("4cea93c0-0a58-11d3-8df0-00105a2799b5")
ITfFnReconversion : public ITfFunction {
 public:
  virtual HRESULT STDMETHODCALLTYPE QueryRange(ITfRange * pRange,
                                               ITfRange * *ppNewRange,
                                               BOOL * pfConvertable) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetReconversion(
      ITfRange * pRange, ITfCandidateList * *ppCandList) = 0;
  virtual HRESULT STDMETHODCALLTYPE Reconvert(ITfRange * pRange) = 0;
};
#endif

#endif  // HODION_HAVE_CTFFUNC

// IID của ctffunc: dùng hằng của SDK khi có (chắc chắn đúng), tự khai khi
// không — bản MinGW chỉ phục vụ cross-compile/CI, không phải bản phát hành.
#ifdef HODION_HAVE_CTFFUNC
#define kIID_ITfFnReconversion IID_ITfFnReconversion
#define kIID_ITfCandidateList IID_ITfCandidateList
#define kIID_ITfCandidateString IID_ITfCandidateString
#define kIID_IEnumTfCandidates IID_IEnumTfCandidates
#else
constexpr IID kIID_ITfFnReconversion = {
    0x4cea93c0, 0x0a58, 0x11d3,
    {0x8d, 0xf0, 0x00, 0x10, 0x5a, 0x27, 0x99, 0xb5}};
constexpr IID kIID_ITfCandidateList = {
    0xa3ad50fb, 0x9bdb, 0x49e3,
    {0xa8, 0x43, 0x6c, 0x76, 0x52, 0x0f, 0xbf, 0x5d}};
constexpr IID kIID_ITfCandidateString = {
    0x581f317e, 0xfd9d, 0x443f,
    {0xb9, 0x72, 0xed, 0x00, 0x46, 0x7c, 0x5d, 0x40}};
constexpr IID kIID_IEnumTfCandidates = {
    0xdefb1926, 0x6c80, 0x4ce8,
    {0x87, 0xd1, 0x9b, 0x5b, 0x7a, 0x68, 0xe2, 0x8d}};
#endif

// IID khai báo cục bộ để không phụ thuộc uuid.lib của từng toolchain.
constexpr IID kIID_ITfTextInputProcessorEx = {
    0x6e4e2102, 0xf9cd, 0x433d, {0xb4, 0x96, 0x30, 0x3c, 0xe0, 0x3a, 0x65, 0x07}};
constexpr IID kIID_ITfDisplayAttributeProvider = {
    0xfee47777, 0x163c, 0x4769, {0x99, 0x6a, 0x6e, 0x9c, 0x50, 0xad, 0x8f, 0x54}};
constexpr IID kIID_ITfCompartmentEventSink = {
    0x743abd5f, 0xf26d, 0x48df, {0x8c, 0xc5, 0x23, 0x84, 0x92, 0x41, 0x9b, 0x64}};
constexpr IID kIID_ITfCompartmentMgr = {
    0x7dcf57ac, 0x18ad, 0x438b, {0x82, 0x4d, 0x97, 0x9b, 0xff, 0xb7, 0x4b, 0x7c}};
constexpr IID kIID_ITfCompartment = {
    0xbb08f7a9, 0x607a, 0x4384, {0x86, 0x23, 0x05, 0x68, 0x92, 0xb6, 0x43, 0x71}};
