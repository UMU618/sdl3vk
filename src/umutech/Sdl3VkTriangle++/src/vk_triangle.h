#pragma once

#include <SDL3/SDL.h>

#define VK_NO_PROTOTYPES
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
#define VULKAN_HPP_NO_EXCEPTIONS
#include <vulkan/vulkan.hpp>

namespace umutech::sdl3vk {

class VkTriangle {
 public:
  VkTriangle() noexcept = default;
  ~VkTriangle();
  VkTriangle(const VkTriangle&) noexcept = delete;
  VkTriangle& operator=(const VkTriangle&) noexcept = delete;
  VkTriangle(VkTriangle&&) noexcept = delete;
  VkTriangle& operator=(VkTriangle&&) noexcept = delete;

  bool Initialize() noexcept;
  void Free() noexcept;
  bool Draw() noexcept;
  void SetPause(bool pause) noexcept { is_paused_ = pause; }
  bool RecreateSwapchain() noexcept;

 private:
  inline bool CreateInstance() noexcept;         // 1
  inline bool CreateSurface() noexcept;          // 2
  bool PickPhysicalDevice() noexcept;            // 3
  bool CreateLogicalDevice() noexcept;           // 4
  bool CreateSwapchain() noexcept;               // 5
  bool CreateImageViews() noexcept;              // 6
  bool CreateRenderPass() noexcept;              // 7
  bool CreateShaderModules() noexcept;           // 8
  bool CreateGraphicsPipeline() noexcept;        // 9
  bool CreateFramebuffers() noexcept;            // 10
  bool CreateCommandPool() noexcept;             // 11
  bool CreateVertexBuffer() noexcept;            // 12
  bool CreateCommandBuffers() noexcept;          // 13
  bool RecordCommandBuffers() noexcept;          // 14
  bool CreateSynchronizationObjects() noexcept;  // 15

 private:
  SDL_Window* window_{};

  vk::Instance instance_;
  vk::SurfaceKHR surface_;
  vk::PhysicalDevice physical_device_;
  std::uint32_t graphics_family_{std::numeric_limits<std::uint32_t>::max()};
  std::uint32_t present_family_{std::numeric_limits<std::uint32_t>::max()};
  vk::Device device_;

  vk::SwapchainKHR swapchain_;
  vk::SurfaceFormatKHR surface_format_;
  vk::Extent2D swapchain_extent_;
  std::vector<vk::ImageView> swapchain_image_views_;

  vk::RenderPass render_pass_;
  vk::ShaderModule vert_shader_module_;
  vk::ShaderModule frag_shader_module_;
  vk::Pipeline graphics_pipeline_;
  std::vector<vk::Framebuffer> swapchain_framebuffers_;
  vk::CommandPool command_pool_;
  vk::Buffer vertex_buffer_;
  vk::DeviceMemory vertex_buffer_memory_;
  std::vector<vk::CommandBuffer> command_buffers_;

  vk::Fence in_flight_fence_;
  vk::Semaphore image_available_semaphore_;
  vk::Semaphore render_finished_semaphore_;

  bool is_paused_{};
  bool should_recreate_{};
};

}  // namespace umutech::sdl3vk
