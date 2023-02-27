#include <mov/VkUtils.hpp>

#include <vulkan/vulkan.hpp>

namespace mov {

uint32_t find_memory_type(const vk::PhysicalDevice device,
                          const uint32_t type_filter,
                          const vk::MemoryPropertyFlags properties) {
  const auto mem_properties = device.getMemoryProperties();

  for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++)
    if (type_filter & (1 << i) && (mem_properties.memoryTypes[i].propertyFlags &
                                   properties) == properties)
      return i;

  throw std::runtime_error("Failed to find suitable memory type!");
}

}; // namespace mov
