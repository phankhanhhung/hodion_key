#pragma once

#include <functional>
#include <string>
#include <vector>

#include "HodionTsf.h"
#include "HostClient.h"
#include "Settings.h"
#include "hodion/engine.h"

std::wstring HodionToWide(const std::u32string& s);

// Text service chính: một instance được TSF tạo cho mỗi thread có bàn phím
// HodionKey được kích hoạt. Toàn bộ thao tác sửa văn bản đi qua edit session
// đồng bộ xin từ key event sink — đúng mô hình luồng của TSF.
class CTextService : public ITfTextInputProcessorEx,
                     public ITfThreadMgrEventSink,
                     public ITfKeyEventSink,
                     public ITfCompositionSink,
                     public ITfCompartmentEventSink,
                     public ITfDisplayAttributeProvider,
                     public ITfFunctionProvider,
                     public ITfFnReconversion {
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

  // ITfCompartmentEventSink — người dùng bật/tắt IME từ thanh ngôn ngữ.
  STDMETHODIMP OnChange(REFGUID rguid) override;

  // ITfDisplayAttributeProvider
  STDMETHODIMP EnumDisplayAttributeInfo(
      IEnumTfDisplayAttributeInfo** ppEnum) override;
  STDMETHODIMP GetDisplayAttributeInfo(
      REFGUID guid, ITfDisplayAttributeInfo** ppInfo) override;

  // ITfFunctionProvider — cửa để ứng dụng lấy ITfFnReconversion.
  STDMETHODIMP GetType(GUID* pguid) override;
  STDMETHODIMP GetDescription(BSTR* pbstrDesc) override;
  STDMETHODIMP GetFunction(REFGUID rguid, REFIID riid,
                           IUnknown** ppunk) override;

  // ITfFunction / ITfFnReconversion — sửa dấu cho chữ ĐÃ chốt.
  STDMETHODIMP GetDisplayName(BSTR* pbstrName) override;
  STDMETHODIMP QueryRange(ITfRange* pRange, ITfRange** ppNewRange,
                          BOOL* pfConvertable) override;
  STDMETHODIMP GetReconversion(ITfRange* pRange,
                               ITfCandidateList** ppCandList) override;
  STDMETHODIMP Reconvert(ITfRange* pRange) override;

  // Dùng bởi danh sách phương án của reconversion (Reconversion.cpp).
  // Chỉ ghi khi văn bản trong `range` vẫn đúng bằng `expect` — người dùng
  // có thể đã gõ tiếp kể từ lúc danh sách được dựng.
  HRESULT ApplyReconversion(ITfContext* pic, ITfRange* range,
                            const std::wstring& expect,
                            const std::wstring& replacement);

 private:
  ~CTextService();

  // Quyết định số phận một phím — dùng chung cho OnTestKeyDown/OnKeyDown để
  // hai bên luôn trả lời nhất quán với ứng dụng.
  enum class KeyDisposition {
    NotOurs,             // không đụng tới
    FinalizeAndForward,  // chốt composition đang dở rồi cho phím đi tiếp
    Eat,                 // engine xử lý, nuốt phím
    CancelTransform,     // Ctrl+Backspace khi đang gõ dở: hủy biến đổi
  };
  KeyDisposition ClassifyKey(WPARAM wParam, wchar_t* outChar) const;
  HRESULT HandleEatenKey(ITfContext* pic, WPARAM wParam, wchar_t ch);
  HRESULT HandleCancelTransform(ITfContext* pic);

  HRESULT RequestSyncEdit(ITfContext* pic, DWORD flags,
                          const std::function<HRESULT(TfEditCookie)>& fn);
  HRESULT RequestSyncEdit(ITfContext* pic,
                          const std::function<HRESULT(TfEditCookie)>& fn) {
    return RequestSyncEdit(pic, TF_ES_SYNC | TF_ES_READWRITE, fn);
  }
  HRESULT EnsureComposition(TfEditCookie ec, ITfContext* pic);
  HRESULT SetCompositionText(TfEditCookie ec, const std::wstring& text);
  HRESULT EndCompositionKeepText(TfEditCookie ec);
  // Chốt composition đang dở (giữ nguyên chữ trên màn hình), reset engine.
  void FinalizeComposition();
  void AbandonComposition();

  // --- Reconversion (Reconversion.cpp) ---
  // Tìm đúng đoạn văn bản sẽ đổi: đoạn đang bôi đen, hoặc từ mà con trỏ
  // đang đứng trong đó. Trả về E_FAIL khi không có gì đổi được.
  HRESULT FindReconvertRange(TfEditCookie ec, ITfRange* pRange,
                             ITfRange** ppWord, std::wstring* outText);
  std::vector<std::wstring> ReconvertCandidates(const std::wstring& word) const;
  // Xoay từ ngay trước con trỏ sang phương án dấu kế tiếp.
  HRESULT CycleWordDiacritics(ITfContext* pic);
  // Hỏi tiến trình nền danh sách phương án đã xếp hạng theo ngữ cảnh. Hỏng
  // thì trả về rỗng — người gọi tự lo bằng danh sách cục bộ.
  std::vector<std::wstring> HostCandidates(const std::wstring& word);

  // --- Đoán dấu qua tiến trình nền (Predict.cpp) ---
  // Trả về chuỗi đã thêm dấu, hoặc chính `text` khi không đổi gì. Không bao
  // giờ chờ lâu: host chưa chạy thì trả lại nguyên văn ngay.
  std::wstring MaybeRestoreDiacritics(const std::wstring& text);
  // Quên ngữ cảnh câu (đổi ô nhập, hết câu, tắt tiếng Việt). Ghép nhầm ngữ
  // cảnh của câu trước vào câu sau thì mô hình đoán còn tệ hơn không có.
  void ResetPredictContext() { recentWords_.clear(); }

  // --- Ngữ cảnh nhập liệu (InputScope.cpp) ---
  // Ô đang gõ có tự khai là URL / email / mật khẩu / số không? Hỏi lại một
  // lần sau mỗi lần đổi focus hoặc đổi context, không hỏi mỗi phím.
  bool QueryRawInputScope(ITfContext* pic);
  void RefreshInputScope(ITfContext* pic);
  void InvalidateInputScope() { scopeKnown_ = false; }

  // --- Cấu hình & bật/tắt tiếng Việt ---
  void ApplySettings(const HodionSettings& s);
  void ReloadSettingsIfChanged();
  void SetVietnamese(bool on, bool persist);
  // Đăng ký/gỡ toàn bộ phím tắt kiểu preserved key. Phím hủy biến đổi
  // KHÔNG nằm trong này: nó chỉ có hiệu lực khi đang gõ dở nên được xử lý
  // trong ClassifyKey, đăng ký sẽ chiếm mất tổ hợp đó của ứng dụng.
  void RegisterHotKeys();
  void UnregisterHotKeys();
  void RegisterOneKey(REFGUID guid, const ToggleKey& key,
                      const WCHAR* description);
  // Đồng bộ compartment OPENCLOSE (thanh ngôn ngữ, chỉ báo hệ thống).
  void PushOpenCloseCompartment();
  HRESULT GetOpenCloseCompartment(ITfCompartment** out) const;

  LONG refCount_ = 1;
  com_ptr<ITfThreadMgr> threadMgr_;
  TfClientId clientId_ = TF_CLIENTID_NULL;
  DWORD threadMgrEventSinkCookie_ = TF_INVALID_COOKIE;
  DWORD openCloseSinkCookie_ = TF_INVALID_COOKIE;
  bool keySinkAdvised_ = false;

  com_ptr<ITfComposition> composition_;
  com_ptr<ITfContext> compositionContext_;
  TfGuidAtom displayAttributeAtom_ = TF_INVALID_GUIDATOM;

  hodion::Engine engine_;
  SettingsWatcher watcher_;
  ToggleKey toggleKey_{};
  ToggleKey methodKey_{};
  ToggleKey predictKey_{};
  ToggleKey cancelKey_{};
  ToggleKey cycleKey_{};
  bool keysRegistered_ = false;
  bool vietnamese_ = true;
  bool skipInputScopes_ = true;
  bool autoDiacritics_ = false;
  HostClient hostClient_;
  // Vài âm tiết đã chốt ngay trước, làm ngữ cảnh cho mô hình (cũ nhất
  // trước). Chỉ nhìn sang trái: chữ đã chốt là chốt, không sửa ngược.
  std::vector<std::wstring> recentWords_;
  bool scopeRaw_ = false;    // ô hiện tại không nên gõ tiếng Việt
  bool scopeKnown_ = false;  // đã hỏi ITfInputScope cho ô hiện tại chưa
  // Chặn vòng lặp khi chính ta ghi vào compartment OPENCLOSE.
  bool updatingCompartment_ = false;
};
