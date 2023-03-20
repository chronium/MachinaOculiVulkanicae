#pragma once

#include <memory>

#include "backend/VulkanInstance.hpp"
#include "surface/SDLSurface.hpp"

namespace mov {

class Application {
public:
  explicit Application(const ApplicationCreateInfo create_info)
      : create_info_(create_info) {
    init_internals();
  }

  Application(Application &) = delete;
  Application(Application &&) = delete;

  void operator=(Application &) = delete;
  void operator=(Application &&) = delete;

  virtual ~Application() = default;

  virtual void init() = 0;

  virtual void render() = 0;

  int run();

private:
  void init_internals();

  ApplicationCreateInfo create_info_;

  bool running_ = false;

  std::unique_ptr<surface::SDLSurface> surface_;
  std::unique_ptr<backend::VulkanInstance> vulkan_instance_;
};

} // namespace mov
