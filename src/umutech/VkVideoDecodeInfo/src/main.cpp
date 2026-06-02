// UMU: Using Vulkan-Hpp

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_vulkan.h>

#include "vk_video_decode_info.h"

// 0: Load `Vulkan Loader` dynamically Using Vulkan-Hpp
#define LOAD_VULKAN_LOADER_DYNAMICALLY_USING_SDL 1

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

using namespace umutech::sdl3vk;

SDL_AppResult SDL_AppInit(void** /*appstate*/, int /*argc*/, char* /*argv*/[]) {
  SDL_SetAppMetadata("VkVideoDecodeInfo", "1.0", "com.umutech.sdl3vk");

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed: %s",
                 SDL_GetError());
    return SDL_APP_FAILURE;
  }

  // Load Vulkan Loader dynamically
#if LOAD_VULKAN_LOADER_DYNAMICALLY_USING_SDL
  if (!SDL_Vulkan_LoadLibrary(nullptr)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to load Vulkan library: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }
  auto vk_get_instance_proc_addr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
      SDL_Vulkan_GetVkGetInstanceProcAddr());
#else
  vk::detail::DynamicLoader dl;
  if (!dl.success()) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load Vulkan Loader");
    return SDL_APP_FAILURE;
  }
  auto vk_get_instance_proc_addr =
      dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
#endif
  if (!vk_get_instance_proc_addr) {
    return SDL_APP_FAILURE;
  }
  VULKAN_HPP_DEFAULT_DISPATCHER.init(vk_get_instance_proc_addr);

  VkVideoDecodeInfo vk_video_decode_info;
  if (!vk_video_decode_info.Initialize()) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to initialize VkVideoDecodeInfo");
    return SDL_APP_FAILURE;
  }
  vk_video_decode_info.PrintVideoDecodeCapabilities();

  return SDL_APP_SUCCESS;  // -> SDL_AppQuit
}

SDL_AppResult SDL_AppEvent(void* /*appstate*/, SDL_Event* /*event*/) {
  assert(!"SDL_AppEvent should not be called");
  return SDL_APP_SUCCESS;
}

SDL_AppResult SDL_AppIterate(void* /*appstate*/) {
  assert(!"SDL_AppIterate should not be called");
  return SDL_APP_SUCCESS;
}

void SDL_AppQuit(void* /*appstate*/, SDL_AppResult /*result*/) {
#if LOAD_VULKAN_LOADER_DYNAMICALLY_USING_SDL
  SDL_Vulkan_UnloadLibrary();
#endif
}
