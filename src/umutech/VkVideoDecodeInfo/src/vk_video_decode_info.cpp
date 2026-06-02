#include "vk_video_decode_info.h"

#include <SDL3/SDL_vulkan.h>

#include <gsl/util>
#include <magic_enum/magic_enum.hpp>

// MAGIC_ENUM_RANGE_MIN = -128
// MAGIC_ENUM_RANGE_MAX = 128
// vk::Result are outside the default range

template <>
struct magic_enum::customize::enum_range<vk::VideoCapabilityFlagBitsKHR> {
  static constexpr bool is_flags = true;
};

namespace umutech::sdl3vk {

bool VkVideoDecodeInfo::Initialize() noexcept {
  // 1. Create Vulkan instance
  if (!CreateInstance()) {
    return false;
  }
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
              "Successfully created Vulkan instance");

  return true;
}

void VkVideoDecodeInfo::Free() noexcept {
  // 1
  instance_.destroy();
}

void VkVideoDecodeInfo::PrintVideoDecodeCapabilities() noexcept {
  assert(instance_);

  auto physical_devices = instance_.enumeratePhysicalDevices();
  if (!physical_devices.has_value()) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to enumerate physical devices: #%d",
                 physical_devices.result);
    return;
  }
  if (physical_devices.value.empty()) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "No Vulkan-capable devices found");
    return;
  }

  for (const auto& physical_device : physical_devices.value) {
    auto props = physical_device.getProperties();
    SDL_LogInfo(
        SDL_LOG_CATEGORY_APPLICATION,
        "\n===========================================================");
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Physical Device: %s\n",
                props.deviceName);
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "===========================================================");

    bool has_video_queue = CheckDeviceExtensionSupport(
        physical_device, VK_KHR_VIDEO_QUEUE_EXTENSION_NAME);
    if (!has_video_queue) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "- %s not supported",
                   VK_KHR_VIDEO_QUEUE_EXTENSION_NAME);
      continue;
    }

    bool has_decode_queue = CheckDeviceExtensionSupport(
        physical_device, VK_KHR_VIDEO_DECODE_QUEUE_EXTENSION_NAME);
    if (!has_video_queue || !has_decode_queue) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "- %s not supported",
                   VK_KHR_VIDEO_DECODE_QUEUE_EXTENSION_NAME);
      continue;
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "+ Vulkan Video Decode extensions supported");

    bool found_video_queue{};
    auto queue_families = physical_device.getQueueFamilyProperties();
    for (std::uint32_t i = 0; i < queue_families.size(); ++i) {
      if (vk::QueueFlagBits::eVideoDecodeKHR & queue_families[i].queueFlags) {
        found_video_queue = true;
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "  + Queue family %u supports video decode", i);
      }
    }
    if (!found_video_queue) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                   "  - No video decode queue available");
      continue;
    }

    const std::vector<vk::VideoCodecOperationFlagBitsKHR> codecs{
        vk::VideoCodecOperationFlagBitsKHR::eDecodeH264,
        vk::VideoCodecOperationFlagBitsKHR::eDecodeH265,
        vk::VideoCodecOperationFlagBitsKHR::eDecodeAv1,
        vk::VideoCodecOperationFlagBitsKHR::eDecodeVp9};

    for (auto codec : codecs) {
      vk::VideoProfileInfoKHR profile{
          codec, vk::VideoChromaSubsamplingFlagBitsKHR::e420,
          vk::VideoComponentBitDepthFlagBitsKHR::e8,
          vk::VideoComponentBitDepthFlagBitsKHR::e8};

      auto result = physical_device.getVideoCapabilitiesKHR(profile);
      if (result.has_value()) {
        SDL_LogInfo(
            SDL_LOG_CATEGORY_APPLICATION, "  + Video capabilities for %s: %s",
            magic_enum::enum_name(codec).data(),
            magic_enum::enum_name(
                static_cast<vk::VideoCapabilityFlagBitsKHR>(
                    std::underlying_type<vk::VideoCapabilityFlagBitsKHR>::type(
                        result->flags)))
                .data());
      } else {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "  - Video capabilities for %s: NotSupported, #%s(%d)",
                    magic_enum::enum_name(codec).data(),
                    magic_enum::enum_name(result.result).data(), result.result);
      }
    }
  }  // end of for
}

// 1
bool VkVideoDecodeInfo::CreateInstance() noexcept {
  std::uint32_t extension_count{};
  auto extensions = SDL_Vulkan_GetInstanceExtensions(&extension_count);
  if (!extensions) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to get Vulkan instance extensions: %s",
                 SDL_GetError());
    return false;
  }
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Required Vulkan extensions: %u",
              extension_count);
  for (std::uint32_t i = 0; i < extension_count; ++i) {
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Extension %u: %s", i,
                extensions[i]);
  }

  constexpr vk::ApplicationInfo app_info{
      "VkVideoDecodeInfo", VK_MAKE_VERSION(1, 0, 0), "No Engine",
      VK_MAKE_VERSION(1, 0, 0), vk::ApiVersion14};
  vk::InstanceCreateInfo create_info{
      vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR,
      &app_info,
      0,
      nullptr,
      gsl::narrow_cast<std::uint32_t>(extension_count),
      extensions};
  auto instance = vk::createInstance(create_info);
  if (!instance.has_value()) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to create Vulkan instance: #%d", instance.result);
    return false;
  }
  instance_ = instance.value;
  VULKAN_HPP_DEFAULT_DISPATCHER.init(instance_);
  return true;
}

bool VkVideoDecodeInfo::CheckDeviceExtensionSupport(
    const vk::PhysicalDevice& physical_device,
    std::string_view required_extension) noexcept {
  auto extension_properties =
      physical_device.enumerateDeviceExtensionProperties();
  if (!extension_properties.has_value()) {
    return false;
  }

  for (const auto& ext : extension_properties.value) {
    if (required_extension == ext.extensionName) {
      return true;
    }
  }
  return false;
}

}  // namespace umutech::sdl3vk
