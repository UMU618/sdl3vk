#pragma once

#include <SDL3/SDL.h>

#define VK_NO_PROTOTYPES
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
#define VULKAN_HPP_NO_EXCEPTIONS
#include <vulkan/vulkan.hpp>

namespace umutech::sdl3vk {

class VkVideoDecodeInfo {
 public:
  VkVideoDecodeInfo() noexcept = default;
  ~VkVideoDecodeInfo() { Free(); }
  VkVideoDecodeInfo(const VkVideoDecodeInfo&) noexcept = delete;
  VkVideoDecodeInfo& operator=(const VkVideoDecodeInfo&) noexcept = delete;
  VkVideoDecodeInfo(VkVideoDecodeInfo&&) noexcept = delete;
  VkVideoDecodeInfo& operator=(VkVideoDecodeInfo&&) noexcept = delete;

  bool Initialize() noexcept;
  void Free() noexcept;

  void PrintVideoDecodeCapabilities() noexcept;

 private:
  inline bool CreateInstance() noexcept;  // 1

  bool CheckDeviceExtensionSupport(
      const vk::PhysicalDevice& physical_device,
      std::string_view required_extension) noexcept;

 private:
  vk::Instance instance_;
};

}  // namespace umutech::sdl3vk
