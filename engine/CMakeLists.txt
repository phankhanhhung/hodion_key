# Lõi bộ gõ — portable, không phụ thuộc OS.
add_library(hodion_engine STATIC
  src/chartable.cpp
  src/composer.cpp
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
  tests/test_engine.cpp
)
target_link_libraries(hodion_engine_tests PRIVATE hodion_engine)
if(MSVC)
  target_compile_options(hodion_engine_tests PRIVATE /utf-8)
endif()
add_test(NAME engine_tests COMMAND hodion_engine_tests)

add_executable(hodion_demo tools/demo.cpp)
target_link_libraries(hodion_demo PRIVATE hodion_engine)
if(MSVC)
  target_compile_options(hodion_demo PRIVATE /utf-8)
endif()
