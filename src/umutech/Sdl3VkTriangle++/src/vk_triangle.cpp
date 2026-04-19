#include "vk_triangle.h"

#include <array>
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

#include <SDL3/SDL_vulkan.h>

#include <gsl/util>

#include <glm/glm.hpp>

namespace fs = std::filesystem;

namespace {

struct Vertex {
  glm::vec2 pos;
  glm::vec3 color;

  static vk::VertexInputBindingDescription GetBindingDescription() noexcept {
    return vk::VertexInputBindingDescription{0, sizeof(Vertex),
                                             vk::VertexInputRate::eVertex};
  }

  static std::array<vk::VertexInputAttributeDescription, 2>
  GetAttributeDescriptions() noexcept {
    return std::array<vk::VertexInputAttributeDescription, 2>{
        vk::VertexInputAttributeDescription{0, 0, vk::Format::eR32G32Sfloat,
                                            offsetof(Vertex, pos)},
        vk::VertexInputAttributeDescription{1, 0, vk::Format::eR32G32B32Sfloat,
                                            offsetof(Vertex, color)}};
  }
};

// UMU: vk::FrontFace::eClockwise
constexpr std::array<Vertex, 3> kVertices{
    Vertex{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    Vertex{{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
    Vertex{{0.0f, 0.5f}, {0.0f, 0.0f, 1.0f}}};

std::vector<std::byte> ReadFile(const fs::path& filename) {
  std::ifstream file(filename, std::ios::in | std::ios::ate | std::ios::binary);
  if (!file.is_open()) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to open file: %.*s",
                 gsl::narrow_cast<int>(filename.string().size()),
                 filename.string().c_str());
    return {};
  }
  std::size_t file_size = file.tellg();
  std::vector<std::byte> buffer(file_size);
  file.seekg(0);
  file.read(reinterpret_cast<char*>(buffer.data()), file_size);
  file.close();
  return buffer;
}

}  // namespace

VkTriangle::~VkTriangle() {
  Free();
}

bool VkTriangle::Initialize() noexcept {
  window_ = SDL_CreateWindow("Sdl3VkTriangle++", 800, 618,
                             SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
  if (!window_) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Window creation failed: %s",
                 SDL_GetError());
    return false;
  }

  // 1. Create Vulkan instance
  if (!CreateInstance()) {
    return false;
  }
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
              "Successfully created Vulkan instance");

  // 2. Create surface
  if (!CreateSurface()) {
    return false;
  }
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
              "Successfully created Vulkan surface");

  // 3. Pick physical device
  if (!PickPhysicalDevice()) {
    return false;
  }
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Selected physical device");

  // 4. Create logical device
  if (!CreateLogicalDevice()) {
    return false;
  }
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
              "Successfully created logical device");

  // 5. Create swapchain
  if (!CreateSwapchain()) {
    return false;
  }
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Successfully created swapchain");

  // 6. Create image views
  if (!CreateImageViews()) {
    return false;
  }
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Successfully created image views");

  // 7. Create render pass
  if (!CreateRenderPass()) {
    return false;
  }
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Successfully created render pass");

  // 8. Load and create shader modules
  if (!CreateShaderModules()) {
    return false;
  }
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
              "Successfully created shader modules");

  // 9. Create graphics pipeline
  if (!CreateGraphicsPipeline()) {
    return false;
  }
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
              "Successfully created graphics pipeline");

  // 10. Create framebuffers
  if (!CreateFramebuffers()) {
    return false;
  }
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
              "Successfully created %zu framebuffers",
              swapchain_framebuffers_.size());

  // 11. Create command pool
  if (!CreateCommandPool()) {
    return false;
  }
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
              "Successfully created command pool");

  // 12. Create vertex buffer
  if (!CreateVertexBuffer()) {
    return false;
  }
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
              "Successfully created vertex buffer");

  // 13. Create command buffers (one per swapchain image)
  if (!CreateCommandBuffers()) {
    return false;
  }
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
              "Successfully created %zu command buffers",
              command_buffers_.size());

  // 14. Record command buffers
  if (!RecordCommandBuffers()) {
    return false;
  }
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
              "Successfully recorded %zu command buffers",
              command_buffers_.size());

  // 15. Create synchronization objects
  if (!CreateSynchronizationObjects()) {
    return false;
  }
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
              "Successfully created synchronization objects");

  return true;
}

