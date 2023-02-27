#include <vulkan/vulkan.h>
#include <vulkan/vulkan.hpp>
#define XR_USE_GRAPHICS_API_VULKAN
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#pragma warning(push, 0)
#include <openxr/openxr.hpp>
#pragma warning(pop)

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <csignal>
#include <fstream>
#include <iostream>
#include <map>
#include <set>

#include <spdlog/spdlog.h>

#include <mov/VkBuffer.hpp>

#include "mov/VkImage.hpp"

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
    "XR_EXT_debug_utils",
};
static const char *const vulkanLayerNames[] = {"VK_LAYER_KHRONOS_validation"};
static const char *const vulkanExtensionNames[] = {"VK_EXT_debug_utils"};

static const size_t bufferSize = sizeof(float) * 4 * 4 * 2;

static const size_t eyeCount = 2;

static const float nearDistance = 0.01f;
static const float farDistance = 1'000;

static bool quit = false;

static const float grabDistance = 10;

static int objectGrabbed = 0;
static XrVector3f objectPos = {0, 0, 0};

static XrVector3f right_hand_pos{0, 0, 0};
static XrQuaternionf right_hand_orientation{0, 0, 0, 1};

static mov::VkBuffer<mov::Vertex> vertex_buffer;
static mov::VkBuffer<uint16_t> index_buffer;

void onInterrupt(int) { quit = true; }

struct PushConstants {
  glm::mat4 model;
};

const std::vector<mov::Vertex> vertices = {
    {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
    {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}},
    {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}}};

const std::vector<uint16_t> indices = {0, 1, 2, 2, 3, 0};

struct Swapchain {
  Swapchain(xr::Swapchain swapchain, vk::Format format, uint32_t width,
            uint32_t height)
      : swapchain(swapchain), format(format), width(width), height(height) {}

  ~Swapchain() { swapchain.destroy(); }

  xr::Swapchain swapchain;
  vk::Format format;
  uint32_t width;
  uint32_t height;
};

auto find_supported_format(const vk::PhysicalDevice physical_device,
                           const std::vector<vk::Format> &candidates,
                           const vk::ImageTiling tiling,
                           const vk::FormatFeatureFlags features) {
  for (const auto &format : candidates) {
    auto props = physical_device.getFormatProperties(format);

    if (tiling == vk::ImageTiling::eLinear &&
        (props.linearTilingFeatures & features) == features)
      return format;
    if (tiling == vk::ImageTiling::eOptimal &&
        (props.optimalTilingFeatures & features) == features)
      return format;
  }

  throw std::runtime_error("failed to find supported format!");
}

auto find_depth_format(const vk::PhysicalDevice physical_device) -> vk::Format {
  return find_supported_format(
      physical_device,
      {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint,
       vk::Format::eD24UnormS8Uint},
      vk::ImageTiling::eOptimal,
      vk::FormatFeatureFlagBits::eDepthStencilAttachment);
}

struct SwapchainImage {
  SwapchainImage(vk::PhysicalDevice physical_device, vk::Device device,
                 vk::RenderPass render_pass, vk::CommandPool command_pool,
                 vk::DescriptorPool descriptor_pool,
                 vk::DescriptorSetLayout descriptor_set_layout,
                 const Swapchain *swapchain, xr::SwapchainImageVulkanKHR image)
      : image(image), device(device), commandPool(command_pool),
        descriptorPool(descriptor_pool) {
    vk::ImageViewCreateInfo image_view_create_info{};
    image_view_create_info.setImage(image.image)
        .setViewType(vk::ImageViewType::e2D)
        .setFormat(swapchain->format)
        .setSubresourceRange(vk::ImageSubresourceRange()
                                 .setAspectMask(vk::ImageAspectFlagBits::eColor)
                                 .setBaseMipLevel(0)
                                 .setLevelCount(1)
                                 .setBaseArrayLayer(0)
                                 .setLayerCount(1));

    imageView = device.createImageView(image_view_create_info);
    depthImage =
        mov::VkImage(device, physical_device, swapchain->width,
                     swapchain->height, find_depth_format(physical_device),
                     vk::ImageTiling::eOptimal, vk::ImageAspectFlagBits::eDepth,
                     vk::ImageUsageFlagBits::eDepthStencilAttachment,
                     vk::MemoryPropertyFlagBits::eDeviceLocal);

    vk::ImageView imageViews[2] = {imageView, depthImage.image_view};

    vk::FramebufferCreateInfo framebuffer_create_info{};
    framebuffer_create_info.setRenderPass(render_pass)
        .setAttachments(imageViews)
        .setWidth(swapchain->width)
        .setHeight(swapchain->height)
        .setLayers(1);

    framebuffer = device.createFramebuffer(framebuffer_create_info);

    vk::BufferCreateInfo create_info{};
    create_info.setSize(bufferSize)
        .setUsage(vk::BufferUsageFlagBits::eUniformBuffer)
        .setSharingMode(vk::SharingMode::eExclusive);

    buffer = device.createBuffer(create_info);

    vk::MemoryRequirements requirements =
        device.getBufferMemoryRequirements(buffer);

    vk::MemoryPropertyFlags flags = vk::MemoryPropertyFlagBits::eHostVisible |
                                    vk::MemoryPropertyFlagBits::eHostCoherent;

    vk::PhysicalDeviceMemoryProperties properties =
        physical_device.getMemoryProperties();

    uint32_t memory_type_index = 0;

    for (uint32_t i = 0; i < properties.memoryTypeCount; i++) {
      if (!(requirements.memoryTypeBits & (1 << i))) {
        continue;
      }

      if ((properties.memoryTypes[i].propertyFlags & flags) != flags) {
        continue;
      }

      memory_type_index = i;
      break;
    }

    VkMemoryAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = requirements.size;
    allocateInfo.memoryTypeIndex = memory_type_index;

    vk::MemoryAllocateInfo allocate_info{};
    allocate_info.setAllocationSize(requirements.size)
        .setMemoryTypeIndex(memory_type_index);

    memory = device.allocateMemory(allocate_info);

    device.bindBufferMemory(buffer, memory, 0);

    vk::CommandBufferAllocateInfo command_buffer_allocate_info{};
    command_buffer_allocate_info.setCommandPool(command_pool)
        .setLevel(vk::CommandBufferLevel::ePrimary)
        .setCommandBufferCount(1);

    commandBuffer =
        device.allocateCommandBuffers(command_buffer_allocate_info)[0];

    vk::DescriptorSetAllocateInfo descriptor_set_allocate_info{};
    descriptor_set_allocate_info.setDescriptorPool(descriptor_pool)
        .setSetLayouts(descriptor_set_layout);

    descriptorSet =
        device.allocateDescriptorSets(descriptor_set_allocate_info)[0];

    vk::DescriptorBufferInfo descriptor_buffer_info{};
    descriptor_buffer_info.setBuffer(buffer).setOffset(0).setRange(~0);

    vk::WriteDescriptorSet descriptor_write{};
    descriptor_write.setDstSet(descriptorSet)
        .setDstBinding(0)
        .setDstArrayElement(0)
        .setDescriptorCount(1)
        .setDescriptorType(vk::DescriptorType::eUniformBuffer)
        .setPBufferInfo(&descriptor_buffer_info);

    device.updateDescriptorSets(1, &descriptor_write, 0, nullptr);
  }

