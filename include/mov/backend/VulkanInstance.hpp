#pragma once

#include <vector>

#include <vulkan/vulkan.hpp>

#include <mov/CommonStructs.hpp>

namespace mov::backend {

class VulkanInstance {
public:
  explicit VulkanInstance(const ApplicationCreateInfo app_create_info)
      : app_create_info_(app_create_info) {}

  ~VulkanInstance() = default;

  VulkanInstance(VulkanInstance &) = delete;
  VulkanInstance(VulkanInstance &&) = delete;

  void operator=(VulkanInstance &) = delete;
  void operator=(VulkanInstance &&) = delete;

private:
  void create_vulkan_instance(std::vector<const char *> &extensions,
                              std::vector<const char *> &layers);
  friend void create_vulkan_instance(VulkanInstance *instance,
                                     std::vector<const char *> &extensions,
                                     std::vector<const char *> &layers) {
    instance->create_vulkan_instance(extensions, layers);
  }

  void create_debug_messenger();
  friend void create_debug_messenger(VulkanInstance *instance) {
    instance->create_debug_messenger();
  }

  ApplicationCreateInfo app_create_info_;

  vk::UniqueHandle<vk::Instance, vk::DispatchLoaderStatic> vk_instance_;
  vk::UniqueHandle<vk::DebugUtilsMessengerEXT, vk::DispatchLoaderStatic>
      vk_debug_messenger_;

  friend class Application;
};

} // namespace mov::backend