void VkTriangle::Free() noexcept {
  if (!window_) {
    return;
  }

  if (device_) {
    vk::Result result = device_.waitIdle();
    if (vk::Result::eSuccess != result) {
      SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                  "Failed to wait for device idle: #%d", result);
    }
  }

  // 15
  if (render_finished_semaphore_) {
    device_.destroySemaphore(render_finished_semaphore_);
    render_finished_semaphore_ = nullptr;
  }
  if (image_available_semaphore_) {
    device_.destroySemaphore(image_available_semaphore_);
    image_available_semaphore_ = nullptr;
  }
  if (in_flight_fence_) {
    device_.destroyFence(in_flight_fence_);
    in_flight_fence_ = nullptr;
  }
  // 14: Nothing
  // 13
  if (!command_buffers_.empty()) {
    device_.freeCommandBuffers(command_pool_, command_buffers_);
    command_buffers_.clear();
  }
  // 12
  if (vertex_buffer_memory_) {
    device_.freeMemory(vertex_buffer_memory_);
    vertex_buffer_memory_ = nullptr;
  }
  if (vertex_buffer_) {
    device_.destroyBuffer(vertex_buffer_);
    vertex_buffer_ = nullptr;
  }
  // 11
  if (command_pool_) {
    device_.destroyCommandPool(command_pool_);
    command_pool_ = nullptr;
  }
  // 10
  for (auto& framebuffer : swapchain_framebuffers_) {
    device_.destroyFramebuffer(framebuffer);
  }
  swapchain_framebuffers_.clear();
  // 9
  if (graphics_pipeline_) {
    device_.destroyPipeline(graphics_pipeline_);
    graphics_pipeline_ = nullptr;
  }
  // 8
  if (frag_shader_module_) {
    device_.destroyShaderModule(frag_shader_module_);
    frag_shader_module_ = nullptr;
  }
  if (vert_shader_module_) {
    device_.destroyShaderModule(vert_shader_module_);
    vert_shader_module_ = nullptr;
  }
  // 7
  if (render_pass_) {
    device_.destroyRenderPass(render_pass_);
    render_pass_ = nullptr;
  }
  // 6
  for (auto& image_view : swapchain_image_views_) {
    device_.destroyImageView(image_view);
  }
  swapchain_image_views_.clear();
  // 5
  if (swapchain_) {
    device_.destroySwapchainKHR(swapchain_);
    swapchain_ = nullptr;
  }
  // 4
  if (device_) {
    device_.destroy();
    device_ = nullptr;
  }
  // 3
  if (physical_device_) {
    // No need to free physical device
    physical_device_ = nullptr;
  }
  // 2
  if (surface_) {
    instance_.destroySurfaceKHR(surface_);
    surface_ = nullptr;
  }
  // 1
  instance_.destroy();

  SDL_DestroyWindow(window_);
  window_ = nullptr;
}

bool VkTriangle::Draw() noexcept {
  assert(window_);
  assert(device_);
  assert(in_flight_fence_);
  assert(image_available_semaphore_);
  assert(render_finished_semaphore_);

  if (is_paused_) {
    return true;
  }

  if (should_recreate_) {
    if (!RecreateSwapchain()) {
      return false;
    }
  }

  if (vk::Result result =
          device_.waitForFences(in_flight_fence_, VK_TRUE, UINT64_MAX);
      result != vk::Result::eSuccess) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to wait for fences: #%d",
                 result);
    return false;
  }
  if (vk::Result result = device_.resetFences(in_flight_fence_);
      result != vk::Result::eSuccess) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to reset fences: #%d",
                 result);
    return false;
  }
#if _DEBUG
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "The fence is signaled and reset");
#endif

  std::uint32_t image_index{};
  vk::Result result = device_.acquireNextImageKHR(swapchain_, UINT64_MAX,
                                                  image_available_semaphore_,
                                                  nullptr, &image_index);
  if (vk::Result::eErrorOutOfDateKHR == result ||
      vk::Result::eSuboptimalKHR == result) {
    should_recreate_ = true;
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                "Swapchain is out of date, need to recreate");
    return true;
  }
  if (vk::Result::eSuccess != result) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to acquire next image: #%d", result);
    return false;
  }
