#pragma once

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <vector>

#include <gsl/util>

#include <SDL3/SDL.h>

namespace umutech::sdl3vk {

namespace fs = std::filesystem;

inline fs::path GetProductDirectory() noexcept {
  fs::path directory;
  if (auto base = SDL_GetBasePath()) {
    std::string_view path{base};
#if _WIN32
    size_t pos = path.rfind("\\bin\\");
#else
    size_t pos = path.rfind("/bin/");
#endif
    if (std::string_view::npos != pos) {
      directory = path.substr(0, pos + 1);
    } else {
      directory = fs::path{path}.parent_path();
    }
  } else {
    directory = fs::current_path().parent_path();
  }
  return directory;
}

inline std::vector<std::byte> ReadFile(
    const fs::path& filename) {
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

}  // namespace umutech::sdl3vk