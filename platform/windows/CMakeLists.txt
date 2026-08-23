# TSF text service — build bằng MSVC (Visual Studio) hoặc clang-cl trên Windows.
add_library(hodionkey_tsf SHARED
  src/ClassFactory.cpp
  src/DisplayAttribute.cpp
  src/DllMain.cpp
  src/Guids.cpp
  src/KeyHandler.cpp
  src/Register.cpp
  src/Settings.cpp
  src/TextService.cpp
  HodionKey.def
)

set_target_properties(hodionkey_tsf PROPERTIES
  OUTPUT_NAME "HodionKey"
  PREFIX ""
)

target_link_libraries(hodionkey_tsf PRIVATE
  hodion_engine
  ole32
  oleaut32
  uuid
  advapi32
  user32
)

target_compile_definitions(hodionkey_tsf PRIVATE UNICODE _UNICODE)

if(MSVC)
  target_compile_options(hodionkey_tsf PRIVATE /utf-8 /W4)
elseif(MINGW)
  # Không bắt người dùng cài runtime MinGW kèm theo.
  target_link_options(hodionkey_tsf PRIVATE -static-libgcc -static-libstdc++)
endif()

# App cấu hình — hộp thoại Win32 thuần, đọc/ghi cùng khoá registry.
set(hodionkey_config_sources
  config/ConfigApp.cpp
  config/ConfigApp.rc
  src/Settings.cpp
)
if(MSVC)
  # MSVC tự sinh manifest cho exe; CMake chuyển file .manifest này cho
  # linker (/MANIFESTINPUT) để gộp vào đó. Không nhúng thêm từ .rc, nếu
  # không linker báo CVT1100 "duplicate resource".
  list(APPEND hodionkey_config_sources config/HodionKeyConfig.manifest)
endif()

add_executable(hodionkey_config WIN32 ${hodionkey_config_sources})

set_target_properties(hodionkey_config PROPERTIES
  OUTPUT_NAME "HodionKeyConfig"
)

target_include_directories(hodionkey_config PRIVATE src config)

target_link_libraries(hodionkey_config PRIVATE
  hodion_engine
  comctl32
  advapi32
  user32
)

target_compile_definitions(hodionkey_config PRIVATE UNICODE _UNICODE)

if(MSVC)
  target_compile_options(hodionkey_config PRIVATE /utf-8 /W4)
elseif(MINGW)
  # Chọn entry point wWinMain (MSVC tự làm khi có UNICODE).
  target_compile_options(hodionkey_config PRIVATE -municode)
  target_link_options(hodionkey_config PRIVATE
    -municode -static-libgcc -static-libstdc++)
  # ld không tự sinh manifest, nên windres nhúng manifest từ .rc.
  target_compile_definitions(hodionkey_config PRIVATE HODION_RC_MANIFEST)
endif()

# Kiểm thử tầng Windows: vòng đọc/ghi registry + theo dõi thay đổi cấu hình.
add_executable(hodionkey_settings_test
  tests/settings_test.cpp
  src/Settings.cpp
)
target_include_directories(hodionkey_settings_test PRIVATE src)
target_link_libraries(hodionkey_settings_test PRIVATE hodion_engine advapi32)
target_compile_definitions(hodionkey_settings_test PRIVATE UNICODE _UNICODE)
if(MSVC)
  target_compile_options(hodionkey_settings_test PRIVATE /utf-8 /W4)
elseif(MINGW)
  target_link_options(hodionkey_settings_test PRIVATE
    -static-libgcc -static-libstdc++)
endif()
add_test(NAME windows_settings COMMAND hodionkey_settings_test)