#if _DEBUG
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Acquired image %u", image_index);
#endif

  vk::PipelineStageFlags wait_stages[]{
      vk::PipelineStageFlagBits::eColorAttachmentOutput};
  vk::SubmitInfo submit_info{1,
                             &image_available_semaphore_,
                             wait_stages,
                             1,
                             &command_buffers_[image_index],
                             1,
                             &render_finished_semaphore_};
  if (result = device_.getQueue(graphics_family_, 0)
                   .submit(submit_info, in_flight_fence_);
      vk::Result::eSuccess != result) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to submit draw command: #%d", result);
    return false;
  }
#if _DEBUG
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
              "Submitted draw command buffer for image %u", image_index);
#endif

  vk::PresentInfoKHR present_info{1, &render_finished_semaphore_, 1,
                                  &swapchain_, &image_index};
  if (result = device_.getQueue(present_family_, 0).presentKHR(present_info);
      vk::Result::eErrorOutOfDateKHR == result ||
      vk::Result::eSuboptimalKHR == result) {
    should_recreate_ = true;
    SDL_LogWarn(
        SDL_LOG_CATEGORY_APPLICATION,
        "Swapchain is out of date after presentation, need to recreate");
    return true;
  }
  if (vk::Result::eSuccess != result) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to present image: #%d",
                 result);
    return false;
  }
#if _DEBUG
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Presented image %u", image_index);
#endif

  return true;
}

bool VkTriangle::RecreateSwapchain() noexcept {
  should_recreate_ = false;

  if (device_) {
    vk::Result result = device_.waitIdle();
    if (vk::Result::eSuccess != result) {
      SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                  "Failed to wait for device idle: #%d", result);
    }
  }

  // 14: Nothing
  // 13
  if (!command_buffers_.empty()) {
    device_.freeCommandBuffers(command_pool_, command_buffers_);
    command_buffers_.clear();
  }
  // 10
  for (auto& framebuffer : swapchain_framebuffers_) {
    device_.destroyFramebuffer(framebuffer);
  }
  swapchain_framebuffers_.clear();
  // 9
  if (graphics_pipeline_) {
    device_.destroyPipeline(graphics_pipeline_);
    graphics_pipeline_ = nullptr;
  }
  // 6
  for (auto& image_view : swapchain_image_views_) {
    device_.destroyImageView(image_view);
  }
  swapchain_image_views_.clear();
  // 5
  if (swapchain_) {
    device_.destroySwapchainKHR(swapchain_);
    swapchain_ = nullptr;
  }

  // 5
  if (!CreateSwapchain()) {
    return false;
  }
  // 6
  if (!CreateImageViews()) {
    return false;
  }
  // 9
  if (!CreateGraphicsPipeline()) {
    return false;
  }
  // 10
  if (!CreateFramebuffers()) {
    return false;
  }
  // 13
  if (!CreateCommandBuffers()) {
    return false;
  }
  // 14
  if (!RecordCommandBuffers()) {
    return false;
  }
  return true;
}

// 1
bool VkTriangle::CreateInstance() noexcept {
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

  constexpr vk::ApplicationInfo app_info{"VkTriangle", VK_MAKE_VERSION(1, 0, 0),
                                         "No Engine", VK_MAKE_VERSION(1, 0, 0),
                                         vk::ApiVersion14};
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

// 2
bool VkTriangle::CreateSurface() noexcept {
  VkSurfaceKHR surface;
  if (!SDL_Vulkan_CreateSurface(window_, instance_, nullptr, &surface)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to create Vulkan surface: %s", SDL_GetError());
    return false;
  }
  surface_ = surface;
  return true;
}

// 3
bool VkTriangle::PickPhysicalDevice() noexcept {
  auto physical_devices = instance_.enumeratePhysicalDevices();
  if (!physical_devices.has_value()) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to enumerate physical devices: #%d",
                 physical_devices.result);
    return false;
  }
  if (physical_devices.value.empty()) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "No Vulkan-capable devices found");
    return false;
  }

  for (const auto& physical_device : physical_devices.value) {
    const auto queue_families = physical_device.getQueueFamilyProperties();

    bool has_graphics{};
    bool has_present{};
    for (std::uint32_t i = 0; i < queue_families.size(); ++i) {
      if (vk::QueueFlagBits::eGraphics & queue_families[i].queueFlags) {
        graphics_family_ = i;
        has_graphics = true;
      }
      auto present_support = physical_device.getSurfaceSupportKHR(i, surface_);
      if (!present_support.has_value()) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to get physical device surface support: #%d",
                     present_support.result);
        continue;
      }
      if (present_support.value) {
        present_family_ = i;
        has_present = true;
      }
    }
    if (has_graphics && has_present) {
      physical_device_ = physical_device;
      break;
    }
  }
  if (!physical_device_) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "No suitable Vulkan device found");
    return false;
  }
  return true;
}