  ~SwapchainImage() {
    device.freeDescriptorSets(descriptorPool, 1, &descriptorSet);
    device.freeCommandBuffers(commandPool, 1, &commandBuffer);
    device.destroyBuffer(buffer);
    device.freeMemory(memory);
    device.destroyFramebuffer(framebuffer);
    device.destroyImageView(imageView);
    depthImage.destroy();
  }

  xr::SwapchainImageVulkanKHR image;
  vk::ImageView imageView;
  vk::Framebuffer framebuffer;
  vk::DeviceMemory memory;
  vk::Buffer buffer;
  vk::CommandBuffer commandBuffer;
  vk::DescriptorSet descriptorSet;

  mov::VkImage depthImage;

private:
  vk::Device device;
  vk::CommandPool commandPool;
  vk::DescriptorPool descriptorPool;
};

auto create_instance() {
  return xr::createInstance(
      {xr::InstanceCreateFlags(),
       xr::ApplicationInfo{applicationName,
                           static_cast<uint32_t>(XR_MAKE_VERSION(
                               majorVersion, minorVersion, patchVersion)),
                           applicationName,
                           static_cast<uint32_t>(XR_MAKE_VERSION(
                               majorVersion, minorVersion, patchVersion)),
                           xr::Version::current()},
       1, layerNames, _countof(extensionNames), extensionNames});
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

auto handle_xr_error(const XrDebugUtilsMessageSeverityFlagsEXT severity,
                     const XrDebugUtilsMessageTypeFlagsEXT type,
                     const XrDebugUtilsMessengerCallbackDataEXT *callback_data,
                     [[maybe_unused]] void *user_data) -> XrBool32 {
  const auto message_type =
      xrMessageTypeMap.contains(type) ? xrMessageTypeMap.at(type) : "Unknown";
  const auto message_severity = xrMessageSeverityMap.contains(severity)
                                    ? xrMessageSeverityMap.at(severity)
                                    : spdlog::level::err;

  spdlog::log(message_severity, "OpenXR {}: {}", message_type,
              callback_data->message);

  return XR_FALSE;
}

auto create_debug_messenger(const xr::Instance instance)
    -> xr::DebugUtilsMessengerEXT {
  return instance.createDebugUtilsMessengerEXT(
      {xr::DebugUtilsMessageSeverityFlagBitsEXT::AllBits,
       xr::DebugUtilsMessageTypeFlagBitsEXT::AllBits, handle_xr_error, nullptr},
      xr::DispatchLoaderDynamic(instance));
}

auto get_system(const xr::Instance instance) {
  const xr::SystemGetInfo system_get_info{xr::FormFactor::HeadMountedDisplay};

  return instance.getSystem(system_get_info);
}

auto get_vulkan_instance_requirements(const xr::Instance instance,
                                      const xr::SystemId system)
    -> std::tuple<xr::GraphicsRequirementsVulkanKHR, std::set<std::string>> {
  std::set<std::string> extensions;

  const auto graphics_requirements = instance.getVulkanGraphicsRequirementsKHR(
      system, xr::DispatchLoaderDynamic(instance));

  const auto extensions_raw = instance.getVulkanInstanceExtensionsKHR(
      system, xr::DispatchLoaderDynamic(instance));
  std::istringstream iss(extensions_raw);

  std::string extension;
  while (iss >> extension)
    extensions.insert(extension);

  return {graphics_requirements, extensions};
}

auto get_vulkan_device_requirements(const xr::Instance instance,
                                    const xr::SystemId system,
                                    const vk::Instance vulkan_instance)
    -> std::tuple<VkPhysicalDevice, std::set<std::string>> {
  VkPhysicalDevice physical_device;

  if (const auto result = instance.getVulkanGraphicsDeviceKHR(
          system, vulkan_instance, &physical_device,
          xr::DispatchLoaderDynamic(instance));
      result != xr::Result::Success) {
    spdlog::error("Failed to get Vulkan graphics device: {}",
                  xr::to_string_literal(result));
    return {VK_NULL_HANDLE, {}};
  }

  const auto extensions_raw = instance.getVulkanDeviceExtensionsKHR(
      system, xr::DispatchLoaderDynamic(instance));
  std::istringstream iss(extensions_raw);
  std::set<std::string> device_extensions;

  std::string extension;
  while (iss >> extension)
    device_extensions.insert(extension);

  return {physical_device, device_extensions};
}

auto create_vulkan_instance(
    const xr::GraphicsRequirementsVulkanKHR graphics_requirements,
    const std::set<std::string> &instance_extensions) {
  std::vector<const char *> extensions;
  extensions.reserve(instance_extensions.size());
  std::ranges::transform(instance_extensions, std::back_inserter(extensions),
                         [](const auto &str) { return str.c_str(); });

  for (const auto &extension : vulkanExtensionNames)
    extensions.push_back(extension);

  const vk::ApplicationInfo application_info{
      applicationName,
      VK_MAKE_VERSION(majorVersion, minorVersion, patchVersion),
      applicationName,
      VK_MAKE_VERSION(majorVersion, minorVersion, patchVersion),
      static_cast<uint32_t>(
          graphics_requirements.minApiVersionSupported.get())};

  const vk::InstanceCreateInfo create_info{vk::InstanceCreateFlags{},
                                           &application_info, vulkanLayerNames,
                                           extensions};

  return vk::createInstance(create_info);
}

XrBool32
handle_vk_error(const VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                const VkDebugUtilsMessageTypeFlagsEXT type,
                const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
                void *userData) {
  const auto message_severity = vkMessageSeverityMap.contains(severity)
                                    ? vkMessageSeverityMap.at(severity)
                                    : spdlog::level::err;

  if (strcmp(callback_data->pMessageIdName,
             "UNASSIGNED-CoreValidation-DrawState-InvalidImageLayout"))
    spdlog::log(
        message_severity, "Vulkan {}: {}",
        vk::to_string(static_cast<vk::DebugUtilsMessageTypeFlagsEXT>(type)),
        callback_data->pMessage);

  return XR_FALSE;
}

// ReSharper disable CppInconsistentNaming
PFN_vkCreateDebugUtilsMessengerEXT pfnVkCreateDebugUtilsMessengerEXT;
PFN_vkDestroyDebugUtilsMessengerEXT pfnVkDestroyDebugUtilsMessengerEXT;

// ReSharper disable CppParameterMayBeConst
VKAPI_ATTR VkResult VKAPI_CALL vkCreateDebugUtilsMessengerEXT(
    VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
    const VkAllocationCallbacks *pAllocator,
    VkDebugUtilsMessengerEXT *pMessenger) {
  return pfnVkCreateDebugUtilsMessengerEXT(instance, pCreateInfo, pAllocator,
                                           pMessenger);
}

VKAPI_ATTR void VKAPI_CALL vkDestroyDebugUtilsMessengerEXT(
    VkInstance instance, VkDebugUtilsMessengerEXT messenger,
    VkAllocationCallbacks const *pAllocator) {
  return pfnVkDestroyDebugUtilsMessengerEXT(instance, messenger, pAllocator);
}
// ReSharper restore CppParameterMayBeConst
// ReSharper restore CppInconsistentNaming

auto create_vulkan_debug_messenger(const vk::Instance instance) {
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
          instance.getProcAddr("vkCreateDebugUtilsMessengerEXT"));

  const auto result = instance.createDebugUtilsMessengerEXT(
      {{}, severity_flags, message_types, handle_vk_error});

  return result;
}

