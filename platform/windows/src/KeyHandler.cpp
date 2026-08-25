// Xử lý phím: phân loại (OnTestKeyDown/OnKeyDown phải trả lời giống nhau),
// dịch virtual key → ký tự, và đẩy vào engine qua edit session đồng bộ.
#include "Log.h"
#include "TextService.h"

namespace {

bool IsKeyPressed(int vk) { return (GetKeyState(vk) & 0x8000) != 0; }

bool IsCapsLockOn() { return (GetKeyState(VK_CAPITAL) & 0x0001) != 0; }

// Modifier đang giữ, theo cách đánh số của TSF.
UINT CurrentMods() {
  UINT mods = 0;
  if (IsKeyPressed(VK_CONTROL)) mods |= TF_MOD_CONTROL;
  if (IsKeyPressed(VK_MENU)) mods |= TF_MOD_ALT;
  if (IsKeyPressed(VK_SHIFT)) mods |= TF_MOD_SHIFT;
  return mods;
}

// Khớp CHÍNH XÁC: Ctrl+Shift+Backspace không được tính là Ctrl+Backspace.
bool Matches(const ToggleKey& key, WPARAM vk) {
  return key.valid() && key.vk == vk && CurrentMods() == key.mods;
}

// Dịch virtual key → ký tự theo sơ đồ QWERTY-US (layout nền phổ biến của
// người gõ tiếng Việt). Trả về 0 nếu phím không sinh ký tự in được.
wchar_t VkToChar(WPARAM vk) {
  const bool shift = IsKeyPressed(VK_SHIFT);

  if (vk >= 'A' && vk <= 'Z') {
    const bool upper = shift != IsCapsLockOn();
    return static_cast<wchar_t>(upper ? vk : vk + 32);
  }
  if (vk >= '0' && vk <= '9') {
    static const wchar_t kShifted[] = L")!@#$%^&*(";
    return shift ? kShifted[vk - '0'] : static_cast<wchar_t>(vk);
  }
  if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
    return static_cast<wchar_t>(L'0' + (vk - VK_NUMPAD0));
  }

  switch (vk) {
    case VK_SPACE: return L' ';
    case VK_OEM_1: return shift ? L':' : L';';
    case VK_OEM_2: return shift ? L'?' : L'/';
    case VK_OEM_3: return shift ? L'~' : L'`';
    case VK_OEM_4: return shift ? L'{' : L'[';
    case VK_OEM_5: return shift ? L'|' : L'\\';
    case VK_OEM_6: return shift ? L'}' : L']';
    case VK_OEM_7: return shift ? L'"' : L'\'';
    case VK_OEM_COMMA: return shift ? L'<' : L',';
    case VK_OEM_PERIOD: return shift ? L'>' : L'.';
    case VK_OEM_MINUS: return shift ? L'_' : L'-';
    case VK_OEM_PLUS: return shift ? L'+' : L'=';
    case VK_MULTIPLY: return L'*';
    case VK_ADD: return L'+';
    case VK_SUBTRACT: return L'-';
    case VK_DIVIDE: return L'/';
    case VK_DECIMAL: return L'.';
    default: return 0;
  }
}

}  // namespace

CTextService::KeyDisposition CTextService::ClassifyKey(WPARAM wParam,
                                                       wchar_t* outChar) const {
  *outChar = 0;
  const bool composing = engine_.composing();

  // Tắt tiếng Việt, hoặc ô nhập tự khai là URL/email/mật khẩu/số: mọi phím
  // đi thẳng tới ứng dụng (phím chuyển được TSF giao riêng qua
  // OnPreservedKey nên không đi qua đây).
  if (!vietnamese_ || scopeRaw_) {
    return composing ? KeyDisposition::FinalizeAndForward
                     : KeyDisposition::NotOurs;
  }

  // Phím hủy biến đổi khi đang gõ dở: bỏ dấu cho riêng từ này rồi gõ thẳng
  // phần còn lại ("deadline", "test"…). Chỉ chiếm phím lúc đang composing
  // nên tổ hợp đó của ứng dụng (mặc định Ctrl+Backspace = xóa một từ) vẫn
  // nguyên vẹn khi không gõ dở.
  if (composing && !engine_.literal() && Matches(cancelKey_, wParam)) {
    return KeyDisposition::CancelTransform;
  }

  // Tổ hợp Ctrl/Alt (hotkey của ứng dụng): không can thiệp,
  // nhưng từ đang gõ dở phải được chốt trước khi hotkey chạy.
  if (IsKeyPressed(VK_CONTROL) || IsKeyPressed(VK_MENU)) {
    return composing ? KeyDisposition::FinalizeAndForward
                     : KeyDisposition::NotOurs;
  }

  switch (wParam) {
    case VK_SHIFT:
    case VK_CAPITAL:
      return KeyDisposition::NotOurs;
    case VK_BACK:
    case VK_ESCAPE:
      return composing ? KeyDisposition::Eat : KeyDisposition::NotOurs;
    default:
      break;
  }

  const wchar_t ch = VkToChar(wParam);
  if (ch != 0) {
    *outChar = ch;
    if (composing) return KeyDisposition::Eat;  // engine ghép tiếp hoặc chốt kèm ký tự
    // Chưa compose: chỉ nuốt phím mở từ (chữ cái; [ ] { } ở Telex đầy đủ).
    return engine_.starts_word(static_cast<char32_t>(ch))
               ? KeyDisposition::Eat
               : KeyDisposition::NotOurs;
  }

  // Phím điều khiển khác (Enter, Tab, mũi tên, Delete, Home…):
  // chốt từ dở rồi để phím hoạt động bình thường.
  return composing ? KeyDisposition::FinalizeAndForward
                   : KeyDisposition::NotOurs;
}

