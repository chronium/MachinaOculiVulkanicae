#include <mov/backend/VulkanInstance.hpp>
#include <vulkan/vulkan.h>

#include "VulkanDebugger.hpp"

namespace mov::backend {

void VulkanInstance::create_vulkan_instance(
    std::vector<const char *> &extensions, std::vector<const char *> &layers) {
  vk::ApplicationInfo application_info;
  application_info.setEngineVersion(VK_MAKE_VERSION(0, 1, 0))
      .setPEngineName("MachinaOculiVulkanicae")
      .setApplicationVersion(VK_MAKE_VERSION(app_create_info_.app_major,
                                             app_create_info_.app_minor,
                                             app_create_info_.app_patch))
      .setPApplicationName(app_create_info_.app_name)
      .setApiVersion(VK_MAKE_API_VERSION(0, 1, 1, 0));

  vk::InstanceCreateInfo create_info;
  create_info.setPEnabledExtensionNames(extensions)
      .setPEnabledLayerNames(layers)
      .setPApplicationInfo(&application_info);

  vk_instance_ = vk::createInstanceUnique(create_info);
}

void VulkanInstance::create_debug_messenger() {
  vk::DebugUtilsMessageSeverityFlagsEXT severity_flags{
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose};

  vk::DebugUtilsMessageTypeFlagsEXT message_types{
      vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
      vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
      vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation};

  pfnVkCreateDebugUtilsMessengerEXT =
      reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
          vk_instance_.get().getProcAddr("vkCreateDebugUtilsMessengerEXT"));

  vk_debug_messenger_ = vk_instance_.get().createDebugUtilsMessengerEXTUnique(
      {{}, severity_flags, message_types, handle_vk_error});
}

} // namespace mov::backend