auto destroy_vulkan_debug_messenger(
    const vk::Instance instance,
    const vk::DebugUtilsMessengerEXT debug_messenger) {
  pfnVkDestroyDebugUtilsMessengerEXT =
      reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
          instance.getProcAddr("vkDestroyDebugUtilsMessengerEXT"));

  instance.destroyDebugUtilsMessengerEXT(debug_messenger);
}

auto get_device_queue_family(const vk::PhysicalDevice physical_device) {
  uint32_t graphics_queue_family_index = -1;

  const std::vector<vk::QueueFamilyProperties> queue_families =
      physical_device.getQueueFamilyProperties();

  for (uint32_t i = 0; i < queue_families.size(); i++) {
    if (queue_families[i].queueFlags & vk::QueueFlagBits::eGraphics) {
      graphics_queue_family_index = i;
      break;
    }
  }

  if (graphics_queue_family_index == -1) {
    spdlog::error("No graphics queue found.");
    return graphics_queue_family_index;
  }

  return graphics_queue_family_index;
}

auto create_device(const vk::PhysicalDevice physical_device,
                   const uint32_t graphics_queue_family_index,
                   const std::set<std::string> &device_extensions)
    -> std::tuple<vk::Device, vk::Queue> {
  std::vector<const char *> extensions;
  extensions.reserve(device_extensions.size());
  std::ranges::transform(device_extensions, std::back_inserter(extensions),
                         [](const auto &str) { return str.c_str(); });

  float priority = 1;

  vk::DeviceQueueCreateInfo queue_create_info{
      vk::DeviceQueueCreateFlags{}, graphics_queue_family_index, 1, &priority};

  vk::PhysicalDeviceFeatures physical_features{};
  physical_features.setSamplerAnisotropy(true);

  vk::DeviceCreateInfo create_info{};
  create_info.setQueueCreateInfos(queue_create_info)
      .setPEnabledExtensionNames(extensions)
      .setPEnabledFeatures(&physical_features);

  auto device = physical_device.createDevice(create_info);

  auto queue = device.getQueue(graphics_queue_family_index, 0);

  return {device, queue};
}

