#define _HAS_EXCEPTIONS 0
#include <array>
#include <fstream>
#include <vector>

#include <gsl/util>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#if defined(_WIN32)
#define VK_USE_PLATFORM_WIN32_KHR
#elif defined(__linux__) || defined(__unix__)
#define VK_USE_PLATFORM_XLIB_KHR
#elif defined(__APPLE__)
#define VK_USE_PLATFORM_MACOS_MVK
#else
#endif
#define VK_NO_PROTOTYPES
#include <volk.h>

#include <glm/glm.hpp>

struct Vertex {
  glm::vec2 pos;
  glm::vec3 color;

  static VkVertexInputBindingDescription GetBindingDescription() noexcept {
    VkVertexInputBindingDescription binding_description{};
    binding_description.binding = 0;
    binding_description.stride = sizeof(Vertex);
    binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return binding_description;
  }

  static std::array<VkVertexInputAttributeDescription, 2>
  GetAttributeDescriptions() noexcept {
    std::array<VkVertexInputAttributeDescription, 2> attribute_descriptions{};
    attribute_descriptions[0].binding = 0;
    attribute_descriptions[0].location = 0;
    attribute_descriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    attribute_descriptions[0].offset = offsetof(Vertex, pos);
    attribute_descriptions[1].binding = 0;
    attribute_descriptions[1].location = 1;
    attribute_descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribute_descriptions[1].offset = offsetof(Vertex, color);
    return attribute_descriptions;
  }
};