// 4
bool VkTriangle::CreateLogicalDevice() noexcept {
  float queue_priority{1.0f};
  std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
  vk::DeviceQueueCreateInfo graphics_queue_create_info{
      {}, graphics_family_, 1, &queue_priority};
  queue_create_infos.push_back(graphics_queue_create_info);
  if (graphics_family_ != present_family_) {
    vk::DeviceQueueCreateInfo present_queue_create_info{
        {}, present_family_, 1, &queue_priority};
    queue_create_infos.push_back(present_queue_create_info);
  }
  vk::PhysicalDeviceFeatures device_features{};
  constexpr std::array<const char*, 1> device_extensions{
      VK_KHR_SWAPCHAIN_EXTENSION_NAME};
  vk::DeviceCreateInfo device_create_info{
      {},
      gsl::narrow_cast<std::uint32_t>(queue_create_infos.size()),
      queue_create_infos.data(),
      0,
      nullptr,
      gsl::narrow_cast<std::uint32_t>(device_extensions.size()),
      device_extensions.data(),
      &device_features};
  auto device = physical_device_.createDevice(device_create_info);
  if (!device.has_value()) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to create logical device: #%d", device.result);
    return false;
  }
  device_ = device.value;
  return true;
}

// 5
bool VkTriangle::CreateSwapchain() noexcept {
  vk::SurfaceCapabilitiesKHR capabilities;
  if (vk::Result result =
          physical_device_.getSurfaceCapabilitiesKHR(surface_, &capabilities);
      vk::Result::eSuccess != result) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to get surface capabilities: %d", result);
    return false;
  }

  auto formats = physical_device_.getSurfaceFormatsKHR(surface_);
  if (!formats.has_value()) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to get physical device surface formats: #%d",
                 formats.result);
    return false;
  }

  surface_format_ = formats.value[0];
  for (const auto& format : formats.value) {
    if ((vk::Format::eB8G8R8A8Unorm == format.format ||
         vk::Format::eR8G8B8A8Unorm == format.format) &&
        vk::ColorSpaceKHR::eSrgbNonlinear == format.colorSpace) {
      surface_format_ = format;
      break;
    }
  }

  swapchain_extent_ = capabilities.currentExtent;
  if (UINT32_MAX == swapchain_extent_.width) {
    int width, height;
    SDL_GetWindowSizeInPixels(window_, &width, &height);
    swapchain_extent_.width = static_cast<uint32_t>(width);
    swapchain_extent_.height = static_cast<uint32_t>(height);
  }

  vk::SwapchainCreateInfoKHR swapchain_info{
      {},
      surface_,
      2,
      surface_format_.format,
      surface_format_.colorSpace,
      swapchain_extent_,
      1,
      vk::ImageUsageFlagBits::eColorAttachment,
      vk::SharingMode::eExclusive,
      0,
      nullptr,
      capabilities.currentTransform,
      vk::CompositeAlphaFlagBitsKHR::eOpaque,
      vk::PresentModeKHR::eFifo,
      VK_TRUE};
  if (vk::Result result =
          device_.createSwapchainKHR(&swapchain_info, nullptr, &swapchain_);
      vk::Result::eSuccess != result) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create swapchain: %d",
                 result);
    return false;
  }

  return true;
}

// 6
bool VkTriangle::CreateImageViews() noexcept {
  auto swapchain_images = device_.getSwapchainImagesKHR(swapchain_);
  if (!swapchain_images.has_value()) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to get swapchain images: #%d",
                 swapchain_images.result);
    return false;
  }

  swapchain_image_views_.reserve(swapchain_images.value.size());
  for (size_t i = 0; i < swapchain_images.value.size(); ++i) {
    vk::ImageViewCreateInfo create_info{
        {},
        swapchain_images.value[i],
        vk::ImageViewType::e2D,
        surface_format_.format,
        vk::ComponentMapping{},
        {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};
    auto image_view = device_.createImageView(create_info);
    if (!image_view.has_value()) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                   "Failed to create image view for swapchain image %zu: #%d",
                   i, image_view.result);
      return false;
    }
    swapchain_image_views_.emplace_back(std::move(image_view).value);
  }

  return true;
}

