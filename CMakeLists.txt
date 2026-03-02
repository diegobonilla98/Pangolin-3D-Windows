cmake_minimum_required(VERSION 3.23)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(_VCPKG_TOOLCHAIN "")
  if(DEFINED ENV{VCPKG_ROOT})
    set(_VCPKG_TOOLCHAIN "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
  elseif(DEFINED ENV{VCPKG_INSTALLATION_ROOT})
    set(_VCPKG_TOOLCHAIN "$ENV{VCPKG_INSTALLATION_ROOT}/scripts/buildsystems/vcpkg.cmake")
  elseif(DEFINED ENV{USERPROFILE})
    set(_VCPKG_TOOLCHAIN "$ENV{USERPROFILE}/vcpkg/scripts/buildsystems/vcpkg.cmake")
  endif()

  if(_VCPKG_TOOLCHAIN AND EXISTS "${_VCPKG_TOOLCHAIN}")
    set(CMAKE_TOOLCHAIN_FILE "${_VCPKG_TOOLCHAIN}" CACHE FILEPATH "Vcpkg toolchain file")
  endif()
endif()

project(pangolin_fast LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(pybind11 CONFIG REQUIRED)
find_package(Pangolin CONFIG REQUIRED)

pybind11_add_module(_pangolin_fast MODULE
  src/bindings.cpp
  src/viewer.cpp
  src/viewer.h
)

target_include_directories(_pangolin_fast PRIVATE src)
target_link_libraries(_pangolin_fast PRIVATE pango_display)

target_compile_definitions(_pangolin_fast PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
if(MSVC)
  target_compile_options(_pangolin_fast PRIVATE /O2 /EHsc /utf-8)
endif()

install(TARGETS _pangolin_fast
  LIBRARY DESTINATION pangolin_fast
  RUNTIME DESTINATION pangolin_fast
  ARCHIVE DESTINATION pangolin_fast
)

install(FILES pangolin_fast/__init__.py DESTINATION pangolin_fast)
