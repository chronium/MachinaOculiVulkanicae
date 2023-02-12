#include <vulkan/vulkan.h>
#define XR_USE_GRAPHICS_API_VULKAN
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <csignal>
#include <fstream>
#include <iostream>
#include <map>
#include <set>

#include <spdlog/spdlog.h>

const std::map<XrDebugUtilsMessageTypeFlagsEXT, std::string> xrMessageTypeMap =
    {
        {XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, "General"},
        {XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT, "Validation"},
        {XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, "Performance"},
        {XR_DEBUG_UTILS_MESSAGE_TYPE_CONFORMANCE_BIT_EXT, "Conformance"},
};

const std::map<XrDebugUtilsMessageSeverityFlagsEXT, spdlog::level::level_enum>
    xrMessageSeverityMap = {
        {XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, spdlog::level::trace},
        {XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT, spdlog::level::info},
        {XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, spdlog::level::warn},
        {XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, spdlog::level::err},
};

const std::map<VkDebugUtilsMessageTypeFlagsEXT, std::string> vkMessageTypeMap =
    {
        {VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, "General"},
        {VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT, "Validation"},
        {VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, "Performance"},
};

const std::map<VkDebugUtilsMessageSeverityFlagBitsEXT,
               spdlog::level::level_enum>
    vkMessageSeverityMap = {
        {VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, spdlog::level::trace},
        {VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT, spdlog::level::info},
        {VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, spdlog::level::warn},
        {VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, spdlog::level::err},
};

static const auto applicationName = "OpenXR Test";
static const unsigned int majorVersion = 0;
static const unsigned int minorVersion = 1;
static const unsigned int patchVersion = 0;
static const char *const layerNames[] = {"XR_APILAYER_LUNARG_core_validation"};
static const char *const extensionNames[] = {
    "XR_KHR_vulkan_enable",
    "XR_KHR_vulkan_enable2",
    "XR_EXT_debug_utils",
};
static const char *const vulkanLayerNames[] = {"VK_LAYER_KHRONOS_validation"};
static const char *const vulkanExtensionNames[] = {"VK_EXT_debug_utils"};

static const size_t bufferSize = sizeof(float) * 4 * 4 * 3;

static const size_t eyeCount = 2;

static const float nearDistance = 0.01;
static const float farDistance = 1'000;

static bool quit = false;

static const float grabDistance = 10;

static int objectGrabbed = 0;
static XrVector3f objectPos = {0, 0, 0};

void onInterrupt(int) { quit = true; }

struct Swapchain {
  Swapchain(XrSwapchain swapchain, VkFormat format, uint32_t width,
            uint32_t height)
      : swapchain(swapchain), format(format), width(width), height(height) {}

  ~Swapchain() { xrDestroySwapchain(swapchain); }

  XrSwapchain swapchain;
  VkFormat format;
  uint32_t width;
  uint32_t height;
};

struct SwapchainImage {
  SwapchainImage(VkPhysicalDevice physicalDevice, VkDevice device,
                 VkRenderPass renderPass, VkCommandPool commandPool,
                 VkDescriptorPool descriptorPool,
                 VkDescriptorSetLayout descriptorSetLayout,
                 const Swapchain *swapchain, XrSwapchainImageVulkanKHR image)
      : image(image), device(device), commandPool(commandPool),
        descriptorPool(descriptorPool) {
    VkImageViewCreateInfo imageViewCreateInfo{};
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.image = image.image;
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.format = swapchain->format;
    imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    imageViewCreateInfo.subresourceRange.levelCount = 1;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount = 1;

    if (const auto result = vkCreateImageView(device, &imageViewCreateInfo,
                                              nullptr, &imageView);
        result != VK_SUCCESS) {
      spdlog::error("Failed to create Vulkan image view: {}", result);
    }

    VkFramebufferCreateInfo framebufferCreateInfo{};
    framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferCreateInfo.renderPass = renderPass;
    framebufferCreateInfo.attachmentCount = 1;
    framebufferCreateInfo.pAttachments = &imageView;
    framebufferCreateInfo.width = swapchain->width;
    framebufferCreateInfo.height = swapchain->height;
    framebufferCreateInfo.layers = 1;

    if (const auto result = vkCreateFramebuffer(device, &framebufferCreateInfo,
                                                nullptr, &framebuffer);
        result != VK_SUCCESS) {
      spdlog::error("Failed to create Vulkan framebuffer: {}", result);
    }

    VkBufferCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfo.size = bufferSize;
    createInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (const auto result =
            vkCreateBuffer(device, &createInfo, nullptr, &buffer);
        result != VK_SUCCESS) {
      spdlog::error("Failed to create Vulkan buffer: {}", result);
    }

    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(device, buffer, &requirements);

    VkMemoryPropertyFlags flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    VkPhysicalDeviceMemoryProperties properties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &properties);

    uint32_t memoryTypeIndex = 0;

    for (uint32_t i = 0; i < properties.memoryTypeCount; i++) {
      if (!(requirements.memoryTypeBits & (1 << i))) {
        continue;
      }

      if ((properties.memoryTypes[i].propertyFlags & flags) != flags) {
        continue;
      }

      memoryTypeIndex = i;
      break;
    }

    VkMemoryAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = requirements.size;
    allocateInfo.memoryTypeIndex = memoryTypeIndex;

    if (const auto result =
            vkAllocateMemory(device, &allocateInfo, nullptr, &memory);
        result != VK_SUCCESS) {
      spdlog::error("Failed to allocate Vulkan memory: {}", result);
    }

    vkBindBufferMemory(device, buffer, memory, 0);

    VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
    commandBufferAllocateInfo.sType =
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = commandPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = 1;

    if (const auto result = vkAllocateCommandBuffers(
            device, &commandBufferAllocateInfo, &commandBuffer);
        result != VK_SUCCESS) {
      spdlog::error("Failed to allocate Vulkan command buffers: {}", result);
    }

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
    descriptorSetAllocateInfo.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.descriptorPool = descriptorPool;
    descriptorSetAllocateInfo.descriptorSetCount = 1;
    descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayout;

    if (const auto result = vkAllocateDescriptorSets(
            device, &descriptorSetAllocateInfo, &descriptorSet);
        result != VK_SUCCESS) {
      spdlog::error("Failed to allocate Vulkan descriptor set: {}", result);
    }

    VkDescriptorBufferInfo descriptorBufferInfo{};
    descriptorBufferInfo.buffer = buffer;
    descriptorBufferInfo.offset = 0;
    descriptorBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = descriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrite.pBufferInfo = &descriptorBufferInfo;

    vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
  }

  ~SwapchainImage() {
    vkFreeDescriptorSets(device, descriptorPool, 1, &descriptorSet);
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    vkDestroyBuffer(device, buffer, nullptr);
    vkFreeMemory(device, memory, nullptr);
    vkDestroyFramebuffer(device, framebuffer, nullptr);
    vkDestroyImageView(device, imageView, nullptr);
  }

  XrSwapchainImageVulkanKHR image;
  VkImageView imageView;
  VkFramebuffer framebuffer;
  VkDeviceMemory memory;
  VkBuffer buffer;
  VkCommandBuffer commandBuffer;
  VkDescriptorSet descriptorSet;

private:
  VkDevice device;
  VkCommandPool commandPool;
  VkDescriptorPool descriptorPool;
};

