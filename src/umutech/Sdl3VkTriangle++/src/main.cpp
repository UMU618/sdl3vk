// UMU: Using Vulkan-Hpp

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "vk_triangle.h"

SDL_AppResult SDL_AppInit(void** appstate, int /*argc*/, char* /*argv*/[]) {
  SDL_SetAppMetadata("VkTriangle++", "1.0", "com.umutech.sdl3vk");

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed: %s",
                 SDL_GetError());
    return SDL_APP_FAILURE;
  }

  auto window = SDL_CreateWindow("Sdl3VkTriangle++", 800, 618,
                                 SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
  if (!window) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Window creation failed: %s",
                 SDL_GetError());
    return SDL_APP_FAILURE;
  }

  auto vk_triangle = new (std::nothrow) VkTriangle{window};
  if (!vk_triangle) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create VkTriangle");
    return SDL_APP_FAILURE;
  }
  if (!vk_triangle->Initialize()) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to initialize VkTriangle");
    return SDL_APP_FAILURE;
  }
  *appstate = vk_triangle;

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
  switch (event->type) {
    case SDL_EVENT_QUIT:
      return SDL_APP_SUCCESS;
    case SDL_EVENT_KEY_DOWN:
      if (SDLK_ESCAPE == event->key.key) {
        return SDL_APP_SUCCESS;
      }
      break;
    case SDL_EVENT_WINDOW_MINIMIZED:
      if (auto vk_triangle = static_cast<VkTriangle*>(appstate)) {
        vk_triangle->SetPause(true);
      }
      break;
    case SDL_EVENT_WINDOW_RESTORED:
      if (auto vk_triangle = static_cast<VkTriangle*>(appstate)) {
        vk_triangle->SetPause(false);
      }
      break;
    case SDL_EVENT_WINDOW_RESIZED:
      if (auto vk_triangle = static_cast<VkTriangle*>(appstate)) {
        if (!vk_triangle->RecreateSwapchain()) {
          SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                       "Failed to recreate swapchain after window resize");
          return SDL_APP_FAILURE;
        }
      }
      break;
  }
  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
  if (auto vk_triangle = static_cast<VkTriangle*>(appstate)) {
    if (vk_triangle->Draw()) {
      return SDL_APP_CONTINUE;
    }

    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to draw frame");
  }
  return SDL_APP_FAILURE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult /*result*/) {
  if (auto vk_triangle = static_cast<VkTriangle*>(appstate)) {
    delete vk_triangle;
  }
}