// 7
bool VkTriangle::CreateRenderPass() noexcept {
  vk::AttachmentDescription color_attachment{{},
                                             surface_format_.format,
                                             vk::SampleCountFlagBits::e1,
                                             vk::AttachmentLoadOp::eClear,
                                             vk::AttachmentStoreOp::eStore,
                                             vk::AttachmentLoadOp::eDontCare,
                                             vk::AttachmentStoreOp::eDontCare,
                                             vk::ImageLayout::eUndefined,
                                             vk::ImageLayout::ePresentSrcKHR};
  vk::AttachmentReference color_attachment_ref{
      0, vk::ImageLayout::eColorAttachmentOptimal};
  vk::SubpassDescription subpass{{}, vk::PipelineBindPoint::eGraphics,
                                 0,  nullptr,
                                 1,  &color_attachment_ref};
  vk::RenderPassCreateInfo render_pass_info{
      {}, 1, &color_attachment, 1, &subpass};
  if (vk::Result result =
          device_.createRenderPass(&render_pass_info, nullptr, &render_pass_);
      vk::Result::eSuccess != result) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to create render pass: #%d", result);
    return false;
  }
  return true;
}

// 8
bool VkTriangle::CreateShaderModules() noexcept {
  const auto assets_dir = fs::current_path().parent_path() / "assets";

  auto vert_shader_code = ReadFile(assets_dir / "triangle++_vert.spv");
  if (vert_shader_code.empty()) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to load vertex shader file");
    return false;
  }
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
              "Successfully loaded vertex shader (%zu bytes)",
              vert_shader_code.size());

  vk::ShaderModuleCreateInfo vert_shader_info{
      {},
      vert_shader_code.size(),
      reinterpret_cast<const uint32_t*>(vert_shader_code.data())};
  if (vk::Result result = device_.createShaderModule(&vert_shader_info, nullptr,
                                                     &vert_shader_module_);
      vk::Result::eSuccess != result) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to create vertex shader module: #%d", result);
    return false;
  }

  auto frag_shader_code = ReadFile(assets_dir / "triangle++_frag.spv");
  if (frag_shader_code.empty()) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to load fragment shader file");
    return false;
  }
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
              "Successfully loaded fragment shader (%zu bytes)",
              frag_shader_code.size());

  vk::ShaderModuleCreateInfo frag_shader_info{
      {},
      frag_shader_code.size(),
      reinterpret_cast<const std::uint32_t*>(frag_shader_code.data())};
  if (vk::Result result = device_.createShaderModule(&frag_shader_info, nullptr,
                                                     &frag_shader_module_);
      vk::Result::eSuccess != result) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to create fragment shader module: #%d", result);
    return false;
  }

  return true;
}

