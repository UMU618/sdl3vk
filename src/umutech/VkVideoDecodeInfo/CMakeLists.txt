add_executable(VkVideoDecodeInfo
  src/main.cpp
  src/vk_video_decode_info.cpp)
set_property(TARGET VkVideoDecodeInfo PROPERTY CXX_STANDARD 20)

find_package(magic_enum CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(Microsoft.GSL CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(VulkanHeaders CONFIG)

target_include_directories(VkVideoDecodeInfo PRIVATE ..)
target_link_libraries(VkVideoDecodeInfo PRIVATE
  magic_enum::magic_enum
  Microsoft.GSL::GSL
  SDL3::SDL3
  Vulkan::Headers)