XrInstance createInstance() {
  XrInstance instance;

  XrInstanceCreateInfo instanceCreateInfo{};
  instanceCreateInfo.type = XR_TYPE_INSTANCE_CREATE_INFO;
  instanceCreateInfo.createFlags = 0;
  strncpy_s(instanceCreateInfo.applicationInfo.applicationName, applicationName,
            strlen(applicationName));
  instanceCreateInfo.applicationInfo.applicationVersion =
      XR_MAKE_VERSION(majorVersion, minorVersion, patchVersion);
  strncpy_s(instanceCreateInfo.applicationInfo.engineName, applicationName,
            strlen(applicationName));
  instanceCreateInfo.applicationInfo.engineVersion =
      XR_MAKE_VERSION(majorVersion, minorVersion, patchVersion);
  instanceCreateInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
  instanceCreateInfo.enabledApiLayerCount = 1;
  instanceCreateInfo.enabledApiLayerNames = layerNames;
  instanceCreateInfo.enabledExtensionCount =
      sizeof(extensionNames) / sizeof(const char *);
  instanceCreateInfo.enabledExtensionNames = extensionNames;

  if (const auto result = xrCreateInstance(&instanceCreateInfo, &instance);
      result != XR_SUCCESS) {
    spdlog::error("Failed to create OpenXR instance: {}", result);
    return XR_NULL_HANDLE;
  }

  return instance;
}

PFN_vkVoidFunction getVKFunction(VkInstance instance, const char *name) {
  auto func = vkGetInstanceProcAddr(instance, name);

  if (!func) {
    spdlog::error("Failed to load Vulkan extension function: '{}'", name);
    return nullptr;
  }

  return func;
}

PFN_xrVoidFunction getXRFunction(XrInstance instance, const char *name) {
  PFN_xrVoidFunction func;

  if (const auto result = xrGetInstanceProcAddr(instance, name, &func);
      result != XR_SUCCESS) {
    spdlog::error("Failed to load OpenXR extension function '{}': {}", name,
                  result);
    return nullptr;
  }

  return func;
}

XrBool32 handleXRError(XrDebugUtilsMessageSeverityFlagsEXT severity,
                       XrDebugUtilsMessageTypeFlagsEXT type,
                       const XrDebugUtilsMessengerCallbackDataEXT *callbackData,
                       void *userData) {
  const auto messageType =
      xrMessageTypeMap.contains(type) ? xrMessageTypeMap.at(type) : "Unknown";
  const auto messageSeverity = xrMessageSeverityMap.contains(severity)
                                   ? xrMessageSeverityMap.at(severity)
                                   : spdlog::level::err;

  spdlog::log(messageSeverity, "OpenXR {}: {}", messageType,
              callbackData->message);

  return XR_FALSE;
}

XrDebugUtilsMessengerEXT createDebugMessenger(XrInstance instance) {
  XrDebugUtilsMessengerEXT debugMessenger;

  XrDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo{};
  debugMessengerCreateInfo.type = XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  debugMessengerCreateInfo.messageSeverities =
      XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
      XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
      XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  debugMessengerCreateInfo.messageTypes =
      XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
      XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
      XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
      XR_DEBUG_UTILS_MESSAGE_TYPE_CONFORMANCE_BIT_EXT;
  debugMessengerCreateInfo.userCallback = handleXRError;
  debugMessengerCreateInfo.userData = nullptr;

  auto xrCreateDebugUtilsMessengerEXT =
      reinterpret_cast<PFN_xrCreateDebugUtilsMessengerEXT>(
          getXRFunction(instance, "xrCreateDebugUtilsMessengerEXT"));

  if (const auto result = xrCreateDebugUtilsMessengerEXT(
          instance, &debugMessengerCreateInfo, &debugMessenger);
      result != XR_SUCCESS) {
    spdlog::error("Failed to create OpenXR debug messenger: {}", result);
    return XR_NULL_HANDLE;
  }

  return debugMessenger;
}

XrSystemId getSystem(XrInstance instance) {
  XrSystemId systemId;

  XrSystemGetInfo systemGetInfo{};
  systemGetInfo.type = XR_TYPE_SYSTEM_GET_INFO;
  systemGetInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

  if (const auto result = xrGetSystem(instance, &systemGetInfo, &systemId);
      result != XR_SUCCESS) {
    spdlog::error("Failed to get system: {}", result);
    return XR_NULL_SYSTEM_ID;
  }

  return systemId;
}

void destroyDebugMessenger(XrInstance instance,
                           XrDebugUtilsMessengerEXT debugMessenger) {
  auto xrDestroyDebugUtilsMessengerEXT =
      reinterpret_cast<PFN_xrDestroyDebugUtilsMessengerEXT>(
          getXRFunction(instance, "xrDestroyDebugUtilsMessengerEXT"));

  xrDestroyDebugUtilsMessengerEXT(debugMessenger);
}

std::tuple<XrGraphicsRequirementsVulkan2KHR, std::set<std::string>>
getVulkanInstanceRequirements(XrInstance instance, XrSystemId system) {
  auto xrGetVulkanGraphicsRequirements2KHR =
      reinterpret_cast<PFN_xrGetVulkanGraphicsRequirements2KHR>(
          getXRFunction(instance, "xrGetVulkanGraphicsRequirements2KHR"));
  auto xrGetVulkanInstanceExtensionsKHR =
      reinterpret_cast<PFN_xrGetVulkanInstanceExtensionsKHR>(
          getXRFunction(instance, "xrGetVulkanInstanceExtensionsKHR"));

  XrGraphicsRequirementsVulkan2KHR graphicsRequirements{};
  graphicsRequirements.type = XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR;

  auto result = xrGetVulkanGraphicsRequirements2KHR(instance, system,
                                                    &graphicsRequirements);
  if (result != XR_SUCCESS) {
    spdlog::error("Failed to get Vulkan graphics requirements: {}", result);
    return {graphicsRequirements, {}};
  }

  uint32_t instanceExtensionsSize;

  result = xrGetVulkanInstanceExtensionsKHR(instance, system, 0,
                                            &instanceExtensionsSize, nullptr);
  if (result != XR_SUCCESS) {
    spdlog::error("Failed to get Vulkan instance extensions: {}", result);
    return {graphicsRequirements, {}};
  }

  auto instanceExtensionsData = new char[instanceExtensionsSize];

  result = xrGetVulkanInstanceExtensionsKHR(
      instance, system, instanceExtensionsSize, &instanceExtensionsSize,
      instanceExtensionsData);
  if (result != XR_SUCCESS) {
    spdlog::error("Failed to get Vulkan instance extensions: {}", result);
    return {graphicsRequirements, {}};
  }

  std::set<std::string> instanceExtensions;

  uint32_t last = 0;
  for (uint32_t i = 0; i <= instanceExtensionsSize; i++) {
    if (i == instanceExtensionsSize || instanceExtensionsData[i] == ' ') {
      instanceExtensions.insert(
          std::string(instanceExtensionsData + last, i - last));
      last = i + 1;
    }
  }

  delete[] instanceExtensionsData;

  return {graphicsRequirements, instanceExtensions};
}

std::tuple<VkPhysicalDevice, std::set<std::string>>
getVulkanDeviceRequirements(XrInstance instance, XrSystemId system,
                            VkInstance vulkanInstance) {
  auto xrGetVulkanGraphicsDevice2KHR =
      reinterpret_cast<PFN_xrGetVulkanGraphicsDevice2KHR>(
          getXRFunction(instance, "xrGetVulkanGraphicsDevice2KHR"));
  auto xrGetVulkanDeviceExtensionsKHR =
      reinterpret_cast<PFN_xrGetVulkanDeviceExtensionsKHR>(
          getXRFunction(instance, "xrGetVulkanDeviceExtensionsKHR"));

  VkPhysicalDevice physicalDevice;

  XrVulkanGraphicsDeviceGetInfoKHR deviceGetInfo{
      XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR};
  deviceGetInfo.systemId = system;
  deviceGetInfo.vulkanInstance = vulkanInstance;

  auto result =
      xrGetVulkanGraphicsDevice2KHR(instance, &deviceGetInfo, &physicalDevice);

  if (result != XR_SUCCESS) {
    spdlog::error("Failed to get Vulkan graphics device: {}", result);
    return {VK_NULL_HANDLE, {}};
  }

  uint32_t deviceExtensionsSize;

  result = xrGetVulkanDeviceExtensionsKHR(instance, system, 0,
                                          &deviceExtensionsSize, nullptr);
  if (result != XR_SUCCESS) {
    spdlog::error("Failed to get Vulkan device extensions: {}", result);
    return {VK_NULL_HANDLE, {}};
  }

  auto deviceExtensionsData = new char[deviceExtensionsSize];

  result = xrGetVulkanDeviceExtensionsKHR(
      instance, system, deviceExtensionsSize, &deviceExtensionsSize,
      deviceExtensionsData);
  if (result != XR_SUCCESS) {
    spdlog::error("Failed to get Vulkan device extensions: {}", result);
    return {VK_NULL_HANDLE, {}};
  }

  std::set<std::string> deviceExtensions;

  uint32_t last = 0;
  for (uint32_t i = 0; i <= deviceExtensionsSize; i++) {
    if (i == deviceExtensionsSize || deviceExtensionsData[i] == ' ') {
      deviceExtensions.insert(
          std::string(deviceExtensionsData + last, i - last));
      last = i + 1;
    }
  }

  delete[] deviceExtensionsData;

  return {physicalDevice, deviceExtensions};
}