// UMU: This is just a placeholder. What really works is the Vertex Shader.
constexpr std::array<Vertex, 3> kVertices{
    Vertex{{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    Vertex{{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
    Vertex{{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}};

std::vector<std::byte> ReadFile(std::string_view filename) {
  std::ifstream file(filename.data(),
                     std::ios::in | std::ios::ate | std::ios::binary);
  if (!file.is_open()) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to open file: %.*s",
                 gsl::narrow_cast<int>(filename.size()), filename.data());
    return {};
  }
  std::size_t file_size = file.tellg();
  std::vector<std::byte> buffer(file_size);
  file.seekg(0);
  file.read(reinterpret_cast<char*>(buffer.data()), file_size);
  file.close();
  return buffer;
}

int main(int /*argc*/, char* /*argv*/[]) {
  // Initialize SDL
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed: %s",
                 SDL_GetError());
    return EXIT_FAILURE;
  }
  gsl::final_action sdl_cleanup{[]() { SDL_Quit(); }};

  // Create window
  SDL_Window* window =
      SDL_CreateWindow("SDL3 Vulkan Triangle", 800, 618,
                       SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
  if (!window) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Window creation failed: %s",
                 SDL_GetError());
    return EXIT_FAILURE;
  }
  gsl::final_action window_cleanup{[window]() { SDL_DestroyWindow(window); }};

  // Initialize Volk
  if (VkResult result = volkInitialize(); result != VK_SUCCESS) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize Volk: #%d",
                 result);
    return EXIT_FAILURE;
  }
  // UMU: volkFinalize() does not need to be called on process exit

  // 1. Create Vulkan instance
  std::uint32_t extension_count{};
  auto extensions = SDL_Vulkan_GetInstanceExtensions(&extension_count);
  if (!extensions) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to get Vulkan instance extensions: %s",
                 SDL_GetError());
    return EXIT_FAILURE;
  }
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Required Vulkan extensions: %u",
              extension_count);
  for (std::uint32_t i = 0; i < extension_count; ++i) {
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Extension %u: %s", i,
                extensions[i]);
  }

  VkInstance instance;
  {
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Vulkan Triangle";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "No Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = extension_count;
    create_info.ppEnabledExtensionNames = extensions;

    if (vkCreateInstance(&create_info, nullptr, &instance) != VK_SUCCESS) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                   "Failed to create Vulkan instance");
      return EXIT_FAILURE;
    }
  }
  gsl::final_action instance_cleanup{
      [instance]() { vkDestroyInstance(instance, nullptr); }};
  volkLoadInstance(instance);
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
              "Successfully created Vulkan instance");

  // 2. Create surface
  VkSurfaceKHR surface;
  if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to create Vulkan surface: %s", SDL_GetError());
    return EXIT_FAILURE;
  }
  gsl::final_action surface_cleanup{[instance, surface]() {
    vkDestroySurfaceKHR(instance, surface, nullptr);
  }};
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
              "Successfully created Vulkan surface");

  // 3. Pick physical device
  VkPhysicalDevice physical_device{VK_NULL_HANDLE};
  auto graphics_family{std::numeric_limits<std::uint32_t>::max()};
  auto present_family{std::numeric_limits<std::uint32_t>::max()};
  {
    std::uint32_t device_count{};
    if (VkResult result =
            vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
        VK_SUCCESS != result) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                   "Failed to enumerate physical devices: #%d", result);
      return EXIT_FAILURE;
    }
    if (device_count == 0) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                   "No Vulkan-capable devices found");
      return EXIT_FAILURE;
    }
    std::vector<VkPhysicalDevice> devices(device_count);
    if (VkResult result =
            vkEnumeratePhysicalDevices(instance, &device_count, devices.data());
        VK_SUCCESS != result) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                   "Failed to get %u physical devices: #%d", device_count,
                   result);
      return EXIT_FAILURE;
    }

    for (const auto& device : devices) {
      std::uint32_t queue_family_count{};
      vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count,
                                               nullptr);
      std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
      vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count,
                                               queue_families.data());

      bool has_graphics{};
      bool has_present{};
      for (std::uint32_t i = 0; i < queue_family_count; ++i) {
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
          graphics_family = i;
          has_graphics = true;
        }
        VkBool32 present_support{};
        if (VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(
                device, i, surface, &present_support);
            VK_SUCCESS != result) {
          SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                       "Failed to get physical device surface support: %d",
                       result);
          continue;
        }
        if (present_support) {
          present_family = i;
          has_present = true;
        }
      }
      if (has_graphics && has_present) {
        physical_device = device;
        break;
      }
    }
    if (VK_NULL_HANDLE == physical_device) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                   "No suitable Vulkan device found");
      return EXIT_FAILURE;
    }
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Selected physical device");
  }

  // 4. Create logical device
  VkDevice device;
  {
    float queue_priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    VkDeviceQueueCreateInfo graphics_queue_create_info{};
    graphics_queue_create_info.sType =
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    graphics_queue_create_info.queueFamilyIndex = graphics_family;
    graphics_queue_create_info.queueCount = 1;
    graphics_queue_create_info.pQueuePriorities = &queue_priority;
    queue_create_infos.push_back(graphics_queue_create_info);

    if (graphics_family != present_family) {
      VkDeviceQueueCreateInfo present_queue_create_info{};
      present_queue_create_info.sType =
          VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      present_queue_create_info.queueFamilyIndex = present_family;
      present_queue_create_info.queueCount = 1;
      present_queue_create_info.pQueuePriorities = &queue_priority;
      queue_create_infos.push_back(present_queue_create_info);
    }

    VkPhysicalDeviceFeatures device_features{};
    VkDeviceCreateInfo device_create_info{};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.queueCreateInfoCount =
        gsl::narrow_cast<uint32_t>(queue_create_infos.size());
    device_create_info.pQueueCreateInfos = queue_create_infos.data();
    device_create_info.pEnabledFeatures = &device_features;
    constexpr std::array<const char*, 1> device_extensions{
        VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    device_create_info.enabledExtensionCount =
        gsl::narrow_cast<uint32_t>(device_extensions.size());
    device_create_info.ppEnabledExtensionNames = device_extensions.data();

    if (VkResult result = vkCreateDevice(physical_device, &device_create_info,
                                         nullptr, &device);
        VK_SUCCESS != result) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                   "Failed to create logical device: #%d", result);
      return EXIT_FAILURE;
    }
  }
  gsl::final_action device_cleanup{
      [device]() { vkDestroyDevice(device, nullptr); }};
  volkLoadDevice(device);
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
              "Successfully created logical device");

  VkQueue graphics_queue;
  vkGetDeviceQueue(device, graphics_family, 0, &graphics_queue);
  VkQueue present_queue;
  vkGetDeviceQueue(device, present_family, 0, &present_queue);

  // 5. Create swapchain
  VkSurfaceFormatKHR surface_format{};
  VkExtent2D swapchain_extent{};
  VkSwapchainKHR swapchain{};
  {
    VkSurfaceCapabilitiesKHR capabilities;
    if (VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            physical_device, surface, &capabilities);
        VK_SUCCESS != result) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                   "Failed to get physical device surface capabilities: %d",
                   result);
      return EXIT_FAILURE;
    }

    std::uint32_t format_count;
    if (VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(
            physical_device, surface, &format_count, nullptr);
        VK_SUCCESS != result) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                   "Failed to get physical device surface formats: #%d",
                   result);
      return EXIT_FAILURE;
    }
    std::vector<VkSurfaceFormatKHR> formats(format_count);
    if (VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(
            physical_device, surface, &format_count, formats.data());
        VK_SUCCESS != result) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                   "Failed to get %u physical device surface formats: #%d",
                   format_count, result);
      return EXIT_FAILURE;
    }

    surface_format = formats[0];
    for (const auto& format : formats) {
      if ((format.format == VK_FORMAT_B8G8R8A8_UNORM ||
           format.format == VK_FORMAT_R8G8B8A8_UNORM) &&
          format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
        surface_format = format;
        break;
      }
    }

    swapchain_extent = capabilities.currentExtent;
    if (UINT32_MAX == swapchain_extent.width) {
      int width, height;
      SDL_GetWindowSizeInPixels(window, &width, &height);
      swapchain_extent.width = static_cast<uint32_t>(width);
      swapchain_extent.height = static_cast<uint32_t>(height);
    }

    VkSwapchainCreateInfoKHR swapchain_info{};
    swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_info.surface = surface;
    swapchain_info.minImageCount = 2;
    swapchain_info.imageFormat = surface_format.format;
    swapchain_info.imageColorSpace = surface_format.colorSpace;
    swapchain_info.imageExtent = swapchain_extent;
    swapchain_info.imageArrayLayers = 1;
    swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_info.preTransform = capabilities.currentTransform;
    swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchain_info.clipped = VK_TRUE;

    if (VkResult result =
            vkCreateSwapchainKHR(device, &swapchain_info, nullptr, &swapchain);
        VK_SUCCESS != result) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                   "Failed to create swapchain: %d", result);
      return EXIT_FAILURE;
    }
  }
  // swapchain will be recreated on window resize, so we use reference capture
  // here to ensure the cleanup lambda always has the latest swapchain handle
  gsl::final_action swapchain_cleanup{[device, &swapchain]() {
    vkDestroySwapchainKHR(device, swapchain, nullptr);
  }};
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Successfully created swapchain");

  std::uint32_t image_count;
  if (VkResult result =
          vkGetSwapchainImagesKHR(device, swapchain, &image_count, nullptr);
      VK_SUCCESS != result) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to get swapchain images: #%d", result);
    return EXIT_FAILURE;
  }
  std::vector<VkImage> swapchain_images(image_count);
  if (VkResult result = vkGetSwapchainImagesKHR(device, swapchain, &image_count,
                                                swapchain_images.data());
      VK_SUCCESS != result) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to get %u swapchain images: #%d", image_count, result);
    return EXIT_FAILURE;
  }

  // 6. Create image views
  std::vector<VkImageView> swapchain_image_views(image_count);
  for (std::size_t i = 0; i < image_count; ++i) {
    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = swapchain_images[i];
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = surface_format.format;
    view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &view_info, nullptr,
                          &swapchain_image_views[i]) != VK_SUCCESS) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                   "Failed to create image view %zu", i);
      for (std::size_t j = 0; j < i; ++j) {
        vkDestroyImageView(device, swapchain_image_views[j], nullptr);
      }
      return EXIT_FAILURE;
    }
  }
  gsl::final_action image_views_cleanup{[device, &swapchain_image_views]() {
    for (auto& view : swapchain_image_views) {
      vkDestroyImageView(device, view, nullptr);
    }
  }};
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
              "Successfully created %u image views", image_count);

  // 7. Create render pass
  VkRenderPass render_pass{};
  {
    VkAttachmentDescription color_attachment{};
    color_attachment.format = surface_format.format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_ref{};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;

    VkRenderPassCreateInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &color_attachment;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;

    if (VkResult result = vkCreateRenderPass(device, &render_pass_info, nullptr,
                                             &render_pass);
        VK_SUCCESS != result) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                   "Failed to create render pass: #%d", result);
      return EXIT_FAILURE;
    }
  }
  gsl::final_action render_pass_cleanup{[device, render_pass]() {
    vkDestroyRenderPass(device, render_pass, nullptr);
  }};
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Successfully created render pass");

  // 8. Load and create shader modules
  VkShaderModule vert_shader_module{};
  {
    auto vert_shader_code = ReadFile("assets/triangle_vert.spv");
    if (vert_shader_code.empty()) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                   "Failed to load vertex shader file");
      return EXIT_FAILURE;
    }
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Successfully loaded vertex shader (%zu bytes)",
                vert_shader_code.size());

    VkShaderModuleCreateInfo vert_module_info{};
    vert_module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vert_module_info.codeSize = vert_shader_code.size();
    vert_module_info.pCode =
        reinterpret_cast<const std::uint32_t*>(vert_shader_code.data());

    if (VkResult result = vkCreateShaderModule(device, &vert_module_info,
                                               nullptr, &vert_shader_module);
        VK_SUCCESS != result) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                   "Failed to create vertex shader module: #%d", result);
      return EXIT_FAILURE;
    }
  }
  gsl::final_action vert_shader_cleanup{[device, vert_shader_module]() {
    vkDestroyShaderModule(device, vert_shader_module, nullptr);
  }};

  VkShaderModule frag_shader_module{};
  {
    auto frag_shader_code = ReadFile("assets/triangle_frag.spv");
    if (frag_shader_code.empty()) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                   "Failed to load fragment shader file");
      return EXIT_FAILURE;
    }
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Successfully loaded fragment shader (%zu bytes)",
                frag_shader_code.size());

    VkShaderModuleCreateInfo frag_module_info{};
    frag_module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    frag_module_info.codeSize = frag_shader_code.size();
    frag_module_info.pCode =
        reinterpret_cast<const std::uint32_t*>(frag_shader_code.data());

    if (VkResult result = vkCreateShaderModule(device, &frag_module_info,
                                               nullptr, &frag_shader_module);
        VK_SUCCESS != result) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                   "Failed to create fragment shader module: #%d", result);
      return EXIT_FAILURE;
    }
  }
  gsl::final_action frag_shader_cleanup{[device, frag_shader_module]() {
    vkDestroyShaderModule(device, frag_shader_module, nullptr);
  }};

  // 9. Create graphics pipeline
  VkPipeline graphics_pipeline{};
  {
    // vkCreatePipelineLayout
    VkPipelineShaderStageCreateInfo vert_stage_info{};
    vert_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_stage_info.module = vert_shader_module;
    vert_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo frag_stage_info{};
    frag_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_stage_info.module = frag_shader_module;
    frag_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo shader_stages[] = {vert_stage_info,
                                                       frag_stage_info};

    auto binding_description = Vertex::GetBindingDescription();
    auto attribute_descriptions = Vertex::GetAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertex_input_info{};
    vertex_input_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = 1;
    vertex_input_info.pVertexBindingDescriptions = &binding_description;
    vertex_input_info.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(attribute_descriptions.size());
    vertex_input_info.pVertexAttributeDescriptions =
        attribute_descriptions.data();

    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapchain_extent.width);
    viewport.height = static_cast<float>(swapchain_extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapchain_extent;

    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState color_blend_attachment{};
    color_blend_attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo color_blending{};
    color_blending.sType =
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;

    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    VkPipelineLayout pipeline_layout;
    if (VkResult result = vkCreatePipelineLayout(device, &pipeline_layout_info,
                                                 nullptr, &pipeline_layout);
        result != VK_SUCCESS) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                   "Failed to create pipeline layout: #%d", result);
      return EXIT_FAILURE;
    }

    gsl::final_action pipeline_layout_cleanup{[device, pipeline_layout]() {
      vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
    }};

    // vkCreateGraphicsPipelines
    {
      VkGraphicsPipelineCreateInfo pipeline_info{};
      pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
      pipeline_info.stageCount = 2;
      pipeline_info.pStages = shader_stages;
      pipeline_info.pVertexInputState = &vertex_input_info;
      pipeline_info.pInputAssemblyState = &input_assembly;
      pipeline_info.pViewportState = &viewport_state;
      pipeline_info.pRasterizationState = &rasterizer;
      pipeline_info.pMultisampleState = &multisampling;
      pipeline_info.pColorBlendState = &color_blending;
      pipeline_info.layout = pipeline_layout;
      pipeline_info.renderPass = render_pass;
      pipeline_info.subpass = 0;

      if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info,
                                    nullptr,
                                    &graphics_pipeline) != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to create graphics pipeline");
        return EXIT_FAILURE;
      }
    }
  }
  gsl::final_action graphics_pipeline_cleanup{[device, &graphics_pipeline]() {
    vkDestroyPipeline(device, graphics_pipeline, nullptr);
  }};
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
              "Successfully created graphics pipeline");

  // 10. Create framebuffers
  std::vector<VkFramebuffer> swapchain_framebuffers(image_count);
  for (std::size_t i = 0; i < image_count; ++i) {
    VkImageView attachments[] = {swapchain_image_views[i]};
    VkFramebufferCreateInfo framebuffer_info{};
    framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_info.renderPass = render_pass;
    framebuffer_info.attachmentCount = 1;
    framebuffer_info.pAttachments = attachments;
    framebuffer_info.width = swapchain_extent.width;
    framebuffer_info.height = swapchain_extent.height;
    framebuffer_info.layers = 1;

    if (VkResult result = vkCreateFramebuffer(
            device, &framebuffer_info, nullptr, &swapchain_framebuffers[i]);
        result != VK_SUCCESS) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                   "Failed to create framebuffer %zu: #%d", i, result);
      for (std::size_t j = 0; j < i; ++j) {
        vkDestroyFramebuffer(device, swapchain_framebuffers[j], nullptr);
      }
      return EXIT_FAILURE;
    }
  }
  gsl::final_action framebuffers_cleanup{[device, &swapchain_framebuffers]() {
    for (auto& fb : swapchain_framebuffers) {
      vkDestroyFramebuffer(device, fb, nullptr);
    }
  }};
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
              "Successfully created %u framebuffers", image_count);

  // 11. Create command pool
  VkCommandPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.queueFamilyIndex = graphics_family;
  pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

  VkCommandPool command_pool;
  if (VkResult result =
          vkCreateCommandPool(device, &pool_info, nullptr, &command_pool);
      result != VK_SUCCESS) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to create command pool: #%d", result);
    return EXIT_FAILURE;
  }
  gsl::final_action command_pool_cleanup{[device, command_pool]() {
    vkDestroyCommandPool(device, command_pool, nullptr);
  }};
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
              "Successfully created command pool");

  // 12. Create vertex buffer
  VkBuffer vertex_buffer;
  VkDeviceMemory vertex_buffer_memory;
  VkBufferCreateInfo buffer_info{};
  buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_info.size = sizeof(kVertices[0]) * kVertices.size();
  buffer_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  if (VkResult result =
          vkCreateBuffer(device, &buffer_info, nullptr, &vertex_buffer);
      result != VK_SUCCESS) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to create vertex buffer: #%d", result);
    return EXIT_FAILURE;
  }
  gsl::final_action vertex_buffer_cleanup{[device, vertex_buffer]() {
    vkDestroyBuffer(device, vertex_buffer, nullptr);
  }};

  VkMemoryRequirements memory_requirements;
  vkGetBufferMemoryRequirements(device, vertex_buffer, &memory_requirements);

  VkPhysicalDeviceMemoryProperties memory_properties;
  vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);

  auto memory_type_index{std::numeric_limits<std::uint32_t>::max()};
  for (std::uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i) {
    if ((memory_requirements.memoryTypeBits & (1 << i)) &&
        (memory_properties.memoryTypes[i].propertyFlags &
         (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))) {
      memory_type_index = i;
      break;
    }
  }
  if (std::numeric_limits<std::uint32_t>::max() == memory_type_index) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to find suitable memory type for vertex buffer");
    return EXIT_FAILURE;
  }

  VkMemoryAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc_info.allocationSize = memory_requirements.size;
  alloc_info.memoryTypeIndex = memory_type_index;
  if (VkResult result =
          vkAllocateMemory(device, &alloc_info, nullptr, &vertex_buffer_memory);
      VK_SUCCESS != result) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to allocate vertex buffer memory: #%d", result);
    return EXIT_FAILURE;
  }
  gsl::final_action vertex_buffer_memory_cleanup{
      [device, vertex_buffer_memory]() {
        vkFreeMemory(device, vertex_buffer_memory, nullptr);
      }};

  if (VkResult result =
          vkBindBufferMemory(device, vertex_buffer, vertex_buffer_memory, 0);
      VK_SUCCESS != result) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to bind vertex buffer memory: #%d", result);
  }
  void* data{};
  if (VkResult result = vkMapMemory(device, vertex_buffer_memory, 0,
                                    buffer_info.size, 0, &data);
      VK_SUCCESS != result) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to map vertex buffer memory: #%d", result);
    return EXIT_FAILURE;
  }
  std::memcpy(data, kVertices.data(),
              static_cast<std::size_t>(buffer_info.size));
  vkUnmapMemory(device, vertex_buffer_memory);
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
              "Successfully created vertex buffer");

  // 13. Create command buffers (one per swapchain image)
  std::vector<VkCommandBuffer> command_buffers(image_count);
  VkCommandBufferAllocateInfo alloc_info_cmd{};
  alloc_info_cmd.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc_info_cmd.commandPool = command_pool;
  alloc_info_cmd.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  alloc_info_cmd.commandBufferCount = image_count;
  if (VkResult result = vkAllocateCommandBuffers(device, &alloc_info_cmd,
                                                 command_buffers.data());
      VK_SUCCESS != result) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to allocate command buffers: #%d", result);
    return EXIT_FAILURE;
  }
  gsl::final_action command_buffers_cleanup{
      [device, command_pool, &command_buffers]() {
        vkFreeCommandBuffers(device, command_pool,
                             static_cast<std::uint32_t>(command_buffers.size()),
                             command_buffers.data());
      }};
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
              "Successfully allocated %u command buffers", image_count);

  // 14. Record command buffers
  for (std::size_t i = 0; i < image_count; ++i) {
    VkCommandBufferBeginInfo cb_begin_info{};
    cb_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (VkResult result =
            vkBeginCommandBuffer(command_buffers[i], &cb_begin_info);
        VK_SUCCESS != result) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                   "Failed to begin recording command buffer %zu, #%d", i,
                   result);
      return EXIT_FAILURE;
    }

    VkRenderPassBeginInfo rp_begin_info{};
    rp_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_begin_info.renderPass = render_pass;
    rp_begin_info.framebuffer = swapchain_framebuffers[i];
    rp_begin_info.renderArea.offset = {0, 0};
    rp_begin_info.renderArea.extent = swapchain_extent;
    VkClearValue clear_color = {{{0.0f, 0.25f, 0.0f, 1.0f}}};
    rp_begin_info.clearValueCount = 1;
    rp_begin_info.pClearValues = &clear_color;

    vkCmdBeginRenderPass(command_buffers[i], &rp_begin_info,
                         VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS,
                      graphics_pipeline);
    VkBuffer vertex_buffers[] = {vertex_buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(command_buffers[i], 0, 1, vertex_buffers, offsets);
    vkCmdDraw(command_buffers[i], static_cast<uint32_t>(kVertices.size()), 1, 0,
              0);
    vkCmdEndRenderPass(command_buffers[i]);

    if (VkResult result = vkEndCommandBuffer(command_buffers[i]);
        VK_SUCCESS != result) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                   "Failed to record command buffer %zu, #%d", i, result);
      return EXIT_FAILURE;
    }
  }
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
              "Successfully recorded %u command buffers", image_count);

  // 15. Create synchronization objects
  VkFence in_flight_fence;
  VkFenceCreateInfo fence_info{};
  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
  if (vkCreateFence(device, &fence_info, nullptr, &in_flight_fence) !=
      VK_SUCCESS) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create fence");
    return EXIT_FAILURE;
  }
  gsl::final_action fence_cleanup{[device, in_flight_fence]() {
    vkDestroyFence(device, in_flight_fence, nullptr);
  }};

  VkSemaphore image_available_semaphore;
  VkSemaphoreCreateInfo semaphore_info{};
  semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  if (vkCreateSemaphore(device, &semaphore_info, nullptr,
                        &image_available_semaphore) != VK_SUCCESS) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create semaphore");
    return EXIT_FAILURE;
  }
  gsl::final_action image_available_semaphore_cleanup{
      [device, image_available_semaphore]() {
        vkDestroySemaphore(device, image_available_semaphore, nullptr);
      }};

  VkSemaphore render_finished_semaphore;
  if (vkCreateSemaphore(device, &semaphore_info, nullptr,
                        &render_finished_semaphore) != VK_SUCCESS) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create semaphore");
    return EXIT_FAILURE;
  }
  gsl::final_action render_finished_semaphore_cleanup{
      [device, render_finished_semaphore]() {
        vkDestroySemaphore(device, render_finished_semaphore, nullptr);
      }};
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
              "Successfully created synchronization objects");

  // Main loop
  bool framebuffer_resized = false;
  SDL_Event event;
  for (bool should_quit{}; !should_quit;) {
    while (SDL_PollEvent(&event)) {
      if (SDL_EVENT_QUIT == event.type) {
        should_quit = true;
      } else if (SDL_EVENT_KEY_DOWN == event.type &&
                 SDLK_ESCAPE == event.key.key) {
        should_quit = true;
      } else if (SDL_EVENT_WINDOW_RESIZED == event.type) {
        framebuffer_resized = true;
      }
    }

    if (framebuffer_resized) {
      framebuffer_resized = false;

      // Recreate swapchain and related resources
      vkDestroyPipeline(device, graphics_pipeline, nullptr);
      // Not use VK_KHR_dynamic_rendering, need to recreate framebuffer
      for (auto& fb : swapchain_framebuffers) {
        vkDestroyFramebuffer(device, fb, nullptr);
      }
      for (auto& view : swapchain_image_views) {
        vkDestroyImageView(device, view, nullptr);
      }
      vkDestroySwapchainKHR(device, swapchain, nullptr);

      // 5. Recreate swapchain
      VkSurfaceCapabilitiesKHR capabilities;
      if (VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
              physical_device, surface, &capabilities);
          VK_SUCCESS != result) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to get surface capabilities: %d", result);
        break;
      }

      swapchain_extent = capabilities.currentExtent;
      if (UINT32_MAX == swapchain_extent.width) {
        int width, height;
        SDL_GetWindowSizeInPixels(window, &width, &height);
        swapchain_extent.width = static_cast<uint32_t>(width);
        swapchain_extent.height = static_cast<uint32_t>(height);
      }

      VkSwapchainCreateInfoKHR swapchain_info{};
      swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
      swapchain_info.surface = surface;
      swapchain_info.minImageCount = 2;
      swapchain_info.imageFormat = surface_format.format;
      swapchain_info.imageColorSpace = surface_format.colorSpace;
      swapchain_info.imageExtent = swapchain_extent;
      swapchain_info.imageArrayLayers = 1;
      swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
      swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
      swapchain_info.preTransform = capabilities.currentTransform;
      swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
      swapchain_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
      swapchain_info.clipped = VK_TRUE;
      if (VkResult result = vkCreateSwapchainKHR(device, &swapchain_info,
                                                 nullptr, &swapchain);
          VK_SUCCESS != result) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to recreate swapchain: %d", result);
        break;
      }

      if (VkResult result =
              vkGetSwapchainImagesKHR(device, swapchain, &image_count, nullptr);
          VK_SUCCESS != result) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to get swapchain images: #%d", result);
        break;
      }
      swapchain_images.resize(image_count);
      if (VkResult result = vkGetSwapchainImagesKHR(
              device, swapchain, &image_count, swapchain_images.data());
          VK_SUCCESS != result) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to get swapchain images: #%d", result);
        break;
      }

      // 6. Recreate image views
      swapchain_image_views.resize(image_count);
      for (std::size_t i = 0; i < image_count; ++i) {
        VkImageViewCreateInfo view_info{};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = swapchain_images[i];
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = surface_format.format;
        view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &view_info, nullptr,
                              &swapchain_image_views[i]) != VK_SUCCESS) {
          SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                       "Failed to recreate image view %zu", i);
          should_quit = true;
          break;
        }
      }
      if (should_quit) {
        break;
      }

      // 9. Recreate graphics pipeline
      {
        // vkCreatePipelineLayout
        VkPipelineShaderStageCreateInfo vert_stage_info{};
        vert_stage_info.sType =
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vert_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vert_stage_info.module = vert_shader_module;
        vert_stage_info.pName = "main";

        VkPipelineShaderStageCreateInfo frag_stage_info{};
        frag_stage_info.sType =
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        frag_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        frag_stage_info.module = frag_shader_module;
        frag_stage_info.pName = "main";

        VkPipelineShaderStageCreateInfo shader_stages[] = {vert_stage_info,
                                                           frag_stage_info};

        auto binding_description = Vertex::GetBindingDescription();
        auto attribute_descriptions = Vertex::GetAttributeDescriptions();

        VkPipelineVertexInputStateCreateInfo vertex_input_info{};
        vertex_input_info.sType =
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input_info.vertexBindingDescriptionCount = 1;
        vertex_input_info.pVertexBindingDescriptions = &binding_description;
        vertex_input_info.vertexAttributeDescriptionCount =
            static_cast<uint32_t>(attribute_descriptions.size());
        vertex_input_info.pVertexAttributeDescriptions =
            attribute_descriptions.data();

        VkPipelineInputAssemblyStateCreateInfo input_assembly{};
        input_assembly.sType =
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(swapchain_extent.width);
        viewport.height = static_cast<float>(swapchain_extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapchain_extent;

        VkPipelineViewportStateCreateInfo viewport_state{};
        viewport_state.sType =
            VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state.viewportCount = 1;
        viewport_state.pViewports = &viewport;
        viewport_state.scissorCount = 1;
        viewport_state.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType =
            VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType =
            VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState color_blend_attachment{};
        color_blend_attachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        color_blend_attachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo color_blending{};
        color_blending.sType =
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blending.logicOpEnable = VK_FALSE;
        color_blending.attachmentCount = 1;
        color_blending.pAttachments = &color_blend_attachment;

        VkPipelineLayoutCreateInfo pipeline_layout_info{};
        pipeline_layout_info.sType =
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        VkPipelineLayout pipeline_layout;
        if (VkResult result = vkCreatePipelineLayout(
                device, &pipeline_layout_info, nullptr, &pipeline_layout);
            VK_SUCCESS != result) {
          SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                       "Failed to create pipeline layout: #%d", result);
          return EXIT_FAILURE;
        }

        gsl::final_action pipeline_layout_cleanup{[device, pipeline_layout]() {
          vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
        }};

        // vkCreateGraphicsPipelines
        {
          VkGraphicsPipelineCreateInfo pipeline_info{};
          pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
          pipeline_info.stageCount = 2;
          pipeline_info.pStages = shader_stages;
          pipeline_info.pVertexInputState = &vertex_input_info;
          pipeline_info.pInputAssemblyState = &input_assembly;
          pipeline_info.pViewportState = &viewport_state;
          pipeline_info.pRasterizationState = &rasterizer;
          pipeline_info.pMultisampleState = &multisampling;
          pipeline_info.pColorBlendState = &color_blending;
          pipeline_info.layout = pipeline_layout;
          pipeline_info.renderPass = render_pass;
          pipeline_info.subpass = 0;

          if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1,
                                        &pipeline_info, nullptr,
                                        &graphics_pipeline) != VK_SUCCESS) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Failed to create graphics pipeline");
            return EXIT_FAILURE;
          }
        }
      }

      // 10. Recreate framebuffers
      swapchain_framebuffers.resize(image_count);
      for (std::size_t i = 0; i < image_count; ++i) {
        VkImageView attachments[] = {swapchain_image_views[i]};
        VkFramebufferCreateInfo framebuffer_info{};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = render_pass;
        framebuffer_info.attachmentCount = 1;
        framebuffer_info.pAttachments = attachments;
        framebuffer_info.width = swapchain_extent.width;
        framebuffer_info.height = swapchain_extent.height;
        framebuffer_info.layers = 1;

        if (VkResult result = vkCreateFramebuffer(
                device, &framebuffer_info, nullptr, &swapchain_framebuffers[i]);
            result != VK_SUCCESS) {
          SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                       "Failed to recreate framebuffer %zu: #%d", i, result);
          should_quit = true;
          break;
        }
      }
      if (should_quit) {
        break;
      }

      // 13. Recreate command buffers (one per swapchain image)
      vkFreeCommandBuffers(device, command_pool,
                           static_cast<std::uint32_t>(command_buffers.size()),
                           command_buffers.data());
      command_buffers.resize(image_count);
      alloc_info_cmd.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      alloc_info_cmd.commandPool = command_pool;
      alloc_info_cmd.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      alloc_info_cmd.commandBufferCount = image_count;
      if (VkResult result = vkAllocateCommandBuffers(device, &alloc_info_cmd,
                                                     command_buffers.data());
          VK_SUCCESS != result) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to allocate command buffers: #%d", result);
        break;
      }

      // 14. Record command buffers again
      for (std::size_t i = 0; i < image_count; ++i) {
        VkCommandBufferBeginInfo cb_begin_info{};
        cb_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (VkResult result =
                vkBeginCommandBuffer(command_buffers[i], &cb_begin_info);
            VK_SUCCESS != result) {
          SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                       "Failed to begin recording command buffer %zu, #%d", i,
                       result);
          should_quit = true;
          break;
        }

        VkRenderPassBeginInfo rp_begin_info{};
        rp_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rp_begin_info.renderPass = render_pass;
        rp_begin_info.framebuffer = swapchain_framebuffers[i];
        rp_begin_info.renderArea.offset = {0, 0};
        rp_begin_info.renderArea.extent = swapchain_extent;
        VkClearValue clear_color = {{{0.0f, 0.25f, 0.0f, 1.0f}}};
        rp_begin_info.clearValueCount = 1;
        rp_begin_info.pClearValues = &clear_color;

        vkCmdBeginRenderPass(command_buffers[i], &rp_begin_info,
                             VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS,
                          graphics_pipeline);
        VkBuffer vertex_buffers[] = {vertex_buffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(command_buffers[i], 0, 1, vertex_buffers,
                               offsets);
        vkCmdDraw(command_buffers[i], static_cast<uint32_t>(kVertices.size()),
                  1, 0, 0);
        vkCmdEndRenderPass(command_buffers[i]);

        if (VkResult result = vkEndCommandBuffer(command_buffers[i]);
            VK_SUCCESS != result) {
          SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                       "Failed to record command buffer %zu, #%d", i, result);
          should_quit = true;
          break;
        }
      }
      if (should_quit) {
        break;
      }
    }

    vkWaitForFences(device, 1, &in_flight_fence, VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &in_flight_fence);
