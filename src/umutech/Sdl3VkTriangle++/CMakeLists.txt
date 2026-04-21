add_executable(Sdl3VkTriangle++
  src/main.cpp
  src/vk_triangle.cpp)
set_property(TARGET Sdl3VkTriangle++ PROPERTY CXX_STANDARD 20)

find_package(glm CONFIG REQUIRED)
find_package(Microsoft.GSL CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(VulkanHeaders CONFIG)

target_include_directories(Sdl3VkTriangle++ PRIVATE ..)
target_link_libraries(Sdl3VkTriangle++ PRIVATE
  Microsoft.GSL::GSL
  SDL3::SDL3
  Vulkan::Headers)
