// Ô nhập nào thì KHÔNG nên gõ tiếng Việt.
//
// Không đoán bằng heuristic: TSF cho phép hỏi thẳng ứng dụng qua
// ITfInputScope — ô địa chỉ web, ô email, ô mật khẩu, ô số… đều tự khai
// báo kiểu dữ liệu của mình. Đây là nguồn thông tin chính xác nhất và rẻ
// nhất có thể có, nên nó là tầng đầu tiên của việc gõ trộn Việt–Anh.
#include <inputscope.h>

#include "TextService.h"

namespace {

// Chỉ chặn những scope mà dấu tiếng Việt chắc chắn không thuộc về.
//
// KHÔNG chặn: IS_PERSONALNAME_* (tên người Việt rất cần dấu),
// IS_ADDRESS_CITY/STREET/… (địa chỉ Việt Nam), IS_SEARCH*, IS_CHAT,
// IS_TEXT, IS_MAPS — đó đều là chỗ người ta gõ tiếng Việt thật.
bool IsRawScope(InputScope s) {
  switch (s) {
    case IS_URL:
    case IS_FILE_FULLFILEPATH:
    case IS_FILE_FILENAME:
    case IS_EMAIL_USERNAME:
    case IS_EMAIL_SMTPEMAILADDRESS:
    case IS_EMAILNAME_OR_ADDRESS:
    case IS_LOGINNAME:
    case IS_ADDRESS_POSTALCODE:
    case IS_ADDRESS_COUNTRYSHORTNAME:
    case IS_CURRENCY_AMOUNT:
    case IS_CURRENCY_AMOUNTANDSYMBOL:
    case IS_DATE_FULLDATE:
    case IS_DATE_MONTH:
    case IS_DATE_DAY:
    case IS_DATE_YEAR:
    case IS_DIGITS:
    case IS_NUMBER:
    case IS_NUMBER_FULLWIDTH:
    case IS_ONECHAR:
    case IS_PASSWORD:
    case IS_NUMERIC_PASSWORD:
    case IS_NUMERIC_PIN:
    case IS_ALPHANUMERIC_PIN:
    case IS_ALPHANUMERIC_PIN_SET:
    case IS_TELEPHONE_FULLTELEPHONENUMBER:
    case IS_TELEPHONE_COUNTRYCODE:
    case IS_TELEPHONE_AREACODE:
    case IS_TELEPHONE_LOCALNUMBER:
    case IS_TIME_FULLTIME:
    case IS_TIME_HOUR:
    case IS_TIME_MINORSEC:
    case IS_ALPHANUMERIC_HALFWIDTH:
    case IS_ALPHANUMERIC_FULLWIDTH:
    case IS_FORMULA:
    case IS_FORMULA_NUMBER:
      return true;
    default:
      return false;
  }
}

// Một ô có thể khai báo nhiều scope cùng lúc (thanh địa chỉ trình duyệt
// vừa là IS_URL vừa là IS_SEARCH). Chỉ tắt tiếng Việt khi KHÔNG scope nào
// cho phép văn bản tự do — chặn nhầm ô người ta muốn gõ tiếng Việt tệ hơn
// là bỏ sót một ô URL.
bool ScopesBlockVietnamese(ITfInputScope* scope) {
  InputScope* list = nullptr;
  UINT count = 0;
  if (FAILED(scope->GetInputScopes(&list, &count)) || !list) return false;

  bool all_raw = count > 0;
  for (UINT i = 0; i < count && all_raw; ++i) {
    if (!IsRawScope(list[i])) all_raw = false;
  }
  CoTaskMemFree(list);
  return all_raw;
}

// IID/GUID khai báo tại chỗ để không phụ thuộc uuid.lib của từng toolchain
// (đúng giá trị trong inputscope.h của Windows SDK lẫn MinGW-w64).
constexpr IID kIID_ITfInputScope = {
    0xfde1eaee, 0x6924, 0x4cdf,
    {0x91, 0xe7, 0xda, 0x38, 0xcf, 0xf5, 0x55, 0x9d}};
constexpr GUID kGUID_PROP_INPUTSCOPE = {
    0x1713dd5a, 0x68e7, 0x4a5b,
    {0x9a, 0xf6, 0x59, 0x2a, 0x59, 0x5c, 0x77, 0x8d}};

}  // namespace

bool CTextService::QueryRawInputScope(ITfContext* pic) {
  if (!pic || !skipInputScopes_) return false;

  // Đường nhanh: nhiều ứng dụng để ITfInputScope thẳng trên context.
  {
    com_ptr<ITfInputScope> scope;
    if (SUCCEEDED(pic->QueryInterface(kIID_ITfInputScope,
                                      scope.put_void())) &&
        scope) {
      return ScopesBlockVietnamese(scope.get());
    }
  }

  // Đường chuẩn: thuộc tính GUID_PROP_INPUTSCOPE tại vị trí con trỏ. Cần
  // edit cookie nên phải chui vào một edit session chỉ đọc.
  bool blocked = false;
  RequestSyncEdit(pic, TF_ES_SYNC | TF_ES_READ, [&](TfEditCookie ec) {
    TF_SELECTION sel = {};
    ULONG fetched = 0;
    if (FAILED(pic->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &sel,
                                 &fetched)) ||
        fetched != 1) {
      return S_OK;
    }
    com_ptr<ITfRange> range;
    range.attach(sel.range);  // GetSelection trả về đã AddRef

    com_ptr<ITfReadOnlyProperty> prop;
    if (FAILED(pic->GetAppProperty(kGUID_PROP_INPUTSCOPE, prop.put())) ||
        !prop) {
      return S_OK;
    }

    VARIANT var;
    VariantInit(&var);
    if (SUCCEEDED(prop->GetValue(ec, range.get(), &var)) &&
        var.vt == VT_UNKNOWN && var.punkVal) {
      com_ptr<ITfInputScope> scope;
      if (SUCCEEDED(var.punkVal->QueryInterface(kIID_ITfInputScope,
                                                scope.put_void())) &&
          scope) {
        blocked = ScopesBlockVietnamese(scope.get());
      }
    }
    VariantClear(&var);
    return S_OK;
  });
  return blocked;
}

void CTextService::RefreshInputScope(ITfContext* pic) {
  if (scopeKnown_) return;
  scopeRaw_ = QueryRawInputScope(pic);
  scopeKnown_ = true;
}
