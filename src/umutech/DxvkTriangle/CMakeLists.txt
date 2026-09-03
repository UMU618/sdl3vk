cmake_minimum_required (VERSION 4.2)
project(dxvk_triangle LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${PROJECT_SOURCE_DIR}/../bin/Debug)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${PROJECT_SOURCE_DIR}/../bin/Release)

find_package(PkgConfig REQUIRED)

pkg_check_modules(DXVK_D3D11 REQUIRED IMPORTED_TARGET dxvk-d3d11)
pkg_check_modules(DXVK_DXGI  REQUIRED IMPORTED_TARGET dxvk-dxgi)
pkg_check_modules(SDL3       REQUIRED IMPORTED_TARGET sdl3)

add_executable(dxvk_triangle src/dxvk_triangle.cpp)
set_property(TARGET dxvk_triangle PROPERTY CXX_STANDARD 20)
target_link_libraries(dxvk_triangle PRIVATE
  PkgConfig::DXVK_D3D11
  PkgConfig::DXVK_DXGI
  PkgConfig::SDL3)