VkInstance
createVulkanInstance(XrGraphicsRequirementsVulkanKHR graphicsRequirements,
                     std::set<std::string> instanceExtensions) {
  VkInstance instance;

  size_t extensionCount = 1 + instanceExtensions.size();
  auto extensionNames = new const char *[extensionCount];

  size_t i = 0;
  extensionNames[i] = vulkanExtensionNames[0];
  i++;

  for (auto &instanceExtension : instanceExtensions) {
    extensionNames[i] = instanceExtension.c_str();
    i++;
  }

  VkApplicationInfo applicationInfo{};
  applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  applicationInfo.pApplicationName = applicationName;
  applicationInfo.applicationVersion =
      VK_MAKE_VERSION(majorVersion, minorVersion, patchVersion);
  applicationInfo.pEngineName = applicationName;
  applicationInfo.engineVersion =
      VK_MAKE_VERSION(majorVersion, minorVersion, patchVersion);
  applicationInfo.apiVersion = graphicsRequirements.minApiVersionSupported;

  VkInstanceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.pApplicationInfo = &applicationInfo;
  createInfo.enabledExtensionCount = extensionCount;
  createInfo.ppEnabledExtensionNames = extensionNames;
  createInfo.enabledLayerCount = 1;
  createInfo.ppEnabledLayerNames = vulkanLayerNames;

  auto result = vkCreateInstance(&createInfo, nullptr, &instance);

  delete[] extensionNames;

  if (result != VK_SUCCESS) {
    spdlog::error("Failed to create Vulkan instance: {}", result);
    return VK_NULL_HANDLE;
  }

  return instance;
}

XrBool32 handleVKError(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                       VkDebugUtilsMessageTypeFlagsEXT type,
                       const VkDebugUtilsMessengerCallbackDataEXT *callbackData,
                       void *userData) {
  const auto messageType =
      vkMessageTypeMap.contains(type) ? vkMessageTypeMap.at(type) : "Unknown";
  const auto messageSeverity = vkMessageSeverityMap.contains(severity)
                                   ? vkMessageSeverityMap.at(severity)
                                   : spdlog::level::err;

  spdlog::log(messageSeverity, "OpenXR {}: {}", messageType,
              callbackData->pMessage);

  return XR_FALSE;
}

VkDebugUtilsMessengerEXT createVulkanDebugMessenger(VkInstance instance) {
  VkDebugUtilsMessengerEXT debugMessenger;

  VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo{};
  debugMessengerCreateInfo.sType =
      VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  debugMessengerCreateInfo.messageSeverity =
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  debugMessengerCreateInfo.messageType =
      VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  debugMessengerCreateInfo.pfnUserCallback = handleVKError;

  auto vkCreateDebugUtilsMessengerEXT =
      reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
          getVKFunction(instance, "vkCreateDebugUtilsMessengerEXT"));

  if (const auto result = vkCreateDebugUtilsMessengerEXT(
          instance, &debugMessengerCreateInfo, nullptr, &debugMessenger);
      result != VK_SUCCESS) {
    std::cerr << "Failed to create Vulkan debug messenger: " << result
              << std::endl;
    return VK_NULL_HANDLE;
  }

  return debugMessenger;
}

void destroyVulkanDebugMessenger(VkInstance instance,
                                 VkDebugUtilsMessengerEXT debugMessenger) {
  auto vkDestroyDebugUtilsMessengerEXT =
      reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
          getVKFunction(instance, "vkDestroyDebugUtilsMessengerEXT"));

  vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
}

void destroyVulkanInstance(VkInstance instance) {
  vkDestroyInstance(instance, nullptr);
}

int32_t getDeviceQueueFamily(VkPhysicalDevice physicalDevice) {
  int32_t graphicsQueueFamilyIndex = -1;

  uint32_t queueFamilyCount;
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount,
                                           nullptr);

  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount,
                                           queueFamilies.data());

  for (int32_t i = 0; i < queueFamilyCount; i++) {
    if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      graphicsQueueFamilyIndex = i;
      break;
    }
  }

  if (graphicsQueueFamilyIndex == -1) {
    spdlog::error("No graphics queue found.");
    return graphicsQueueFamilyIndex;
  }

  return graphicsQueueFamilyIndex;
}

std::tuple<VkDevice, VkQueue>
createDevice(VkPhysicalDevice physicalDevice, int32_t graphicsQueueFamilyIndex,
             std::set<std::string> deviceExtensions) {
  VkDevice device;

  size_t extensionCount = deviceExtensions.size();
  auto extensions = new const char *[extensionCount];

  size_t i = 0;
  for (const auto &deviceExtension : deviceExtensions) {
    extensions[i] = deviceExtension.c_str();
    i++;
  }

  float priority = 1;

  VkDeviceQueueCreateInfo queueCreateInfo{};
  queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queueCreateInfo.queueFamilyIndex = graphicsQueueFamilyIndex;
  queueCreateInfo.queueCount = 1;
  queueCreateInfo.pQueuePriorities = &priority;

  VkDeviceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  createInfo.queueCreateInfoCount = 1;
  createInfo.pQueueCreateInfos = &queueCreateInfo;
  createInfo.enabledExtensionCount = extensionCount;
  createInfo.ppEnabledExtensionNames = extensions;

  auto result = vkCreateDevice(physicalDevice, &createInfo, nullptr, &device);

  delete[] extensions;

  if (result != VK_SUCCESS) {
    spdlog::error("Failed to create vulkan device: {}", result);
    return {VK_NULL_HANDLE, VK_NULL_HANDLE};
  }

  VkQueue queue;
  vkGetDeviceQueue(device, graphicsQueueFamilyIndex, 0, &queue);

  return {device, queue};
}

void destroyDevice(VkDevice device) { vkDestroyDevice(device, nullptr); }

VkRenderPass createRenderPass(VkDevice device) {
  VkRenderPass renderPass;

  VkAttachmentDescription attachment{};
  attachment.format = VK_FORMAT_R8G8B8A8_SRGB;
  attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentReference attachmentRef{};
  attachmentRef.attachment = 0;
  attachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &attachmentRef;

  VkRenderPassCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  createInfo.flags = 0;
  createInfo.attachmentCount = 1;
  createInfo.pAttachments = &attachment;
  createInfo.subpassCount = 1;
  createInfo.pSubpasses = &subpass;

  if (const auto result =
          vkCreateRenderPass(device, &createInfo, nullptr, &renderPass);
      result != VK_SUCCESS) {
    spdlog::error("Failed to create Vulkan render pass: {}", result);
    return VK_NULL_HANDLE;
  }

  return renderPass;
}

void destroyRenderPass(VkDevice device, VkRenderPass renderPass) {
  vkDestroyRenderPass(device, renderPass, nullptr);
}

