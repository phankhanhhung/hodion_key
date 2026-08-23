cmake_minimum_required(VERSION 3.16)
project(hodion_key LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

add_subdirectory(engine)

# Tầng Windows (TSF text service) chỉ build trên Windows.
if(WIN32)
  add_subdirectory(platform/windows)
endif()
