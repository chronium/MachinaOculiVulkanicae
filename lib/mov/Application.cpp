#include <mov/Application.hpp>

namespace mov {

void Application::init_internals() {
  surface_ = std::make_unique<surface::SDLSurface>(
      create_info_.app_name, create_info_.width, create_info_.height);

  auto extensions = surface_->get_extensions();
  extensions.push_back("VK_EXT_debug_utils");
  extensions.push_back("VK_EXT_debug_report");

  std::vector<const char *> layers;
  layers.push_back("VK_LAYER_KHRONOS_validation");

  vulkan_instance_ = std::make_unique<backend::VulkanInstance>(create_info_);

  create_vulkan_instance(vulkan_instance_.get(), extensions, layers);
  create_debug_messenger(vulkan_instance_.get());
}

int Application::run() {
  running_ = true;

  init();
  
  while (running_) {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
      switch (event.type) {
      case SDL_QUIT:
        running_ = false;
        break;
      default:
        break;
      }
    }
  }

  return 0;
}

} // namespace mov