auto create_render_pass(const vk::Device device,
                        const vk::PhysicalDevice physical_device) {
  vk::AttachmentDescription attachment{};
  attachment.setFormat(vk::Format::eR8G8B8A8Srgb)
      .setSamples(vk::SampleCountFlagBits::e1)
      .setLoadOp(vk::AttachmentLoadOp::eClear)
      .setStoreOp(vk::AttachmentStoreOp::eStore)
      .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
      .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
      .setInitialLayout(vk::ImageLayout::eUndefined)
      .setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal);

  vk::AttachmentReference attachment_ref{};
  attachment_ref.setAttachment(0).setLayout(
      vk::ImageLayout::eColorAttachmentOptimal);

  vk::AttachmentDescription depth_attachment{};
  depth_attachment.setFormat(find_depth_format(physical_device))
      .setSamples(vk::SampleCountFlagBits::e1)
      .setLoadOp(vk::AttachmentLoadOp::eClear)
      .setStoreOp(vk::AttachmentStoreOp::eStore)
      .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
      .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
      .setInitialLayout(vk::ImageLayout::eUndefined)
      .setFinalLayout(vk::ImageLayout::eDepthAttachmentOptimal);

  vk::AttachmentReference depth_ref{};
  depth_ref.setAttachment(1).setLayout(
      vk::ImageLayout::eDepthStencilAttachmentOptimal);

  vk::SubpassDescription subpass{};
  subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
      .setColorAttachments(attachment_ref)
      .setPDepthStencilAttachment(&depth_ref);

  vk::SubpassDependency dependency{};
  dependency.setSrcSubpass(VK_SUBPASS_EXTERNAL)
  .setDstSubpass(0)
  .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
  .setSrcAccessMask(vk::AccessFlagBits::eNone)
  .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
  .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

  vk::SubpassDependency depth_dependency{};
  depth_dependency.setSrcSubpass(VK_SUBPASS_EXTERNAL)
      .setDstSubpass(0)
      .setSrcStageMask(vk::PipelineStageFlagBits::eEarlyFragmentTests |
                       vk::PipelineStageFlagBits::eLateFragmentTests)
      .setSrcAccessMask(vk::AccessFlagBits::eNone)
      .setDstStageMask(vk::PipelineStageFlagBits::eEarlyFragmentTests |
                       vk::PipelineStageFlagBits::eLateFragmentTests)
      .setDstAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentWrite);

  vk::AttachmentDescription attachments[2] = {attachment, depth_attachment};
  vk::SubpassDependency dependencies[2] = {dependency, depth_dependency};

  vk::RenderPassCreateInfo create_info{};
  create_info.setAttachments(attachments).setSubpasses(subpass).setDependencies(dependencies);

  return device.createRenderPass(create_info, nullptr);
}

auto create_command_pool(const vk::Device device,
                         const uint32_t graphics_queue_family_index) {
  return device.createCommandPool(
      {vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
       graphics_queue_family_index});
}

auto create_descriptor_pool(const vk::Device device) {
  vk::DescriptorPoolSize pool_size{};
  pool_size.setType(vk::DescriptorType::eUniformBuffer).setDescriptorCount(32);

  vk::DescriptorPoolCreateInfo create_info{};
  create_info.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
      .setMaxSets(32)
      .setPoolSizes(pool_size);

  return device.createDescriptorPool(create_info);
}

auto create_descriptor_set_layout(const vk::Device device) {
  vk::DescriptorSetLayoutBinding binding{};
  binding.setBinding(0)
      .setDescriptorType(vk::DescriptorType::eUniformBuffer)
      .setDescriptorCount(1)
      .setStageFlags(vk::ShaderStageFlagBits::eVertex);

  vk::DescriptorSetLayoutCreateInfo create_info{};
  create_info.setBindings(binding);

  return device.createDescriptorSetLayout(create_info);
}

auto create_shader(const vk::Device device, const std::string &path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  const std::streamsize file_size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<uint32_t> bytes(file_size);
  file.read(reinterpret_cast<char *>(bytes.data()), file_size);

  vk::ShaderModuleCreateInfo create_info{};
  create_info.setCode(bytes).setCodeSize(file_size);

  return device.createShaderModule(create_info);
}

auto create_pipeline(vk::Device device, vk::RenderPass render_pass,
                     vk::DescriptorSetLayout descriptor_set_layout,
                     vk::ShaderModule vertex_shader,
                     vk::ShaderModule fragment_shader)
    -> std::tuple<vk::PipelineLayout, vk::Pipeline> {
  vk::PipelineLayoutCreateInfo layout_create_info{};
  layout_create_info.setSetLayouts(descriptor_set_layout)
      .setPushConstantRanges(
          vk::PushConstantRange()
              .setOffset(0)
              .setSize(sizeof PushConstants)
              .setStageFlags(vk::ShaderStageFlagBits::eVertex));

  auto pipeline_layout = device.createPipelineLayout(layout_create_info);

  auto binding_descriptors = mov::Vertex::get_binding_description();
  auto attribute_descriptors = mov::Vertex::get_attribute_descriptions();

  vk::PipelineVertexInputStateCreateInfo vertex_input_stage{};
  vertex_input_stage.setVertexBindingDescriptions(binding_descriptors)
      .setVertexAttributeDescriptions(attribute_descriptors);

  vk::PipelineInputAssemblyStateCreateInfo input_assembly_stage{};
  input_assembly_stage.setTopology(vk::PrimitiveTopology::eTriangleList)
      .setPrimitiveRestartEnable(false);

  vk::PipelineShaderStageCreateInfo vertex_shader_stage{};
  vertex_shader_stage.setStage(vk::ShaderStageFlagBits::eVertex)
      .setModule(vertex_shader)
      .setPName("main");

  const vk::Viewport viewport = {0, 0, 1024, 1024, 0, 1};
  constexpr vk::Rect2D scissor = {{0, 0}, {1024, 1024}};

  vk::PipelineViewportStateCreateInfo viewport_stage{};
  viewport_stage.setViewports(viewport).setScissors(scissor);

  vk::PipelineRasterizationStateCreateInfo rasterization_stage{};
  rasterization_stage.setDepthClampEnable(false)
      .setRasterizerDiscardEnable(false)
      .setPolygonMode(vk::PolygonMode::eFill)
      .setLineWidth(1)
      .setCullMode(vk::CullModeFlagBits::eNone)
      .setFrontFace(vk::FrontFace::eCounterClockwise)
      .setDepthBiasEnable(false)
      .setDepthBiasConstantFactor(0)
      .setDepthBiasClamp(0)
      .setDepthBiasSlopeFactor(0);

  vk::PipelineMultisampleStateCreateInfo multisample_stage{};
  multisample_stage.setRasterizationSamples(vk::SampleCountFlagBits::e1)
      .setSampleShadingEnable(false)
      .setMinSampleShading(0.25);

  vk::PipelineDepthStencilStateCreateInfo depth_stencil_stage{};
  depth_stencil_stage.setDepthTestEnable(true)
      .setDepthWriteEnable(true)
      .setDepthCompareOp(vk::CompareOp::eLess)
      .setDepthBoundsTestEnable(false)
      .setMinDepthBounds(0)
      .setMaxDepthBounds(1)
      .setStencilTestEnable(false);

  vk::PipelineShaderStageCreateInfo fragment_shader_stage{};
  fragment_shader_stage.setStage(vk::ShaderStageFlagBits::eFragment)
      .setModule(fragment_shader)
      .setPName("main");

  vk::PipelineColorBlendAttachmentState color_blend_attachment{};
  color_blend_attachment
      .setColorWriteMask(
          vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
          vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA)
      .setBlendEnable(true)
      .setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
      .setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
      .setColorBlendOp(vk::BlendOp::eAdd)
      .setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
      .setDstAlphaBlendFactor(vk::BlendFactor::eOne)
      .setAlphaBlendOp(vk::BlendOp::eAdd);

  vk::PipelineColorBlendStateCreateInfo color_blend_stage{};
  color_blend_stage.setLogicOpEnable(false)
      .setLogicOp(vk::LogicOp::eCopy)
      .setAttachments(color_blend_attachment)
      .setBlendConstants(std::array{0.f, 0.f, 0.f, 0.f});

  vk::DynamicState dynamic_states[] = {vk::DynamicState::eViewport,
                                       vk::DynamicState::eScissor};

  vk::PipelineDynamicStateCreateInfo dynamic_state{};
  dynamic_state.setDynamicStates(dynamic_states);

  vk::PipelineShaderStageCreateInfo shader_stages[] = {vertex_shader_stage,
                                                       fragment_shader_stage};

  vk::GraphicsPipelineCreateInfo create_info{};
  create_info.setStages(shader_stages)
      .setPVertexInputState(&vertex_input_stage)
      .setPInputAssemblyState(&input_assembly_stage)
      .setPTessellationState(nullptr)
      .setPViewportState(&viewport_stage)
      .setPRasterizationState(&rasterization_stage)
      .setPMultisampleState(&multisample_stage)
      .setPDepthStencilState(&depth_stencil_stage)
      .setPColorBlendState(&color_blend_stage)
      .setPDynamicState(&dynamic_state)
      .setLayout(pipeline_layout)
      .setRenderPass(render_pass)
      .setSubpass(0)
      .setBasePipelineHandle(nullptr)
      .setBasePipelineIndex(-1);

  const auto result = device.createGraphicsPipeline(nullptr, create_info);

  if (result.result != vk::Result::eSuccess) {
    spdlog::error("Failed to create Vulkan pipeline: {}",
                  vk::to_string(result.result));
    return {VK_NULL_HANDLE, VK_NULL_HANDLE};
  }

  return {pipeline_layout, result.value};
}

