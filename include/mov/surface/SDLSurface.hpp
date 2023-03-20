#pragma once

#include <SDL2/sdl.h>
#include <SDL2/sdl_vulkan.h>
#include <vector>

namespace mov::surface {

class SDLSurface {
public:
  SDLSurface(const char *title, const int32_t width, const int32_t height)
      : width_(width), height_(height), title_(title) {
    SDL_Init(SDL_INIT_VIDEO);
    window_ =
        SDL_CreateWindow(title_, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         width_, height_, SDL_WINDOW_VULKAN);
  }

  ~SDLSurface() {
    SDL_DestroyWindow(window_);
    SDL_Quit();
  }

  [[nodiscard]] std::vector<const char *> get_extensions() const;

private:
  int32_t width_;
  int32_t height_;

  const char *title_;

  SDL_Window *window_;
};

} // namespace mov::surface