HRESULT CTextService::HandleEatenKey(ITfContext* pic, WPARAM wParam,
                                     wchar_t ch) {
  if (wParam == VK_BACK) {
    const auto r = engine_.process_backspace();
    if (r.action == hodion::Engine::Result::Action::None) return S_OK;
    const std::wstring text = HodionToWide(r.text);
    const bool stillComposing = engine_.composing();
    return RequestSyncEdit(pic, [&](TfEditCookie ec) {
      HRESULT hr = SetCompositionText(ec, text);
      if (SUCCEEDED(hr) && !stillComposing) hr = EndCompositionKeepText(ec);
      return hr;
    });
  }

  if (wParam == VK_ESCAPE) {
    // Esc: trả lại đúng chuỗi phím thô (hủy mọi biến đổi tiếng Việt). Nếu từ
    // đã bị sửa bằng Backspace thì engine trả về chính chữ đang hiển thị,
    // nên Esc chỉ kết thúc composition.
    const std::wstring rawText = HodionToWide(engine_.raw());
    engine_.reset();
    return RequestSyncEdit(pic, [&](TfEditCookie ec) {
      HRESULT hr = SetCompositionText(ec, rawText);
      if (SUCCEEDED(hr)) hr = EndCompositionKeepText(ec);
      return hr;
    });
  }

  // Người dùng đã chủ động huỷ biến đổi cho từ này thì đừng đụng vào nó nữa.
  const bool wasLiteral = engine_.literal();
  const auto r = engine_.process_char(static_cast<char32_t>(ch));
  switch (r.action) {
    case hodion::Engine::Result::Action::Composing: {
      const std::wstring text = HodionToWide(r.text);
      const HRESULT hr = RequestSyncEdit(pic, [&](TfEditCookie ec) {
        HRESULT hrInner = EnsureComposition(ec, pic);
        if (SUCCEEDED(hrInner)) hrInner = SetCompositionText(ec, text);
        return hrInner;
      });
      // Không mở được composition (ngữ cảnh không cho sửa): bỏ trạng thái
      // gõ dở để engine không lệch khỏi những gì ứng dụng hiển thị.
      if (FAILED(hr) && !composition_) engine_.reset();
      return hr;
    }
    case hodion::Engine::Result::Action::Commit: {
      const std::wstring raw = HodionToWide(r.text);
      const std::wstring text =
          wasLiteral ? raw : MaybeRestoreDiacritics(raw);
      return RequestSyncEdit(pic, [&](TfEditCookie ec) {
        HRESULT hr = EnsureComposition(ec, pic);
        if (SUCCEEDED(hr)) hr = SetCompositionText(ec, text);
        if (SUCCEEDED(hr)) hr = EndCompositionKeepText(ec);
        return hr;
      });
    }
    case hodion::Engine::Result::Action::None:
    default:
      return S_OK;
  }
}

HRESULT CTextService::HandleCancelTransform(ITfContext* pic) {
  const auto r = engine_.cancel_transform();
  if (r.action != hodion::Engine::Result::Action::Composing) return S_OK;
  const std::wstring text = HodionToWide(r.text);
  // Composition vẫn mở: người dùng gõ tiếp phần còn lại của từ tiếng Anh.
  return RequestSyncEdit(
      pic, [&](TfEditCookie ec) { return SetCompositionText(ec, text); });
}