auto create_session(const xr::Instance instance, const xr::SystemId system_id,
                    const vk::Instance vulkan_instance,
                    const vk::PhysicalDevice phys_device,
                    const vk::Device device,
                    const uint32_t queue_family_index) {
  xr::GraphicsBindingVulkanKHR graphics_binding{vulkan_instance, phys_device,
                                                device, queue_family_index, 0};

  const xr::SessionCreateInfo session_create_info{
      xr::SessionCreateFlagBits::None, system_id, &graphics_binding};

  return instance.createSession(session_create_info);
}

auto create_swapchains(const xr::Instance instance, const xr::SystemId system,
                       const xr::Session session)
    -> std::tuple<Swapchain *, Swapchain *> {
  const std::vector<xr::ViewConfigurationView> config_views =
      instance.enumerateViewConfigurationViewsToVector(
          system, xr::ViewConfigurationType::PrimaryStereo);

  const std::vector<int64_t> formats =
      session.enumerateSwapchainFormatsToVector();
  int64_t chosen_format = formats.front();

  for (const int64_t format : formats) {
    if (format == VK_FORMAT_R8G8B8A8_SRGB) {
      chosen_format = format;
      break;
    }
  }

  xr::Swapchain swapchains[eyeCount];

  for (uint32_t i = 0; i < eyeCount; i++) {
    xr::SwapchainCreateInfo swapchain_create_info{
        xr::SwapchainCreateFlagBits::None,
        xr::SwapchainUsageFlagBits::ColorAttachment,
        chosen_format,
        static_cast<uint32_t>(vk::SampleCountFlagBits::e1),
        config_views[i].recommendedImageRectWidth,
        config_views[i].recommendedImageRectHeight,
        1,
        1,
        1};

    swapchains[i] = session.createSwapchain(swapchain_create_info);
  }

  return {new Swapchain(swapchains[0], static_cast<vk::Format>(chosen_format),
                        config_views[0].recommendedImageRectWidth,
                        config_views[0].recommendedImageRectHeight),
          new Swapchain(swapchains[1], static_cast<vk::Format>(chosen_format),
                        config_views[1].recommendedImageRectWidth,
                        config_views[1].recommendedImageRectHeight)};
}

auto create_space(const xr::Session session,
                  xr::ReferenceSpaceType type = xr::ReferenceSpaceType::Stage) {
  return session.createReferenceSpace({type, {{0, 0, 0, 1}, {0, 0, 0}}});
}

