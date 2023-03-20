#pragma once

#include <map>
#include <spdlog/spdlog.h>

static const std::map<VkDebugUtilsMessageSeverityFlagBitsEXT,
                      spdlog::level::level_enum>
    vk_message_severity_map = {
        {VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, spdlog::level::trace},
        {VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT, spdlog::level::info},
        {VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, spdlog::level::warn},
        {VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, spdlog::level::err},
};

inline VkBool32
handle_vk_error(const VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                const VkDebugUtilsMessageTypeFlagsEXT type,
                const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
                // ReSharper disable once CppParameterMayBeConstPtrOrRef
                void *user_data) {
  (void)user_data;
  const auto message_severity = vk_message_severity_map.contains(severity)
                                    ? vk_message_severity_map.at(severity)
                                    : spdlog::level::err;

  if (strncmp(callback_data->pMessageIdName,
              "UNASSIGNED-CoreValidation-DrawState-InvalidImageLayout",
              56) != 0)
    spdlog::log(
        message_severity, "Vulkan {}: {}",
        vk::to_string(static_cast<vk::DebugUtilsMessageTypeFlagsEXT>(type)),
        callback_data->pMessage);

  return VK_FALSE;
}

// ReSharper disable CppInconsistentNaming
inline PFN_vkCreateDebugUtilsMessengerEXT pfnVkCreateDebugUtilsMessengerEXT;
inline PFN_vkDestroyDebugUtilsMessengerEXT pfnVkDestroyDebugUtilsMessengerEXT;

// ReSharper disable CppParameterMayBeConst
inline VKAPI_ATTR VkResult VKAPI_CALL vkCreateDebugUtilsMessengerEXT(
    VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
    const VkAllocationCallbacks *pAllocator,
    VkDebugUtilsMessengerEXT *pMessenger) {
  return pfnVkCreateDebugUtilsMessengerEXT(instance, pCreateInfo, pAllocator,
                                           pMessenger);
}

inline VKAPI_ATTR void VKAPI_CALL vkDestroyDebugUtilsMessengerEXT(
    VkInstance instance, VkDebugUtilsMessengerEXT messenger,
    VkAllocationCallbacks const *pAllocator) {
  return pfnVkDestroyDebugUtilsMessengerEXT(instance, messenger, pAllocator);
}
// ReSharper restore CppParameterMayBeConst
// ReSharper restore CppInconsistentNaming