VkCommandPool createCommandPool(VkDevice device,
                                int32_t graphicsQueueFamilyIndex) {
  VkCommandPool commandPool;

  VkCommandPoolCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  createInfo.queueFamilyIndex = graphicsQueueFamilyIndex;
  createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

  if (const auto result =
          vkCreateCommandPool(device, &createInfo, nullptr, &commandPool);
      result != VK_SUCCESS) {
    spdlog::error("Failed to create Vulkan command pool: {}", result);
    return VK_NULL_HANDLE;
  }

  return commandPool;
}

void destroyCommandPool(VkDevice device, VkCommandPool commandPool) {
  vkDestroyCommandPool(device, commandPool, nullptr);
}

VkDescriptorPool createDescriptorPool(VkDevice device) {
  VkDescriptorPool descriptorPool;

  VkDescriptorPoolSize poolSize{};
  poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  poolSize.descriptorCount = 32;

  VkDescriptorPoolCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  createInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  createInfo.maxSets = 32;
  createInfo.poolSizeCount = 1;
  createInfo.pPoolSizes = &poolSize;

  if (const auto result =
          vkCreateDescriptorPool(device, &createInfo, nullptr, &descriptorPool);
      result != VK_SUCCESS) {
    spdlog::error("Failed to create Vulkan descriptor pool: {}", result);
    return VK_NULL_HANDLE;
  }

  return descriptorPool;
}

void destroyDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool) {
  vkDestroyDescriptorPool(device, descriptorPool, nullptr);
}

VkDescriptorSetLayout createDescriptorSetLayout(VkDevice device) {
  VkDescriptorSetLayout descriptorSetLayout;

  VkDescriptorSetLayoutBinding binding{};
  binding.binding = 0;
  binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  binding.descriptorCount = 1;
  binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  VkDescriptorSetLayoutCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  createInfo.bindingCount = 1;
  createInfo.pBindings = &binding;

  if (const auto result = vkCreateDescriptorSetLayout(
          device, &createInfo, nullptr, &descriptorSetLayout)) {
    spdlog::error("Failed to create Vulkan descriptor set layout: {}", result);
    return VK_NULL_HANDLE;
  }

  return descriptorSetLayout;
}

void destroyDescriptorSetLayout(VkDevice device,
                                VkDescriptorSetLayout descriptorSetLayout) {
  vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
}

VkShaderModule createShader(VkDevice device, std::string path) {
  VkShaderModule shader;

  std::ifstream file(path, std::ios::binary);
  std::string source = std::string(std::istreambuf_iterator<char>(file),
                                   std::istreambuf_iterator<char>());

  VkShaderModuleCreateInfo shaderCreateInfo{};
  shaderCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  shaderCreateInfo.codeSize = source.size();
  shaderCreateInfo.pCode = reinterpret_cast<const uint32_t *>(source.data());

  VkResult result =
      vkCreateShaderModule(device, &shaderCreateInfo, nullptr, &shader);

  if (result != VK_SUCCESS) {
    std::cerr << "Failed to create Vulkan shader: " << result << std::endl;
  }

  return shader;
}

void destroyShader(VkDevice device, VkShaderModule shader) {
  vkDestroyShaderModule(device, shader, nullptr);
}

std::tuple<VkPipelineLayout, VkPipeline>
createPipeline(VkDevice device, VkRenderPass renderPass,
               VkDescriptorSetLayout descriptorSetLayout,
               VkShaderModule vertexShader, VkShaderModule fragmentShader) {
  VkPipelineLayout pipelineLayout;
  VkPipeline pipeline;

  VkPipelineLayoutCreateInfo layoutCreateInfo{};
  layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  layoutCreateInfo.setLayoutCount = 1;
  layoutCreateInfo.pSetLayouts = &descriptorSetLayout;

  if (const auto result = vkCreatePipelineLayout(device, &layoutCreateInfo,
                                                 nullptr, &pipelineLayout);
      result != VK_SUCCESS) {
    spdlog::error("Failed to create Vulkan pipeline layout: {}", result);
    return {VK_NULL_HANDLE, VK_NULL_HANDLE};
  }

  VkPipelineVertexInputStateCreateInfo vertexInputStage{};
  vertexInputStage.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputStage.vertexBindingDescriptionCount = 0;
  vertexInputStage.pVertexBindingDescriptions = nullptr;
  vertexInputStage.vertexAttributeDescriptionCount = 0;
  vertexInputStage.pVertexAttributeDescriptions = nullptr;

  VkPipelineInputAssemblyStateCreateInfo inputAssemblyStage{};
  inputAssemblyStage.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssemblyStage.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  inputAssemblyStage.primitiveRestartEnable = false;

  VkPipelineShaderStageCreateInfo vertexShaderStage{};
  vertexShaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertexShaderStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertexShaderStage.module = vertexShader;
  vertexShaderStage.pName = "main";

  const VkViewport viewport = {0, 0, 1024, 1024, 0, 1};
  constexpr VkRect2D scissor = {{0, 0}, {1024, 1024}};

  VkPipelineViewportStateCreateInfo viewportStage{};
  viewportStage.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportStage.viewportCount = 1;
  viewportStage.pViewports = &viewport;
  viewportStage.scissorCount = 1;
  viewportStage.pScissors = &scissor;

  VkPipelineRasterizationStateCreateInfo rasterizationStage{};
  rasterizationStage.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizationStage.depthClampEnable = false;
  rasterizationStage.rasterizerDiscardEnable = false;
  rasterizationStage.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizationStage.lineWidth = 1;
  rasterizationStage.cullMode = VK_CULL_MODE_NONE;
  rasterizationStage.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizationStage.depthBiasEnable = false;
  rasterizationStage.depthBiasConstantFactor = 0;
  rasterizationStage.depthBiasClamp = 0;
  rasterizationStage.depthBiasSlopeFactor = 0;

  VkPipelineMultisampleStateCreateInfo multisampleStage{};
  multisampleStage.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampleStage.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  multisampleStage.sampleShadingEnable = false;
  multisampleStage.minSampleShading = 0.25;

  VkPipelineDepthStencilStateCreateInfo depthStencilStage{};
  depthStencilStage.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencilStage.depthTestEnable = true;
  depthStencilStage.depthWriteEnable = true;
  depthStencilStage.depthCompareOp = VK_COMPARE_OP_LESS;
  depthStencilStage.depthBoundsTestEnable = false;
  depthStencilStage.minDepthBounds = 0;
  depthStencilStage.maxDepthBounds = 1;
  depthStencilStage.stencilTestEnable = false;

  VkPipelineShaderStageCreateInfo fragmentShaderStage{};
  fragmentShaderStage.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragmentShaderStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragmentShaderStage.module = fragmentShader;
  fragmentShaderStage.pName = "main";

  VkPipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  colorBlendAttachment.blendEnable = true;
  colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  colorBlendAttachment.dstColorBlendFactor =
      VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
  colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

  VkPipelineColorBlendStateCreateInfo colorBlendStage{};
  colorBlendStage.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlendStage.logicOpEnable = false;
  colorBlendStage.logicOp = VK_LOGIC_OP_COPY;
  colorBlendStage.attachmentCount = 1;
  colorBlendStage.pAttachments = &colorBlendAttachment;
  colorBlendStage.blendConstants[0] = 0;
  colorBlendStage.blendConstants[1] = 0;
  colorBlendStage.blendConstants[2] = 0;
  colorBlendStage.blendConstants[3] = 0;

  VkDynamicState dynamicStates[] = {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR,
  };

  VkPipelineDynamicStateCreateInfo dynamicState{};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.dynamicStateCount = 2;
  dynamicState.pDynamicStates = dynamicStates;

  VkPipelineShaderStageCreateInfo shaderStages[] = {vertexShaderStage,
                                                    fragmentShaderStage};

  VkGraphicsPipelineCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  createInfo.stageCount = 2;
  createInfo.pStages = shaderStages;
  createInfo.pVertexInputState = &vertexInputStage;
  createInfo.pInputAssemblyState = &inputAssemblyStage;
  createInfo.pTessellationState = nullptr;
  createInfo.pViewportState = &viewportStage;
  createInfo.pRasterizationState = &rasterizationStage;
  createInfo.pMultisampleState = &multisampleStage;
  createInfo.pDepthStencilState = &depthStencilStage;
  createInfo.pColorBlendState = &colorBlendStage;
  createInfo.pDynamicState = &dynamicState;
  createInfo.layout = pipelineLayout;
  createInfo.renderPass = renderPass;
  createInfo.subpass = 0;
  createInfo.basePipelineHandle = VK_NULL_HANDLE;
  createInfo.basePipelineIndex = -1;

  if (const auto result = vkCreateGraphicsPipelines(
          device, nullptr, 1, &createInfo, nullptr, &pipeline);
      result != VK_SUCCESS) {
    spdlog::error("Failed to create Vulkan pipeline: {}", result);
    return {VK_NULL_HANDLE, VK_NULL_HANDLE};
  }

  return {pipelineLayout, pipeline};
}