auto render_eye(Swapchain *swapchain,
                const std::vector<SwapchainImage *> &images, xr::View view,
                vk::Device device, vk::Queue queue, vk::RenderPass render_pass,
                vk::PipelineLayout pipeline_layout, vk::Pipeline pipeline) {
  uint32_t active_index;

  swapchain->swapchain.acquireSwapchainImage({}, &active_index);

  swapchain->swapchain.waitSwapchainImage(
      {xr::Duration{std::numeric_limits<int64_t>::max()}});

  const SwapchainImage *image = images[active_index];

  auto data = static_cast<float *>(device.mapMemory(image->memory, 0, ~0, {}));

  float angle_width = tan(view.fov.angleRight) - tan(view.fov.angleLeft);
  float angle_height = tan(view.fov.angleDown) - tan(view.fov.angleUp);

  float projection_matrix[4][4]{{0}};

  projection_matrix[0][0] = 2.0f / angle_width;
  projection_matrix[2][0] =
      (tan(view.fov.angleRight) + tan(view.fov.angleLeft)) / angle_width;
  projection_matrix[1][1] = 2.0f / angle_height;
  projection_matrix[2][1] =
      (tan(view.fov.angleUp) + tan(view.fov.angleDown)) / angle_height;
  projection_matrix[2][2] = -farDistance / (farDistance - nearDistance);
  projection_matrix[3][2] =
      -(farDistance * nearDistance) / (farDistance - nearDistance);
  projection_matrix[2][3] = -1;

  auto view_matrix = inverse(
      translate(glm::mat4(1.0f),
                glm::vec3(view.pose.position.x, view.pose.position.y,
                          view.pose.position.z)) *
      mat4_cast(glm::quat(view.pose.orientation.w, view.pose.orientation.x,
                          view.pose.orientation.y, view.pose.orientation.z)));

  memcpy(data, projection_matrix, sizeof(float) * 4 * 4);
  memcpy(4 * 4 + data, value_ptr(view_matrix), sizeof(float) * 4 * 4);

  device.unmapMemory(image->memory);

  vk::CommandBufferBeginInfo begin_info{};
  begin_info.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

  image->commandBuffer.begin(&begin_info);

  vk::ClearValue clear_value{};
  clear_value.setColor({0.f, 0.f, 0.f, 1.f});

  vk::ClearValue depth_value{};
  depth_value.setDepthStencil({1.0f, 0});

  vk::ClearValue clear_values[2] = {clear_value, depth_value};

  vk::RenderPassBeginInfo begin_render_pass_info{};
  begin_render_pass_info.setRenderPass(render_pass)
      .setFramebuffer(image->framebuffer)
      .setRenderArea({{0, 0}, {(swapchain->width), (swapchain->height)}})
      .setClearValues(clear_values);

  image->commandBuffer.beginRenderPass(&begin_render_pass_info,
                                       vk::SubpassContents::eInline);

  vk::Viewport viewport = {0,
                           0,
                           static_cast<float>(swapchain->width),
                           static_cast<float>(swapchain->height),
                           0,
                           1};

  image->commandBuffer.setViewport(0, 1, &viewport);

  vk::Rect2D scissor = {{0, 0}, {swapchain->width, swapchain->height}};

  image->commandBuffer.setScissor(0, 1, &scissor);

  image->commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);

  image->commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                          pipeline_layout, 0, 1,
                                          &image->descriptorSet, 0, nullptr);

  vk::Buffer vertex_buffers[] = {vertex_buffer.buffer};
  vk::DeviceSize offsets[] = {0};

  image->commandBuffer.bindVertexBuffers(0, 1, vertex_buffers, offsets);
  image->commandBuffer.bindIndexBuffer(index_buffer.buffer, 0,
                                       vk::IndexType::eUint16);

  auto constants = PushConstants{
      glm::translate(glm::identity<glm::mat4>(),
                     glm::vec3(objectPos.x, objectPos.y, objectPos.z))};

  image->commandBuffer.pushConstants(pipeline_layout,
                                     vk::ShaderStageFlagBits::eVertex, 0,
                                     sizeof PushConstants, &constants);

  image->commandBuffer.drawIndexed(static_cast<uint32_t>(indices.size()), 1, 0,
                                   0, 0);

  constants =
      PushConstants{glm::translate(glm::identity<glm::mat4>(),
                                   glm::vec3(right_hand_pos.x, right_hand_pos.y,
                                             right_hand_pos.z)) *
                    glm::mat4_cast(glm::quat(
                        right_hand_orientation.w, right_hand_orientation.x,
                        right_hand_orientation.y, right_hand_orientation.z)) *
                    glm::scale(glm::identity<glm::mat4>(), glm::vec3(0.1f))};

  image->commandBuffer.pushConstants(pipeline_layout,
                                     vk::ShaderStageFlagBits::eVertex, 0,
                                     sizeof PushConstants, &constants);

  image->commandBuffer.drawIndexed(static_cast<uint32_t>(indices.size()), 1, 0,
                                   0, 0);

  image->commandBuffer.endRenderPass();
  image->commandBuffer.end();

  vk::PipelineStageFlags stage_mask =
      vk::PipelineStageFlagBits::eColorAttachmentOutput;

  vk::SubmitInfo submit_info{};
  submit_info.setWaitDstStageMask(stage_mask)
      .setCommandBuffers(image->commandBuffer)
      .setWaitSemaphoreCount(0);

  queue.submit(1, &submit_info, nullptr);

  swapchain->swapchain.releaseSwapchainImage({});

  return true;
}

auto render(const xr::Session session, Swapchain *swapchains[2],
            std::vector<SwapchainImage *> swapchain_images[2],
            const xr::Space space, xr::Time predicted_display_type,
            const VkDevice device, const VkQueue queue,
            const VkRenderPass render_pass,
            const VkPipelineLayout pipeline_layout, const VkPipeline pipeline) {
  session.beginFrame({});

  XrViewState view_state{.type = XR_TYPE_VIEW_STATE};

  constexpr uint32_t view_count = eyeCount;
  const std::vector<xr::View> views = session.locateViewsToVector(
      {xr::ViewConfigurationType::PrimaryStereo, predicted_display_type, space},
      &view_state);

  for (size_t i = 0; i < eyeCount; i++) {
    render_eye(swapchains[i], swapchain_images[i], views[i], device, queue,
               render_pass, pipeline_layout, pipeline);
  }

  xr::CompositionLayerProjectionView projected_views[2]{};

  for (size_t i = 0; i < eyeCount; i++) {
    projected_views[i].pose = views[i].pose;
    projected_views[i].fov = views[i].fov;
    projected_views[i].subImage =
        xr::SwapchainSubImage{swapchains[i]->swapchain,
                              {{0, 0},
                               {static_cast<int32_t>(swapchains[i]->width),
                                static_cast<int32_t>(swapchains[i]->height)}},
                              0};
  }

  const xr::CompositionLayerProjection layer{
      xr::CompositionLayerFlagBits::None, space, view_count, projected_views};

  auto p_layer =
      reinterpret_cast<const xr::CompositionLayerBaseHeader *>(&layer);

  session.endFrame(
      {predicted_display_type, xr::EnvironmentBlendMode::Opaque, 1, &p_layer});
  return true;
}