// ---- ITfKeyEventSink ------------------------------------------------------

STDMETHODIMP CTextService::OnSetFocus(BOOL fForeground) {
  if (!fForeground) FinalizeComposition();
  return S_OK;
}

STDMETHODIMP CTextService::OnTestKeyDown(ITfContext* pic, WPARAM wParam,
                                         LPARAM /*lParam*/, BOOL* pfEaten) {
  if (!pfEaten) return E_INVALIDARG;
  // Hỏi kiểu ô nhập một lần cho mỗi lần đổi focus/context. OnKeyDown luôn
  // đi sau OnTestKeyDown nên hai bên nhìn thấy cùng một câu trả lời.
  RefreshInputScope(pic);
  wchar_t ch = 0;
  const KeyDisposition d = ClassifyKey(wParam, &ch);
  if (d == KeyDisposition::FinalizeAndForward) {
    // Phím sẽ đi thẳng tới app nên OnKeyDown không được gọi nữa —
    // phải chốt composition ngay tại đây.
    FinalizeComposition();
  }
  *pfEaten = (d == KeyDisposition::Eat ||
              d == KeyDisposition::CancelTransform);
  return S_OK;
}

STDMETHODIMP CTextService::OnKeyDown(ITfContext* pic, WPARAM wParam,
                                     LPARAM /*lParam*/, BOOL* pfEaten) {
  if (!pfEaten) return E_INVALIDARG;
  // Giữa các từ là thời điểm an toàn để nạp cấu hình vừa đổi.
  if (!engine_.composing()) ReloadSettingsIfChanged();
  RefreshInputScope(pic);

  wchar_t ch = 0;
  const KeyDisposition d = ClassifyKey(wParam, &ch);
  switch (d) {
    case KeyDisposition::Eat:
      *pfEaten = TRUE;
      return HandleEatenKey(pic, wParam, ch);
    case KeyDisposition::CancelTransform:
      *pfEaten = TRUE;
      return HandleCancelTransform(pic);
    case KeyDisposition::FinalizeAndForward:
      FinalizeComposition();
      *pfEaten = FALSE;
      return S_OK;
    case KeyDisposition::NotOurs:
    default:
      *pfEaten = FALSE;
      return S_OK;
  }
}

STDMETHODIMP CTextService::OnTestKeyUp(ITfContext*, WPARAM, LPARAM,
                                       BOOL* pfEaten) {
  if (!pfEaten) return E_INVALIDARG;
  *pfEaten = FALSE;
  return S_OK;
}

STDMETHODIMP CTextService::OnKeyUp(ITfContext*, WPARAM, LPARAM,
                                   BOOL* pfEaten) {
  if (!pfEaten) return E_INVALIDARG;
  *pfEaten = FALSE;
  return S_OK;
}

STDMETHODIMP CTextService::OnPreservedKey(ITfContext* pic, REFGUID rguid,
                                          BOOL* pfEaten) {
  if (!pfEaten) return E_INVALIDARG;
  *pfEaten = TRUE;

  if (IsEqualGUID(rguid, GUID_HodionKeyToggle)) {
    SetVietnamese(!vietnamese_, /*persist=*/true);
    return S_OK;
  }

  if (IsEqualGUID(rguid, GUID_HodionKeyMethod)) {
    // Đổi kiểu gõ giữa chừng thì từ đang dở phải chốt trước, nếu không nửa
    // đầu gõ theo luật này còn nửa sau theo luật kia.
    FinalizeComposition();
    hodion::Config cfg = engine_.config();
    cfg.method = cfg.method == hodion::InputMethod::Telex
                     ? hodion::InputMethod::Vni
                     : hodion::InputMethod::Telex;
    engine_.set_config(cfg);
    SaveHodionInputMethod(cfg.method);
    watcher_.poll();  // nuốt lượt báo do chính ta vừa ghi
    return S_OK;
  }

  if (IsEqualGUID(rguid, GUID_HodionKeyCycle)) {
    HODION_LOG(L"phím xoay dấu: pic=%p", static_cast<void*>(pic));
    return CycleWordDiacritics(pic);
  }

  if (IsEqualGUID(rguid, GUID_HodionKeyPredict)) {
    autoDiacritics_ = !autoDiacritics_;
    ResetPredictContext();
    SaveHodionAutoDiacritics(autoDiacritics_);
    watcher_.poll();
    return S_OK;
  }

  *pfEaten = FALSE;
  return S_OK;
}
