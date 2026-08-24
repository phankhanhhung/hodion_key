cmake_minimum_required(VERSION 3.16)
project(hodion_key LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

add_subdirectory(engine)
add_subdirectory(wordlist)

# Demo CLI — chỗ duy nhất ngoài các frontend ghép hai tầng lại với nhau:
# lõi engine + từ điển tiếng Anh tùy chọn. Đặt ở đây (không phải trong
# engine/) để giữ đúng chiều phụ thuộc: engine không biết gì về wordlist.
add_executable(hodion_demo engine/tools/demo.cpp)
target_link_libraries(hodion_demo PRIVATE hodion_engine hodion_words)
if(MSVC)
  target_compile_options(hodion_demo PRIVATE /utf-8)
endif()

# Tầng Windows (TSF text service) chỉ build trên Windows.
if(WIN32)
  add_subdirectory(platform/windows)
endif()