auto create_action_set(const xr::Instance instance, const char *name,
                       const char *localized_name) {
  return instance.createActionSet({name, localized_name, 0});
}

auto create_action(const xr::ActionSet action_set, const char *name,
                   const char *localized_name, xr::ActionType type) {
  return action_set.createAction({name, type, 0, nullptr, localized_name});
}

auto create_action_space(const xr::Session session, const xr::Action action) {
  return session.createActionSpace(
      {action, xr::Path::null(), {{0, 0, 0, 1}, {0, 0, 0}}});
}

auto suggest_bindings(const xr::Instance instance, xr::Action left_hand_action,
                      xr::Action right_hand_action, xr::Action left_grab_action,
                      xr::Action right_grab_action) {
  const auto left_hand_path =
      instance.stringToPath("/user/hand/left/input/grip/pose");
  const auto right_hand_path =
      instance.stringToPath("/user/hand/right/input/grip/pose");
  const auto left_hand_button_path =
      instance.stringToPath("/user/hand/left/input/x/click");
  const auto right_hand_button_path =
      instance.stringToPath("/user/hand/right/input/a/click");
  const auto interaction_profile_path =
      instance.stringToPath("/interaction_profiles/oculus/touch_controller");

  xr::ActionSuggestedBinding suggested_bindings[] = {
      {left_hand_action, left_hand_path},
      {right_hand_action, right_hand_path},
      {left_grab_action, left_hand_button_path},
      {right_grab_action, right_hand_button_path},
  };

  instance.suggestInteractionProfileBindings({interaction_profile_path,
                                              _countof(suggested_bindings),
                                              suggested_bindings});
}

auto attach_action_set(const xr::Session session, xr::ActionSet action_set) {
  session.attachSessionActionSets({1, &action_set});
}

auto get_action_boolean(const xr::Session session, const xr::Action action) {
  return session.getActionStateBoolean({action, xr::Path::null()})
             .currentState == true;
}

auto get_action_pose(const xr::Session session, const xr::Action action,
                     const xr::Space space, const xr::Space room_space,
                     const xr::Time predicted_display_time) {
  if (!session.getActionStatePose({action, xr::Path::null()}).isActive)
    return xr::Posef{};
  return space.locateSpace(room_space, predicted_display_time).pose;
}

auto input(const xr::Session session, const xr::ActionSet action_set,
           const xr::Space room_space, const xr::Time predicted_display_time,
           const xr::Action left_hand_action,
           const xr::Action right_hand_action,
           const xr::Action left_grab_action,
           const xr::Action right_grab_action, const xr::Space left_hand_space,
           const xr::Space right_hand_space) {
  xr::ActiveActionSet active_action_set = {action_set, xr::Path::null()};

  if (const auto sync_result = session.syncActions({1, &active_action_set});
      sync_result == xr::Result::SessionNotFocused) {
    return true;
  } else if (sync_result != xr::Result::Success) {
    spdlog::error("Failed to synchronize actions: {}",
                  xr::to_string_literal(sync_result));
    return false;
  }

  auto left_hand = get_action_pose(session, left_hand_action, left_hand_space,
                                   room_space, predicted_display_time);

  auto right_hand =
      get_action_pose(session, right_hand_action, right_hand_space, room_space,
                      predicted_display_time);

  right_hand_pos = right_hand.position;
  right_hand_orientation = right_hand.orientation;

  const auto left_grab = get_action_boolean(session, left_grab_action);
  const auto right_grab = get_action_boolean(session, right_grab_action);

  if (left_grab && !objectGrabbed &&
      sqrt(pow(objectPos.x - left_hand.position.x, 2) +
           pow(objectPos.y - left_hand.position.y, 2) +
           pow(objectPos.z - left_hand.position.z, 2)) < grabDistance) {
    objectGrabbed = 1;
  } else if (!left_grab && objectGrabbed == 1) {
    objectGrabbed = 0;
  }

  if (right_grab && !objectGrabbed &&
      sqrt(pow(objectPos.x - left_hand.position.x, 2) +
           pow(objectPos.y - left_hand.position.y, 2) +
           pow(objectPos.z - left_hand.position.z, 2)) < grabDistance) {
    objectGrabbed = 2;
  } else if (!right_grab && objectGrabbed == 2) {
    objectGrabbed = 0;
  }

  switch (objectGrabbed) {
  case 0:
    break;
  case 1:
    objectPos = left_hand.position;
    break;
  case 2:
    objectPos = right_hand.position;
    break;
  }

  return true;
}