// 9
bool VkTriangle::CreateGraphicsPipeline() noexcept {
  vk::PipelineShaderStageCreateInfo shader_stages[] = {
      // vert
      {{}, vk::ShaderStageFlagBits::eVertex, vert_shader_module_, "main"},
      //  frag
      {{}, vk::ShaderStageFlagBits::eFragment, frag_shader_module_, "main"}};

  auto binding_description = Vertex::GetBindingDescription();
  auto attribute_descriptions = Vertex::GetAttributeDescriptions();

  vk::PipelineVertexInputStateCreateInfo vertex_input_info{
      {},
      1,
      &binding_description,
      gsl::narrow_cast<std::uint32_t>(attribute_descriptions.size()),
      attribute_descriptions.data()};
  vk::PipelineInputAssemblyStateCreateInfo input_assembly{
      {}, vk::PrimitiveTopology::eTriangleList, VK_FALSE};
  vk::Viewport viewport{0.0f,
                        0.0f,
                        static_cast<float>(swapchain_extent_.width),
                        static_cast<float>(swapchain_extent_.height),
                        0.0f,
                        1.0f};
  vk::Rect2D scissor{{0, 0}, swapchain_extent_};
  vk::PipelineViewportStateCreateInfo viewport_state{
      {}, 1, &viewport, 1, &scissor};
  vk::PipelineRasterizationStateCreateInfo rasterizer{
      {},
      VK_FALSE,
      VK_FALSE,
      vk::PolygonMode::eFill,
      vk::CullModeFlagBits::eBack,
      vk::FrontFace::eClockwise,
      VK_FALSE,
      0.0f,
      0.0f,
      0.0f};
  vk::PipelineMultisampleStateCreateInfo multisampling{
      {}, vk::SampleCountFlagBits::e1, VK_FALSE};
  vk::PipelineColorBlendAttachmentState color_blend_attachment{
      VK_FALSE,
      vk::BlendFactor::eOne,
      vk::BlendFactor::eZero,
      vk::BlendOp::eAdd,
      vk::BlendFactor::eOne,
      vk::BlendFactor::eZero,
      vk::BlendOp::eAdd,
      vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
          vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};
  vk::PipelineColorBlendStateCreateInfo color_blending{
      {},
      VK_FALSE,
      vk::LogicOp::eCopy,
      1,
      &color_blend_attachment,
      {0.0f, 0.0f, 0.0f, 0.0f}};
  vk::PipelineLayoutCreateInfo pipeline_layout_info{};
  vk::PipelineLayout pipeline_layout;
  if (vk::Result result = device_.createPipelineLayout(
          &pipeline_layout_info, nullptr, &pipeline_layout);
      vk::Result::eSuccess != result) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to create pipeline layout: #%d", result);
    return false;
  }
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
              "Successfully created pipeline layout");

  vk::GraphicsPipelineCreateInfo pipeline_info{{},
                                               2,
                                               shader_stages,
                                               &vertex_input_info,
                                               &input_assembly,
                                               nullptr,
                                               &viewport_state,
                                               &rasterizer,
                                               &multisampling,
                                               nullptr,
                                               &color_blending,
                                               nullptr,
                                               pipeline_layout,
                                               render_pass_,
                                               0};
  if (vk::Result result = device_.createGraphicsPipelines(
          VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &graphics_pipeline_);
      vk::Result::eSuccess != result) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to create graphics pipeline: #%d", result);
    return false;
  }
  return true;
}

// 10
bool VkTriangle::CreateFramebuffers() noexcept {
  swapchain_framebuffers_.resize(swapchain_image_views_.size());
  for (size_t i = 0; i < swapchain_image_views_.size(); ++i) {
    vk::ImageView attachments[] = {swapchain_image_views_[i]};
    vk::FramebufferCreateInfo framebuffer_info{{},
                                               render_pass_,
                                               1,
                                               attachments,
                                               swapchain_extent_.width,
                                               swapchain_extent_.height,
                                               1};
    if (vk::Result result = device_.createFramebuffer(
            &framebuffer_info, nullptr, &swapchain_framebuffers_[i]);
        vk::Result::eSuccess != result) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                   "Failed to create framebuffer: #%d", result);
      return false;
    }
  }
  return true;
}

// 11
bool VkTriangle::CreateCommandPool() noexcept {
  vk::CommandPoolCreateInfo pool_info{{}, graphics_family_};
  if (vk::Result result =
          device_.createCommandPool(&pool_info, nullptr, &command_pool_);
      vk::Result::eSuccess != result) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to create command pool: #%d", result);
    return false;
  }
  return true;
}

