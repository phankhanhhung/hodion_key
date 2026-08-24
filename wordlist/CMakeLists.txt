# Từ điển tiếng Anh — tầng riêng, tùy chọn, portable.
#
# Lõi engine KHÔNG phụ thuộc thư viện này; nó chỉ cung cấp một cài đặt của
# hodion::ForeignWords để frontend nạp vào bằng Engine::set_foreign_words().
add_library(hodion_words STATIC
  src/english_words.cpp
  src/viet_words.cpp
  src/predict.cpp
)
target_include_directories(hodion_words PUBLIC include)
target_include_directories(hodion_words PRIVATE data)
target_link_libraries(hodion_words PUBLIC hodion_engine)
if(MSVC)
  target_compile_options(hodion_words PRIVATE /utf-8 /W4)
else()
  target_compile_options(hodion_words PRIVATE -Wall -Wextra)
endif()

# Test dữ liệu: kiểm lại chính tính chất an toàn của bảng (không từ nào ghi
# đè được lên một âm tiết tiếng Việt hợp lệ) trên TỪNG từ, mỗi lần chạy CI.
add_executable(hodion_wordlist_tests tests/wordlist_test.cpp)
target_link_libraries(hodion_wordlist_tests PRIVATE hodion_words)
target_include_directories(hodion_wordlist_tests PRIVATE data)
if(MSVC)
  target_compile_options(hodion_wordlist_tests PRIVATE /utf-8)
endif()
add_test(NAME wordlist_tests COMMAND hodion_wordlist_tests)