int main(int, char **) {
#if defined _DEBUG
  spdlog::set_level(spdlog::level::trace);
#endif

  auto instance = create_instance();
  auto debug_messenger = create_debug_messenger(instance);
  const auto system = get_system(instance);

  auto [graphicsRequirements, instanceExtensions] =
      get_vulkan_instance_requirements(instance, system);
  const auto vulkan_instance =
      create_vulkan_instance(graphicsRequirements, instanceExtensions);
  const auto vulkan_debug_messenger =
      create_vulkan_debug_messenger(vulkan_instance);

  auto [physicalDevice, deviceExtensions] =
      get_vulkan_device_requirements(instance, system, vulkan_instance);
  const auto graphics_queue_family_index =
      get_device_queue_family(physicalDevice);
  auto [device, queue] = create_device(
      physicalDevice, graphics_queue_family_index, deviceExtensions);

  const auto render_pass = create_render_pass(device, physicalDevice);
  const auto command_pool =
      create_command_pool(device, graphics_queue_family_index);
  const auto descriptor_pool = create_descriptor_pool(device);
  const auto descriptor_set_layout = create_descriptor_set_layout(device);
  const auto vertex_shader = create_shader(device, "data\\vertex.vert.spv");
  const auto fragment_shader = create_shader(device, "data\\fragment.frag.spv");
  auto [pipelineLayout, pipeline] =
      create_pipeline(device, render_pass, descriptor_set_layout, vertex_shader,
                      fragment_shader);

  vertex_buffer = mov::VkBuffer(device, physicalDevice, command_pool, queue,
                                vk::BufferUsageFlagBits::eVertexBuffer,
                                vertices.data(), vertices.size());

  index_buffer = mov::VkBuffer(device, physicalDevice, command_pool, queue,
                               vk::BufferUsageFlagBits::eIndexBuffer,
                               indices.data(), indices.size());

  auto session =
      create_session(instance, system, vulkan_instance, physicalDevice, device,
                     graphics_queue_family_index);

  Swapchain *swapchains[eyeCount];
  std::tie(swapchains[0], swapchains[1]) =
      create_swapchains(instance, system, session);

  std::vector<xr::SwapchainImageVulkanKHR> swapchain_images[eyeCount];

  for (size_t i = 0; i < eyeCount; i++) {
    swapchain_images[i] =
        swapchains[i]
            ->swapchain
            .enumerateSwapchainImagesToVector<xr::SwapchainImageVulkanKHR>();
  }

  std::vector<SwapchainImage *> wrapped_swapchain_images[eyeCount];

  for (size_t i = 0; i < eyeCount; i++) {
    wrapped_swapchain_images[i] =
        std::vector<SwapchainImage *>(swapchain_images[i].size(), nullptr);

    for (size_t j = 0; j < wrapped_swapchain_images[i].size(); j++) {
      wrapped_swapchain_images[i][j] = new SwapchainImage(
          physicalDevice, device, render_pass, command_pool, descriptor_pool,
          descriptor_set_layout, swapchains[i], swapchain_images[i][j]);
    }
  }

  auto space = create_space(session);

  auto action_set = create_action_set(instance, "default", "Default");

  auto left_hand_action = create_action(action_set, "left-hand", "Left Hand",
                                        xr::ActionType::PoseInput);
  auto right_hand_action = create_action(action_set, "right-hand", "Right Hand",
                                         xr::ActionType::PoseInput);
  auto left_grab_action = create_action(action_set, "left-grab", "Left Grab",
                                        xr::ActionType::BooleanInput);
  auto right_grab_action = create_action(action_set, "right-grab", "Right Grab",
                                         xr::ActionType::BooleanInput);

  auto left_hand_space = create_action_space(session, left_hand_action);
  auto right_hand_space = create_action_space(session, right_hand_action);

  suggest_bindings(instance.get(), left_hand_action, right_hand_action,
                   left_grab_action, right_grab_action);
  attach_action_set(session, action_set);

  signal(SIGINT, onInterrupt);

  bool running = false;
  while (!quit) {
    xr::EventDataBuffer event_data{};

    if (const auto result = instance.pollEvent(event_data);
        result == xr::Result::EventUnavailable) {
      if (running) {
        auto frame_state = session.waitFrame({}, {});

        if (!frame_state.shouldRender) {
          continue;
        }

        quit =
            !input(session, action_set, space, frame_state.predictedDisplayTime,
                   left_hand_action, right_hand_action, left_grab_action,
                   right_grab_action, left_hand_space, right_hand_space);

        quit = !render(session, swapchains, wrapped_swapchain_images, space,
                       frame_state.predictedDisplayTime, device, queue,
                       render_pass, pipelineLayout, pipeline);
      }
    } else if (result != xr::Result::Success) {
      spdlog::error("Failed to poll events: {}", xr::to_string_literal(result));
      break;
    } else {
      switch (event_data.type) {
      default:
        spdlog::error("Unknown event type received: {}",
                      xr::to_string_literal(event_data.type));
        break;
      case xr::StructureType::EventDataEventsLost:
        spdlog::error("Event queue overflowed and events were lost.");
        break;
      case xr::StructureType::EventDataInstanceLossPending:
        spdlog::error("OpenXR instance is shutting down.");
        quit = true;
        break;
      case xr::StructureType::EventDataInteractionProfileChanged:
        spdlog::info("The interaction profile has changed.");
        break;
      case xr::StructureType::EventDataReferenceSpaceChangePending:
        spdlog::info("The reference space is changing.");
        break;
      case xr::StructureType::EventDataSessionStateChanged: {
        switch (const auto event =
                    reinterpret_cast<xr::EventDataSessionStateChanged *>(
                        &event_data);
                event->state) {
        case xr::SessionState::Unknown:
          spdlog::error("Unknown session state entered: {}",
                        xr::to_string_literal(event->state));
          break;
        case xr::SessionState::Idle:
          running = false;
          break;
        case xr::SessionState::Ready: {
          session.beginSession({xr::ViewConfigurationType::PrimaryStereo});

          running = true;
          break;
        }
        case xr::SessionState::Synchronized:
        case xr::SessionState::Visible:
        case xr::SessionState::Focused:
          running = true;
          break;
        case xr::SessionState::Stopping:
          session.endSession();
          break;
        case xr::SessionState::LossPending:
          spdlog::info("OpenXR session is shutting down.");
          quit = true;
          break;
        case xr::SessionState::Exiting:
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

  left_hand_space.destroy();
  right_hand_space.destroy();

  right_grab_action.destroy();
  left_grab_action.destroy();
  right_hand_action.destroy();
  left_hand_action.destroy();

  action_set.destroy();

  space.destroy();

  for (const auto &wrappedSwapchainImage : wrapped_swapchain_images) {
    for (const auto &swapchain_image : wrappedSwapchainImage) {
      delete swapchain_image;
    }
  }

  for (const auto &swapchain : swapchains) {
    delete swapchain;
  }

  session.destroy();

  index_buffer.destroy();
  vertex_buffer.destroy();

  device.destroyPipeline(pipeline);
  device.destroyPipelineLayout(pipelineLayout);
  device.destroyShaderModule(fragment_shader);
  device.destroyShaderModule(vertex_shader);
  device.destroyDescriptorSetLayout(descriptor_set_layout);
  device.destroyDescriptorPool(descriptor_pool);
  device.destroyCommandPool(command_pool);
  device.destroyRenderPass(render_pass);

  device.destroy();

  destroy_vulkan_debug_messenger(vulkan_instance, vulkan_debug_messenger);
  vulkan_instance.destroy();

  debug_messenger.destroy(xr::DispatchLoaderDynamic(instance));
  instance.destroy();

  return 0;
}
