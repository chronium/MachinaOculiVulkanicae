#pragma once

#include <vulkan/vulkan.hpp>

namespace mov {
extern uint32_t find_memory_type(vk::PhysicalDevice device,
                                 uint32_t type_filter,
                                 vk::MemoryPropertyFlags properties);
}