void destroyPipeline(VkDevice device, VkPipelineLayout pipelineLayout,
                     VkPipeline pipeline) {
  vkDestroyPipeline(device, pipeline, nullptr);
  vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
}

XrSession createSession(XrInstance instance, XrSystemId systemID,
                        VkInstance vulkanInstance, VkPhysicalDevice physDevice,
                        VkDevice device, uint32_t queueFamilyIndex) {
  XrSession session;

  XrGraphicsBindingVulkanKHR graphicsBinding{};
  graphicsBinding.type = XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR;
  graphicsBinding.instance = vulkanInstance;
  graphicsBinding.physicalDevice = physDevice;
  graphicsBinding.device = device;
  graphicsBinding.queueFamilyIndex = queueFamilyIndex;
  graphicsBinding.queueIndex = 0;

  XrSessionCreateInfo sessionCreateInfo{};
  sessionCreateInfo.type = XR_TYPE_SESSION_CREATE_INFO;
  sessionCreateInfo.next = &graphicsBinding;
  sessionCreateInfo.createFlags = 0;
  sessionCreateInfo.systemId = systemID;

  if (const auto result =
          xrCreateSession(instance, &sessionCreateInfo, &session);
      result != XR_SUCCESS) {
    spdlog::error("Failed to create OpenXR session: {}", result);
    return XR_NULL_HANDLE;
  }

  return session;
}

void destroySession(XrSession session) { xrDestroySession(session); }

void destroyInstance(XrInstance instance) { xrDestroyInstance(instance); }

std::tuple<Swapchain *, Swapchain *>
createSwapchains(XrInstance instance, XrSystemId system, XrSession session) {
  uint32_t configViewsCount = eyeCount;
  std::vector<XrViewConfigurationView> configViews(
      configViewsCount, {.type = XR_TYPE_VIEW_CONFIGURATION_VIEW});

  if (const auto result = xrEnumerateViewConfigurationViews(
          instance, system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
          configViewsCount, &configViewsCount, configViews.data());
      result != XR_SUCCESS) {
    spdlog::error("Failed to enumerate view configuration views: {}", result);
    return {nullptr, nullptr};
  }

  uint32_t formatCount = 0;

  if (const auto result =
          xrEnumerateSwapchainFormats(session, 0, &formatCount, nullptr);
      result != XR_SUCCESS) {
    spdlog::error("Failed to enumerate swapchain formats: {}", result);
    return {nullptr, nullptr};
  }

  std::vector<int64_t> formats(formatCount);

  if (const auto result = xrEnumerateSwapchainFormats(
          session, formatCount, &formatCount, formats.data());
      result != XR_SUCCESS) {
    spdlog::error("Failed to enumerate swapchain formats: {}", result);
    return {nullptr, nullptr};
  }

  int64_t chosenFormat = formats.front();

  for (int64_t format : formats) {
    if (format == VK_FORMAT_R8G8B8A8_SRGB) {
      chosenFormat = format;
      break;
    }
  }

  XrSwapchain swapchains[eyeCount];

  for (uint32_t i = 0; i < eyeCount; i++) {
    XrSwapchainCreateInfo swapchainCreateInfo{};
    swapchainCreateInfo.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
    swapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainCreateInfo.format = chosenFormat;
    swapchainCreateInfo.sampleCount = VK_SAMPLE_COUNT_1_BIT;
    swapchainCreateInfo.width = configViews[i].recommendedImageRectWidth;
    swapchainCreateInfo.height = configViews[i].recommendedImageRectHeight;
    swapchainCreateInfo.faceCount = 1;
    swapchainCreateInfo.arraySize = 1;
    swapchainCreateInfo.mipCount = 1;

    if (const auto result =
            xrCreateSwapchain(session, &swapchainCreateInfo, &swapchains[i]);
        result != XR_SUCCESS) {
      spdlog::error("Failed to create swapchain: {}", result);
      return {nullptr, nullptr};
    }
  }

  return {new Swapchain(swapchains[0], static_cast<VkFormat>(chosenFormat),
                        configViews[0].recommendedImageRectWidth,
                        configViews[0].recommendedImageRectHeight),
          new Swapchain(swapchains[1], static_cast<VkFormat>(chosenFormat),
                        configViews[1].recommendedImageRectWidth,
                        configViews[1].recommendedImageRectHeight)};
}

std::vector<XrSwapchainImageVulkanKHR>
getSwapchainImages(XrSwapchain swapchain) {
  uint32_t imageCount;

  if (const auto result =
          xrEnumerateSwapchainImages(swapchain, 0, &imageCount, nullptr);
      result != XR_SUCCESS) {
    spdlog::error("Failed to enumerate swapchain images: {}", result);
    return {};
  }

  std::vector<XrSwapchainImageVulkanKHR> images(
      imageCount, {.type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR});

  if (const auto result = xrEnumerateSwapchainImages(
          swapchain, imageCount, &imageCount,
          reinterpret_cast<XrSwapchainImageBaseHeader *>(images.data()));
      result != XR_SUCCESS) {
    spdlog::error("Failed ot enumerate swapchain images: {}", result);
    return {};
  }

  return images;
}

XrSpace createSpace(XrSession session) {
  XrSpace space;

  XrReferenceSpaceCreateInfo spaceCreateInfo{};
  spaceCreateInfo.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
  spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
  spaceCreateInfo.poseInReferenceSpace = {{0, 0, 0, 1}, {0, 0, 0}};

  if (const auto result =
          xrCreateReferenceSpace(session, &spaceCreateInfo, &space);
      result != XR_SUCCESS) {
    spdlog::error("Failed to create space: {}", result);
    return XR_NULL_HANDLE;
  }

  return space;
}

void destroySpace(XrSpace space) { xrDestroySpace(space); }