#if _DEBUG
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Waiting for fence and resetting");
#endif

    std::uint32_t image_index;
    if (VkResult acquire_result = vkAcquireNextImageKHR(
            device, swapchain, UINT64_MAX, image_available_semaphore,
            VK_NULL_HANDLE, &image_index);
        VK_SUCCESS != acquire_result && VK_SUBOPTIMAL_KHR != acquire_result &&
        VK_ERROR_OUT_OF_DATE_KHR != acquire_result) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                   "Failed to acquire next image: %d", acquire_result);
      should_quit = true;
      continue;
    } else if (VK_ERROR_OUT_OF_DATE_KHR == acquire_result ||
               VK_SUBOPTIMAL_KHR == acquire_result) {
      framebuffer_resized = true;
      continue;
    }
#if _DEBUG
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Acquired image %u", image_index);
#endif

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkSemaphore wait_semaphores[] = {image_available_semaphore};
    VkPipelineStageFlags wait_stages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffers[image_index];
    VkSemaphore signal_semaphores[] = {render_finished_semaphore};
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;

    if (VkResult submit_result =
            vkQueueSubmit(graphics_queue, 1, &submit_info, in_flight_fence);
        VK_SUCCESS != submit_result) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                   "Failed to submit draw command buffer for image %u: %d",
                   image_index, submit_result);
      should_quit = true;
      continue;
    }
#if _DEBUG
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Submitted draw command buffer for image %u", image_index);
#endif

    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &swapchain;
    present_info.pImageIndices = &image_index;
    if (VkResult present_result =
            vkQueuePresentKHR(present_queue, &present_info);
        present_result != VK_SUCCESS && present_result != VK_SUBOPTIMAL_KHR &&
        present_result != VK_ERROR_OUT_OF_DATE_KHR) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                   "Failed to present image %u: %d", image_index,
                   present_result);
      should_quit = true;
      continue;
    } else if (present_result == VK_ERROR_OUT_OF_DATE_KHR ||
               present_result == VK_SUBOPTIMAL_KHR) {
      framebuffer_resized = true;
    }
#if _DEBUG
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Presented image %u",
                image_index);
#endif
    SDL_Delay(16);  // ~60 FPS cap
  }

  vkDeviceWaitIdle(device);
}