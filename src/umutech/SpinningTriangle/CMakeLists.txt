add_executable(SpinningTriangle
  src/main.cpp
  src/spinning_triangle.cpp)
set_property(TARGET SpinningTriangle PROPERTY CXX_STANDARD 20)

find_package(magic_enum CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(Microsoft.GSL CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(VulkanHeaders CONFIG)

target_include_directories(SpinningTriangle PRIVATE ..)
target_link_libraries(SpinningTriangle PRIVATE
  magic_enum::magic_enum
  Microsoft.GSL::GSL
  SDL3::SDL3
  Vulkan::Headers)