// 12
bool VkTriangle::CreateVertexBuffer() noexcept {
  vk::BufferCreateInfo buffer_info{{},
                                   sizeof(kVertices),
                                   vk::BufferUsageFlagBits::eVertexBuffer,
                                   vk::SharingMode::eExclusive};
  if (vk::Result result =
          device_.createBuffer(&buffer_info, nullptr, &vertex_buffer_);
      vk::Result::eSuccess != result) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to create vertex buffer: #%d", result);
    return false;
  }

  vk::MemoryRequirements memory_requirements;
  device_.getBufferMemoryRequirements(vertex_buffer_, &memory_requirements);
  vk::PhysicalDeviceMemoryProperties memory_properties;
  physical_device_.getMemoryProperties(&memory_properties);

  auto memory_type_index{std::numeric_limits<std::uint32_t>::max()};
  for (std::uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i) {
    if ((memory_requirements.memoryTypeBits & (1 << i)) &&
        (memory_properties.memoryTypes[i].propertyFlags &
         (vk::MemoryPropertyFlagBits::eHostVisible |
          vk::MemoryPropertyFlagBits::eHostCoherent))) {
      memory_type_index = i;
      break;
    }
  }
  if (std::numeric_limits<std::uint32_t>::max() == memory_type_index) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to find suitable memory type for vertex buffer");
    return false;
  }

  vk::MemoryAllocateInfo alloc_info{memory_requirements.size,
                                    memory_type_index};
  if (vk::Result result =
          device_.allocateMemory(&alloc_info, nullptr, &vertex_buffer_memory_);
      vk::Result::eSuccess != result) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to allocate memory for vertex buffer: #%d", result);
    return false;
  }

  if (vk::Result result =
          device_.bindBufferMemory(vertex_buffer_, vertex_buffer_memory_, 0);
      vk::Result::eSuccess != result) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to bind vertex buffer memory: #%d", result);
    return false;
  }
  auto data = device_.mapMemory(vertex_buffer_memory_, 0, buffer_info.size, {});
  if (!data.has_value()) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to map vertex buffer memory: #%d", data.result);
    return false;
  }
  std::memcpy(data.value, kVertices.data(), sizeof(kVertices));
  device_.unmapMemory(vertex_buffer_memory_);

  return true;
}

// 13
bool VkTriangle::CreateCommandBuffers() noexcept {
  vk::CommandBufferAllocateInfo alloc_info{
      command_pool_, vk::CommandBufferLevel::ePrimary,
      gsl::narrow_cast<std::uint32_t>(swapchain_framebuffers_.size())};
  auto command_buffers = device_.allocateCommandBuffers(alloc_info);
  if (!command_buffers.has_value()) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to allocate command buffers: #%d",
                 command_buffers.result);
    return false;
  }
  command_buffers_ = std::move(command_buffers).value;

  return true;
}

// 14
bool VkTriangle::RecordCommandBuffers() noexcept {
  for (auto& command_buffer : command_buffers_) {
    vk::CommandBufferBeginInfo begin_info{};
    if (vk::Result result = command_buffer.begin(&begin_info);
        vk::Result::eSuccess != result) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                   "Failed to begin recording command buffer: #%d", result);
      return false;
    }
    vk::ClearValue clear{std::array<float, 4>{0.0f, 0.25f, 0.0f, 1.0f}};
    vk::RenderPassBeginInfo render_pass_info{
        render_pass_,
        swapchain_framebuffers_[&command_buffer - &command_buffers_[0]],
        {{0, 0}, swapchain_extent_},
        1,
        &clear};
    command_buffer.beginRenderPass(&render_pass_info,
                                   vk::SubpassContents::eInline);
    command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                graphics_pipeline_);
    vk::Buffer vertex_buffers[] = {vertex_buffer_};
    vk::DeviceSize offsets[] = {0};
    command_buffer.bindVertexBuffers(0, 1, vertex_buffers, offsets);
    command_buffer.draw(static_cast<uint32_t>(kVertices.size()), 1, 0, 0);
    command_buffer.endRenderPass();
    if (vk::Result result = command_buffer.end();
        vk::Result::eSuccess != result) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                   "Failed to end recording command buffer: #%d", result);
      return false;
    }
  }
  return true;
}

// 15
bool VkTriangle::CreateSynchronizationObjects() noexcept {
  vk::FenceCreateInfo fence_info{vk::FenceCreateFlagBits::eSignaled};
  if (vk::Result result =
          device_.createFence(&fence_info, nullptr, &in_flight_fence_);
      vk::Result::eSuccess != result) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create fence: #%d",
                 result);
    return false;
  }

  vk::SemaphoreCreateInfo semaphore_info{};
  if (vk::Result result = device_.createSemaphore(&semaphore_info, nullptr,
                                                  &image_available_semaphore_);
      vk::Result::eSuccess != result) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to create image available semaphore: #%d", result);
    return false;
  }

  if (vk::Result result = device_.createSemaphore(&semaphore_info, nullptr,
                                                  &render_finished_semaphore_);
      vk::Result::eSuccess != result) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to create render finished semaphore: #%d", result);
    return false;
  }

  return true;
}