bool renderEye(Swapchain *swapchain,
               const std::vector<SwapchainImage *> &images, XrView view,
               VkDevice device, VkQueue queue, VkRenderPass renderPass,
               VkPipelineLayout pipelineLayout, VkPipeline pipeline) {
  XrSwapchainImageAcquireInfo acquireImageInfo{};
  acquireImageInfo.type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO;

  uint32_t activeIndex;

  if (const auto result = xrAcquireSwapchainImage(
          swapchain->swapchain, &acquireImageInfo, &activeIndex);
      result != XR_SUCCESS) {
    spdlog::error("Failed to acquire swapchain image: {}", result);
    return false;
  }

  XrSwapchainImageWaitInfo waitImageInfo{};
  waitImageInfo.type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO;
  waitImageInfo.timeout = std::numeric_limits<int64_t>::max();

  if (const auto result =
          xrWaitSwapchainImage(swapchain->swapchain, &waitImageInfo);
      result != XR_SUCCESS) {
    spdlog::error("Failed to wait for swapchain image: {}", result);
    return false;
  }

  const SwapchainImage *image = images[activeIndex];

  float *data;

  if (const auto result = vkMapMemory(device, image->memory, 0, VK_WHOLE_SIZE,
                                      0, reinterpret_cast<void **>(&data));
      result != VK_SUCCESS) {
    spdlog::error("Failed to map Vulkan memory: {}", result);
  }

  float angleWidth = tan(view.fov.angleRight) - tan(view.fov.angleLeft);
  float angleHeight = tan(view.fov.angleDown) - tan(view.fov.angleUp);

  float projectionMatrix[4][4]{{0}};

  projectionMatrix[0][0] = 2.0f / angleWidth;
  projectionMatrix[2][0] =
      (tan(view.fov.angleRight) + tan(view.fov.angleLeft)) / angleWidth;
  projectionMatrix[1][1] = 2.0f / angleHeight;
  projectionMatrix[2][1] =
      (tan(view.fov.angleUp) + tan(view.fov.angleDown)) / angleHeight;
  projectionMatrix[2][2] = -farDistance / (farDistance - nearDistance);
  projectionMatrix[3][2] =
      -(farDistance * nearDistance) / (farDistance - nearDistance);
  projectionMatrix[2][3] = -1;

  auto viewMatrix = inverse(
      translate(glm::mat4(1.0f),
                glm::vec3(view.pose.position.x, view.pose.position.y,
                          view.pose.position.z)) *
      mat4_cast(glm::quat(view.pose.orientation.w, view.pose.orientation.x,
                          view.pose.orientation.y, view.pose.orientation.z)));

  float modelMatrix[4][4]{{1, 0, 0, 0},
                          {0, 1, 0, 0},
                          {0, 0, 1, 0},
                          {objectPos.x, objectPos.y, objectPos.z, 1}};

  memcpy(data, projectionMatrix, sizeof(float) * 4 * 4);
  memcpy(data + (4 * 4), value_ptr(viewMatrix), sizeof(float) * 4 * 4);
  memcpy(data + (4 * 4) * 2, modelMatrix, sizeof(float) * 4 * 4);

  vkUnmapMemory(device, image->memory);

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(image->commandBuffer, &beginInfo);

  VkClearValue clearValue{};
  clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};

  VkRenderPassBeginInfo beginRenderPassInfo{};
  beginRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  beginRenderPassInfo.renderPass = renderPass;
  beginRenderPassInfo.framebuffer = image->framebuffer;
  beginRenderPassInfo.renderArea = {{0, 0},
                                    {(swapchain->width), (swapchain->height)}};
  beginRenderPassInfo.clearValueCount = 1;
  beginRenderPassInfo.pClearValues = &clearValue;

  vkCmdBeginRenderPass(image->commandBuffer, &beginRenderPassInfo,
                       VK_SUBPASS_CONTENTS_INLINE);

  VkViewport viewport = {0,
                         0,
                         static_cast<float>(swapchain->width),
                         static_cast<float>(swapchain->height),
                         0,
                         1};

  vkCmdSetViewport(image->commandBuffer, 0, 1, &viewport);

  VkRect2D scissor = {{0, 0}, swapchain->width, swapchain->height};

  vkCmdSetScissor(image->commandBuffer, 0, 1, &scissor);

  vkCmdBindPipeline(image->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipeline);

  vkCmdBindDescriptorSets(image->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipelineLayout, 0, 1, &image->descriptorSet, 0,
                          nullptr);

  vkCmdDraw(image->commandBuffer, 3, 1, 0, 0);

  vkCmdEndRenderPass(image->commandBuffer);

  if (const auto result = vkEndCommandBuffer(image->commandBuffer);
      result != VK_SUCCESS) {
    spdlog::error("Failed to end Vulkan command buffer: {}", result);
    return false;
  }

  VkPipelineStageFlags stageMask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.pWaitDstStageMask = &stageMask;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &image->commandBuffer;

  if (const auto result = vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
      result != VK_SUCCESS) {
    spdlog::error("Failed to submit Vulkan command buffer: {}", result);
    return false;
  }

  XrSwapchainImageReleaseInfo releaseImageInfo{};
  releaseImageInfo.type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO;

  if (const auto result =
          xrReleaseSwapchainImage(swapchain->swapchain, &releaseImageInfo);
      result != XR_SUCCESS) {
    spdlog::error("Failed to release swapchain image: {}", result);
    return false;
  }

  return true;
}

bool render(XrSession session, Swapchain *swapchains[2],
            std::vector<SwapchainImage *> swapchainImages[2], XrSpace space,
            XrTime predictedDisplayType, VkDevice device, VkQueue queue,
            VkRenderPass renderPass, VkPipelineLayout pipelineLayout,
            VkPipeline pipeline) {
  XrFrameBeginInfo beginFrameInfo{};
  beginFrameInfo.type = XR_TYPE_FRAME_BEGIN_INFO;

  if (const auto result = xrBeginFrame(session, &beginFrameInfo);
      result != XR_SUCCESS) {
    spdlog::error("Failed to begin frame: {}", result);
    return false;
  }

  XrViewLocateInfo viewLocateInfo{};

  viewLocateInfo.type = XR_TYPE_VIEW_LOCATE_INFO;
  viewLocateInfo.viewConfigurationType =
      XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
  viewLocateInfo.displayTime = predictedDisplayType;
  viewLocateInfo.space = space;

  XrViewState viewState{};
  viewState.type = XR_TYPE_VIEW_STATE;

  uint32_t viewCount = eyeCount;
  std::vector<XrView> views(viewCount, {.type = XR_TYPE_VIEW});

  if (const auto result = xrLocateViews(session, &viewLocateInfo, &viewState,
                                        viewCount, &viewCount, views.data());
      result != XR_SUCCESS) {
    spdlog::error("Failed to locate views: {}", result);
    return false;
  }

  for (size_t i = 0; i < eyeCount; i++) {
    renderEye(swapchains[i], swapchainImages[i], views[i], device, queue,
              renderPass, pipelineLayout, pipeline);
  }

  XrCompositionLayerProjectionView projectedViews[2]{};

  for (size_t i = 0; i < eyeCount; i++) {
    projectedViews[i].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
    projectedViews[i].pose = views[i].pose;
    projectedViews[i].fov = views[i].fov;
    projectedViews[i].subImage = {
        swapchains[i]->swapchain,
        {{0, 0},
         {static_cast<int32_t>(swapchains[i]->width),
          static_cast<int32_t>(swapchains[i]->height)}},
        0};
  }

  XrCompositionLayerProjection layer{};
  layer.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
  layer.space = space;
  layer.viewCount = eyeCount;
  layer.views = projectedViews;

  auto pLayer = reinterpret_cast<const XrCompositionLayerBaseHeader *>(&layer);

  XrFrameEndInfo endFrameInfo{};
  endFrameInfo.type = XR_TYPE_FRAME_END_INFO;
  endFrameInfo.displayTime = predictedDisplayType;
  endFrameInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
  endFrameInfo.layerCount = 1;
  endFrameInfo.layers = &pLayer;

  if (const auto result = xrEndFrame(session, &endFrameInfo);
      result != XR_SUCCESS) {
    spdlog::error("Failed to end frame: {}", result);
    return false;
  }

  return true;
}

XrActionSet createActionSet(XrInstance instance) {
  XrActionSet actionSet;

  XrActionSetCreateInfo actionSetCreateInfo{};
  actionSetCreateInfo.type = XR_TYPE_ACTION_SET_CREATE_INFO;
  strcpy(actionSetCreateInfo.actionSetName, "default");
  strcpy(actionSetCreateInfo.localizedActionSetName, "Default");
  actionSetCreateInfo.priority = 0;

  if (const auto result =
          xrCreateActionSet(instance, &actionSetCreateInfo, &actionSet);
      result != XR_SUCCESS) {
    spdlog::error("Failed to create action set: {}", result);
    return nullptr;
  }

  return actionSet;
}

void destroyActionSet(XrActionSet actionSet) { xrDestroyActionSet(actionSet); }

