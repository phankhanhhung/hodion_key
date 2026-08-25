// Nhờ tiến trình nền thêm dấu cho từ vừa chốt.
//
// Chỗ này nằm trên đường gõ, nên luật số một là **không được chờ lâu**.
// Host chưa chạy, đã tắt, hay đang treo thì hàm này trả lại nguyên văn
// gần như tức thì. Không có tính năng còn hơn gõ bị khựng.
#include <string>

#include "TextService.h"
#include "hodion/english_words.h"
#include "hodion/utf.h"

namespace {

// Âm tiết tiếng Việt dài nhất là 7 ký tự; lấy 8 cho chắc.
constexpr size_t kMaxWordChars = 8;

// Mô hình là 3-gram nên hai âm tiết trước là đủ; giữ thêm cũng vô ích.
constexpr size_t kContextWords = 2;

// Ký tự kết câu: qua đây thì ngữ cảnh cũ không còn liên quan.
bool EndsSentence(wchar_t c) {
  return c == L'.' || c == L'!' || c == L'?' || c == L';' || c == L':' ||
         c == L'\n' || c == L'\r' || c == L'\t';
}

// Chờ tối đa ngần này cho một lần hỏi. Chỉ xảy ra lúc chốt từ (không phải
// mỗi phím) và trong máy nên thực tế là vài chục micro giây; con số này chỉ
// là chặn trên cho trường hợp host kẹt.
constexpr DWORD kTimeoutMs = 20;

bool IsAsciiLetter(wchar_t c) {
  return (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z');
}

std::u32string ToU32Lower(const std::wstring& s) {
  std::u32string out;
  out.reserve(s.size());
  for (wchar_t c : s) {
    out.push_back(static_cast<char32_t>(c >= L'A' && c <= L'Z' ? c + 32 : c));
  }
  return out;
}

}  // namespace

std::wstring CTextService::MaybeRestoreDiacritics(const std::wstring& text) {
  if (text.empty()) return text;

  // Chuỗi chốt thường kèm ký tự ngắt từ ở cuối ("nguyet "). Tách ra, xử lý
  // phần chữ, rồi nối lại y nguyên.
  size_t end = text.size();
  while (end > 0 && !IsAsciiLetter(text[end - 1])) --end;

  // Hết câu thì quên ngữ cảnh, dù có bật đoán dấu hay không.
  for (size_t i = end; i < text.size(); ++i) {
    if (EndsSentence(text[i])) {
      ResetPredictContext();
      break;
    }
  }
  if (!autoDiacritics_) return text;
  if (end == 0 || end > kMaxWordChars) {
    // Không phải một âm tiết: nó vẫn cắt mạch ngữ cảnh.
    ResetPredictContext();
    return text;
  }

  const std::wstring word = text.substr(0, end);
  const std::wstring rest = text.substr(end);

  // Ghi lại từ vừa chốt làm ngữ cảnh cho từ sau, dù có đổi được hay không.
  const auto remember = [this](const std::wstring& committed) {
    recentWords_.push_back(committed);
    if (recentWords_.size() > kContextWords) {
      recentWords_.erase(recentWords_.begin());
    }
  };

  // Chỉ xét chữ toàn ASCII: có dấu rồi thì người dùng đã nói rõ họ muốn gì.
  for (wchar_t c : word) {
    if (!IsAsciiLetter(c)) {
      remember(word);
      return text;
    }
  }

  // Từ tiếng Anh đã biết thì để yên — người ta gõ tiếng Anh thật, và tầng
  // trước đã cố ý trả lại nguyên chữ cho nó.
  const std::u32string lower = ToU32Lower(word);
  if (lower.size() >= 4 && hodion::english_words().contains(lower)) {
    remember(word);
    return text;
  }

  // Gói tin: chữ vừa gõ, rồi ngữ cảnh trái (cũ nhất trước).
  std::string payload =
      hodion::utf::to_utf8(std::u32string(word.begin(), word.end()));
  for (const std::wstring& w : recentWords_) {
    payload += '\t';
    payload += hodion::utf::to_utf8(std::u32string(w.begin(), w.end()));
  }

  std::string reply;
  if (!hostClient_.Request(hodionipc::Op::Restore, payload, &reply,
                           kTimeoutMs)) {
    remember(word);
    return text;  // không có host, hoặc quá hạn
  }
  if (reply.empty()) {
    remember(word);
    return text;  // host không đủ bằng chứng
  }

  const std::u16string u16 = hodion::utf::to_utf16(hodion::utf::from_utf8(reply));
  const std::wstring restored(u16.begin(), u16.end());
  // Chỉ nhận nếu đúng là cùng một từ đã thêm dấu, không phải thứ gì khác.
  if (restored.size() != word.size()) {
    remember(word);
    return text;
  }
  remember(restored);
  return restored + rest;
}
