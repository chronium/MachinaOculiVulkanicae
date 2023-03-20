#include <mov/surface/SDLSurface.hpp>

namespace mov::surface {

std::vector<const char *> SDLSurface::get_extensions() const {
  uint32_t count = 0;
  SDL_Vulkan_GetInstanceExtensions(window_, &count, nullptr);
  std::vector<const char *> extensions(count);
  SDL_Vulkan_GetInstanceExtensions(window_, &count, extensions.data());
  return extensions;
}

} // namespace mov::surface