XrAction createAction(XrActionSet actionSet, const char *name,
                      XrActionType type) {
  XrAction action;

  XrActionCreateInfo actionCreateInfo{};
  actionCreateInfo.type = XR_TYPE_ACTION_CREATE_INFO;
  strcpy(actionCreateInfo.actionName, name);
  strcpy(actionCreateInfo.localizedActionName, name);
  actionCreateInfo.actionType = type;

  if (const auto result = xrCreateAction(actionSet, &actionCreateInfo, &action);
      result != XR_SUCCESS) {
    spdlog::error("Failed to create action: {}", result);
    return nullptr;
  }

  return action;
}

void destroyAction(XrAction action) { xrDestroyAction(action); }

XrSpace createActionSpace(XrSession session, XrAction action) {
  XrSpace space;

  XrActionSpaceCreateInfo actionSpaceCreateInfo{};
  actionSpaceCreateInfo.type = XR_TYPE_ACTION_SPACE_CREATE_INFO;
  actionSpaceCreateInfo.action = action;
  actionSpaceCreateInfo.poseInActionSpace.position = {0, 0, 0};
  actionSpaceCreateInfo.poseInActionSpace.orientation = {0, 0, 0, 1};

  if (const auto result =
          xrCreateActionSpace(session, &actionSpaceCreateInfo, &space);
      result != XR_SUCCESS) {
    spdlog::error("Failed to create action space: {}", result);
    return XR_NULL_HANDLE;
  }

  return space;
}

void destroyActionSpace(XrSpace actionSpace) { xrDestroySpace(actionSpace); }

XrPath getPath(XrInstance instance, const char *path) {
  XrPath pathHandle;

  if (const auto result = xrStringToPath(instance, path, &pathHandle);
      result != XR_SUCCESS) {
    spdlog::error("Failed to get path: {}", result);
    return XR_NULL_PATH;
  }

  return pathHandle;
}

void suggestBindings(XrInstance instance, XrAction leftHandAction,
                     XrAction rightHandAction, XrAction leftGrabAction,
                     XrAction rightGrabAction) {
  auto leftHandPath = getPath(instance, "/user/hand/left/input/grip/pose");
  auto rightHandPath = getPath(instance, "/user/hand/right/input/grip/pose");
  auto leftHandButtonPath = getPath(instance, "/user/hand/left/input/x/click");
  auto rightHandButtonPath =
      getPath(instance, "/user/hand/right/input/a/click");
  auto interactionProfilePath =
      getPath(instance, "/interaction_profiles/oculus/touch_controller");

  XrActionSuggestedBinding suggestedBindings[] = {
      {leftHandAction, leftHandPath},
      {rightHandAction, rightHandPath},
      {leftGrabAction, leftHandButtonPath},
      {rightGrabAction, rightHandButtonPath},
  };

  XrInteractionProfileSuggestedBinding suggestedBinding{};
  suggestedBinding.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING;
  suggestedBinding.interactionProfile = interactionProfilePath;
  suggestedBinding.countSuggestedBindings =
      sizeof(suggestedBindings) / sizeof(XrActionSuggestedBinding);
  suggestedBinding.suggestedBindings = suggestedBindings;

  if (const auto result =
          xrSuggestInteractionProfileBindings(instance, &suggestedBinding);
      result != XR_SUCCESS) {
    spdlog::error("Failed to suggest interaction profile bindings: {}", result);
  }
}

void attachActionSet(XrSession session, XrActionSet actionSet) {
  XrSessionActionSetsAttachInfo actionSetsAttachInfo{};
  actionSetsAttachInfo.type = XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO;
  actionSetsAttachInfo.countActionSets = 1;
  actionSetsAttachInfo.actionSets = &actionSet;

  if (const auto result =
          xrAttachSessionActionSets(session, &actionSetsAttachInfo);
      result != XR_SUCCESS) {
    spdlog::error("Failed to attach action set: {}", result);
  }
}

bool getActionBoolean(XrSession session, XrAction action) {
  XrActionStateGetInfo getInfo{};
  getInfo.type = XR_TYPE_ACTION_STATE_GET_INFO;
  getInfo.action = action;

  XrActionStateBoolean state{};
  state.type = XR_TYPE_ACTION_STATE_BOOLEAN;

  if (const auto result = xrGetActionStateBoolean(session, &getInfo, &state);
      result != XR_SUCCESS) {
    spdlog::error("Failed to get boolean action state: {}", result);
    return false;
  }

  return state.currentState;
}

XrPosef getActionPose(XrSession session, XrAction action, XrSpace space,
                      XrSpace roomSpace, XrTime predictedDisplayTime) {
  XrPosef pose = {{0, 0, 0, 1}, {0, 0, 0}};

  XrActionStateGetInfo getInfo{};
  getInfo.type = XR_TYPE_ACTION_STATE_GET_INFO;
  getInfo.action = action;

  XrActionStatePose state{};
  state.type = XR_TYPE_ACTION_STATE_POSE;

  XrResult result = xrGetActionStatePose(session, &getInfo, &state);

  if (result != XR_SUCCESS) {
    spdlog::error("Failed to get pose action state: {}", result);
    return pose;
  }

  XrSpaceLocation location{};
  location.type = XR_TYPE_SPACE_LOCATION;

  result = xrLocateSpace(space, roomSpace, predictedDisplayTime, &location);

  if (result != XR_SUCCESS) {
    spdlog::error("Failed to locate space: {}", result);
    return pose;
  }

  if (!(location.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) ||
      !(location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT)) {
    spdlog::error("Received incomplete result when locating space");
    return pose;
  }

  return location.pose;
}

bool input(XrSession session, XrActionSet actionSet, XrSpace roomSpace,
           XrTime predictedDisplayTime, XrAction leftHandAction,
           XrAction rightHandAction, XrAction leftGrabAction,
           XrAction rightGrabAction, XrSpace leftHandSpace,
           XrSpace rightHandSpace) {
  XrActiveActionSet activeActionSet = {actionSet, XR_NULL_PATH};

  XrActionsSyncInfo syncInfo{};
  syncInfo.type = XR_TYPE_ACTIONS_SYNC_INFO;
  syncInfo.countActiveActionSets = 1;
  syncInfo.activeActionSets = &activeActionSet;

  const auto result = xrSyncActions(session, &syncInfo);

  if (result == XR_SESSION_NOT_FOCUSED) {
    return true;
  } else if (result != XR_SUCCESS) {
    spdlog::error("Failed to synchronize actions: {}", result);
    return false;
  }

  auto leftHand = getActionPose(session, leftHandAction, leftHandSpace,
                                roomSpace, predictedDisplayTime);
  auto rightHand = getActionPose(session, rightHandAction, rightHandSpace,
                                 roomSpace, predictedDisplayTime);

  bool leftGrab = getActionBoolean(session, leftGrabAction);
  bool rightGrab = getActionBoolean(session, rightGrabAction);

  if (leftGrab && !objectGrabbed &&
      sqrt(pow(objectPos.x - leftHand.position.x, 2) +
           pow(objectPos.y - leftHand.position.y, 2) +
           pow(objectPos.z - leftHand.position.z, 2)) < grabDistance) {
    objectGrabbed = 1;
  } else if (!leftGrab && objectGrabbed == 1) {
    objectGrabbed = 0;
  }

  if (rightGrab && !objectGrabbed &&
      sqrt(pow(objectPos.x - leftHand.position.x, 2) +
           pow(objectPos.y - leftHand.position.y, 2) +
           pow(objectPos.z - leftHand.position.z, 2)) < grabDistance) {
    objectGrabbed = 2;
  } else if (!rightGrab && objectGrabbed == 2) {
    objectGrabbed = 0;
  }

  switch (objectGrabbed) {
  case 0:
    break;
  case 1:
    objectPos = leftHand.position;
    break;
  case 2:
    objectPos = rightHand.position;
    break;
  }

  return true;
}

