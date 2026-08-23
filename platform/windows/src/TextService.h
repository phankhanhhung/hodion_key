#pragma once

#include <functional>
#include <string>

#include "HodionTsf.h"
#include "hodion/engine.h"

// Text service chính: một instance được TSF tạo cho mỗi thread có bàn phím
// HodionKey được kích hoạt. Toàn bộ thao tác sửa văn bản đi qua edit session
// đồng bộ xin từ key event sink — đúng mô hình luồng của TSF.
std::wstring HodionToWide(const std::u32string& s);

class CTextService : public ITfTextInputProcessorEx,
                     public ITfThreadMgrEventSink,
                     public ITfKeyEventSink,
                     public ITfCompositionSink,
                     public ITfDisplayAttributeProvider {
 public:
  CTextService();

  // IUnknown
  STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP_(ULONG) Release() override;

  // ITfTextInputProcessor / Ex
  STDMETHODIMP Activate(ITfThreadMgr* ptim, TfClientId tid) override;
  STDMETHODIMP ActivateEx(ITfThreadMgr* ptim, TfClientId tid,
                          DWORD dwFlags) override;
  STDMETHODIMP Deactivate() override;

  // ITfThreadMgrEventSink
  STDMETHODIMP OnInitDocumentMgr(ITfDocumentMgr* pdim) override;
  STDMETHODIMP OnUninitDocumentMgr(ITfDocumentMgr* pdim) override;
  STDMETHODIMP OnSetFocus(ITfDocumentMgr* pdimFocus,
                          ITfDocumentMgr* pdimPrevFocus) override;
  STDMETHODIMP OnPushContext(ITfContext* pic) override;
  STDMETHODIMP OnPopContext(ITfContext* pic) override;

  // ITfKeyEventSink
  STDMETHODIMP OnSetFocus(BOOL fForeground) override;
  STDMETHODIMP OnTestKeyDown(ITfContext* pic, WPARAM wParam, LPARAM lParam,
                             BOOL* pfEaten) override;
  STDMETHODIMP OnKeyDown(ITfContext* pic, WPARAM wParam, LPARAM lParam,
                         BOOL* pfEaten) override;
  STDMETHODIMP OnTestKeyUp(ITfContext* pic, WPARAM wParam, LPARAM lParam,
                           BOOL* pfEaten) override;
  STDMETHODIMP OnKeyUp(ITfContext* pic, WPARAM wParam, LPARAM lParam,
                       BOOL* pfEaten) override;
  STDMETHODIMP OnPreservedKey(ITfContext* pic, REFGUID rguid,
                              BOOL* pfEaten) override;

  // ITfCompositionSink
  STDMETHODIMP OnCompositionTerminated(TfEditCookie ecWrite,
                                       ITfComposition* pComposition) override;

  // ITfDisplayAttributeProvider
  STDMETHODIMP EnumDisplayAttributeInfo(
      IEnumTfDisplayAttributeInfo** ppEnum) override;
  STDMETHODIMP GetDisplayAttributeInfo(
      REFGUID guid, ITfDisplayAttributeInfo** ppInfo) override;

 private:
  ~CTextService();

  // Quyết định số phận một phím — dùng chung cho OnTestKeyDown/OnKeyDown để
  // hai bên luôn trả lời nhất quán với ứng dụng.
  enum class KeyDisposition {
    NotOurs,             // không đụng tới
    FinalizeAndForward,  // chốt composition đang dở rồi cho phím đi tiếp
    Eat,                 // engine xử lý, nuốt phím
  };
  KeyDisposition ClassifyKey(WPARAM wParam, wchar_t* outChar) const;
  HRESULT HandleEatenKey(ITfContext* pic, WPARAM wParam, wchar_t ch);

  HRESULT RequestSyncEdit(ITfContext* pic,
                          const std::function<HRESULT(TfEditCookie)>& fn);
  HRESULT EnsureComposition(TfEditCookie ec, ITfContext* pic);
  HRESULT SetCompositionText(TfEditCookie ec, const std::wstring& text);
  HRESULT EndCompositionKeepText(TfEditCookie ec);
  // Chốt composition đang dở (giữ nguyên chữ trên màn hình), reset engine.
  void FinalizeComposition();
  void AbandonComposition();

  void LoadSettings();

  LONG refCount_ = 1;
  com_ptr<ITfThreadMgr> threadMgr_;
  TfClientId clientId_ = TF_CLIENTID_NULL;
  DWORD threadMgrEventSinkCookie_ = TF_INVALID_COOKIE;
  bool keySinkAdvised_ = false;

  com_ptr<ITfComposition> composition_;
  com_ptr<ITfContext> compositionContext_;
  TfGuidAtom displayAttributeAtom_ = TF_INVALID_GUIDATOM;

  hodion::Engine engine_;
};
