# Lõi bộ gõ — portable, không phụ thuộc OS.
add_library(hodion_engine STATIC
  src/chartable.cpp
  src/vnlexi.cpp
  src/word.cpp
  src/engine.cpp
  src/engine_c.cpp
  src/utf.cpp
)
target_include_directories(hodion_engine PUBLIC include)
target_compile_features(hodion_engine PUBLIC cxx_std_17)
if(MSVC)
  target_compile_options(hodion_engine PRIVATE /utf-8 /W4)
else()
  target_compile_options(hodion_engine PRIVATE -Wall -Wextra)
endif()

add_executable(hodion_engine_tests
  tests/test_main.cpp
  tests/test_telex.cpp
  tests/test_vni.cpp
  tests/test_edge.cpp
  tests/test_fuzz.cpp
  tests/test_gaps.cpp
  tests/test_mixed.cpp
  tests/test_engine.cpp
)
target_link_libraries(hodion_engine_tests PRIVATE hodion_engine)
# Test white-box: xem được cả bảng nội bộ của engine.
target_include_directories(hodion_engine_tests PRIVATE src)
if(MSVC)
  target_compile_options(hodion_engine_tests PRIVATE /utf-8)
endif()
add_test(NAME engine_tests COMMAND hodion_engine_tests)

# Công cụ dev để dựng/kiểm tra từ điển ngoại lai (xem tools/build_wordlist.py).
add_executable(hodion_wordscan tools/wordscan.cpp)
target_link_libraries(hodion_wordscan PRIVATE hodion_engine)
if(MSVC)
  target_compile_options(hodion_wordscan PRIVATE /utf-8)
endif()
