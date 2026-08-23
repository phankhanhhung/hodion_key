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
endif()