int main(int, char **) {
#if defined _DEBUG
  spdlog::set_level(spdlog::level::trace);
#endif

  auto instance = createInstance();
  auto debugMessenger = createDebugMessenger(instance);
  auto system = getSystem(instance);

  auto [graphicsRequirements, instanceExtensions] =
      getVulkanInstanceRequirements(instance, system);
  auto vulkanInstance =
      createVulkanInstance(graphicsRequirements, instanceExtensions);
  auto vulkanDebugMessenger = createVulkanDebugMessenger(vulkanInstance);

  auto [physicalDevice, deviceExtensions] =
      getVulkanDeviceRequirements(instance, system, vulkanInstance);
  auto graphicsQueueFamilyIndex = getDeviceQueueFamily(physicalDevice);
  auto [device, queue] =
      createDevice(physicalDevice, graphicsQueueFamilyIndex, deviceExtensions);

  auto renderPass = createRenderPass(device);
  auto commandPool = createCommandPool(device, graphicsQueueFamilyIndex);
  auto descriptorPool = createDescriptorPool(device);
  auto descriptorSetLayout = createDescriptorSetLayout(device);
  auto vertexShader = createShader(device, "data\\vertex.vert.spv");
  auto fragmentShader = createShader(device, "data\\fragment.frag.spv");
  auto [pipelineLayout, pipeline] = createPipeline(
      device, renderPass, descriptorSetLayout, vertexShader, fragmentShader);

  auto session = createSession(instance, system, vulkanInstance, physicalDevice,
                               device, graphicsQueueFamilyIndex);

  Swapchain *swapchains[eyeCount];
  std::tie(swapchains[0], swapchains[1]) =
      createSwapchains(instance, system, session);

  std::vector<XrSwapchainImageVulkanKHR> swapchainImages[eyeCount];

  for (size_t i = 0; i < eyeCount; i++) {
    swapchainImages[i] = getSwapchainImages(swapchains[i]->swapchain);
  }

  std::vector<SwapchainImage *> wrappedSwapchainImages[eyeCount];

  for (size_t i = 0; i < eyeCount; i++) {
    wrappedSwapchainImages[i] =
        std::vector<SwapchainImage *>(swapchainImages[i].size(), nullptr);

    for (size_t j = 0; j < wrappedSwapchainImages[i].size(); j++) {
      wrappedSwapchainImages[i][j] = new SwapchainImage(
          physicalDevice, device, renderPass, commandPool, descriptorPool,
          descriptorSetLayout, swapchains[i], swapchainImages[i][j]);
    }
  }

  auto space = createSpace(session);

  auto actionSet = createActionSet(instance);

  auto leftHandAction =
      createAction(actionSet, "left-hand", XR_ACTION_TYPE_POSE_INPUT);
  auto rightHandAction =
      createAction(actionSet, "right-hand", XR_ACTION_TYPE_POSE_INPUT);
  auto leftGrabAction =
      createAction(actionSet, "left-grab", XR_ACTION_TYPE_BOOLEAN_INPUT);
  auto rightGrabAction =
      createAction(actionSet, "right-grab", XR_ACTION_TYPE_BOOLEAN_INPUT);

  auto leftHandSpace = createActionSpace(session, leftHandAction);
  auto rightHandSpace = createActionSpace(session, rightHandAction);

  suggestBindings(instance, leftHandAction, rightHandAction, leftGrabAction,
                  rightGrabAction);
  attachActionSet(session, actionSet);

  signal(SIGINT, onInterrupt);

  bool running = false;
  while (!quit) {
    XrEventDataBuffer eventData{};
    eventData.type = XR_TYPE_EVENT_DATA_BUFFER;

    if (auto result = xrPollEvent(instance, &eventData);
        result == XR_EVENT_UNAVAILABLE) {
      if (running) {
        XrFrameWaitInfo frameWaitInfo{};
        frameWaitInfo.type = XR_TYPE_FRAME_WAIT_INFO;

        XrFrameState frameState{};
        frameState.type = XR_TYPE_FRAME_STATE;

        if (const auto result =
                xrWaitFrame(session, &frameWaitInfo, &frameState);
            result != XR_SUCCESS) {
          spdlog::error("Failed to wait for frame: {}", result);
          break;
        }

        if (!frameState.shouldRender) {
          continue;
        }

        quit =
            !input(session, actionSet, space, frameState.predictedDisplayTime,
                   leftHandAction, rightHandAction, leftGrabAction,
                   rightGrabAction, leftHandSpace, rightHandSpace);

        quit = !render(session, swapchains, wrappedSwapchainImages, space,
                       frameState.predictedDisplayTime, device, queue,
                       renderPass, pipelineLayout, pipeline);
      }
    } else if (result != XR_SUCCESS) {
      spdlog::error("Failed to poll events: {}", result);
      break;
    } else {
      switch (eventData.type) {
      default:
        spdlog::error("Unknown event type received: {}", eventData.type);
        break;
      case XR_TYPE_EVENT_DATA_EVENTS_LOST:
        spdlog::error("Event queue overflowed and events were lost.");
        break;
      case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
        spdlog::error("OpenXR instance is shutting down.");
        quit = true;
        break;
      case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
        spdlog::info("The interaction profile has changed.");
        break;
      case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
        spdlog::info("The reference space is changing.");
        break;
      case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
        switch (
            const auto event =
                reinterpret_cast<XrEventDataSessionStateChanged *>(&eventData);
            event->state) {
        case XR_SESSION_STATE_UNKNOWN:
        case XR_SESSION_STATE_MAX_ENUM:
          spdlog::error("Unknown session state entered: {}", event->state);
          break;
        case XR_SESSION_STATE_IDLE:
          running = false;
          break;
        case XR_SESSION_STATE_READY: {
          XrSessionBeginInfo sessionBeginInfo{};
          sessionBeginInfo.type = XR_TYPE_SESSION_BEGIN_INFO;
          sessionBeginInfo.primaryViewConfigurationType =
              XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;

          result = xrBeginSession(session, &sessionBeginInfo);

          if (result != XR_SUCCESS) {
            spdlog::error("Failed to being session: {}", result);
          }

          running = true;
          break;
        }
        case XR_SESSION_STATE_SYNCHRONIZED:
        case XR_SESSION_STATE_VISIBLE:
        case XR_SESSION_STATE_FOCUSED:
          running = true;
          break;
        case XR_SESSION_STATE_STOPPING:
          result = xrEndSession(session);

          if (result != XR_SUCCESS) {
            spdlog::error("Failed to end session: {}", result);
          }
          break;
        case XR_SESSION_STATE_LOSS_PENDING:
          spdlog::info("OpenXR session is shutting down.");
          quit = true;
          break;
        case XR_SESSION_STATE_EXITING:
          spdlog::info("OpenXr runtime requested shutdown.");
          quit = true;
          break;
        }
        break;
      }
      }
    }
  }

  if (auto result = vkDeviceWaitIdle(device); result != VK_SUCCESS) {
    spdlog::error("Failed to wait for device to idle: {}", result);
  }

  destroyActionSpace(leftHandSpace);
  destroyActionSpace(rightHandSpace);

  destroyAction(rightGrabAction);
  destroyAction(leftGrabAction);
  destroyAction(rightHandAction);
  destroyAction(leftHandAction);

  destroyActionSet(actionSet);

  destroySpace(space);

  for (const auto &wrappedSwapchainImage : wrappedSwapchainImages) {
    for (const auto &swapchain_image : wrappedSwapchainImage) {
      delete swapchain_image;
    }
  }

  for (const auto &swapchain : swapchains) {
    delete swapchain;
  }

  destroySession(session);

  destroyPipeline(device, pipelineLayout, pipeline);
  destroyShader(device, fragmentShader);
  destroyShader(device, vertexShader);
  destroyDescriptorSetLayout(device, descriptorSetLayout);
  destroyDescriptorPool(device, descriptorPool);
  destroyCommandPool(device, commandPool);
  destroyRenderPass(device, renderPass);

  destroyDevice(device);

  destroyVulkanDebugMessenger(vulkanInstance, vulkanDebugMessenger);
  destroyVulkanInstance(vulkanInstance);

  destroyDebugMessenger(instance, debugMessenger);
  destroyInstance(instance);

  return 0;
}
