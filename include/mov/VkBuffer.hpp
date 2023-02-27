#pragma once

#include <mov/Vertex.hpp>
#include <vulkan/vulkan.hpp>

namespace mov {

template <typename T> class VkBuffer {
public:
  VkBuffer() {}

  VkBuffer(vk::Device device, vk::PhysicalDevice physical_device,
           vk::CommandPool command_pool, vk::Queue queue,
           vk::BufferUsageFlags usage, const T *data, std::size_t count);

  VkBuffer(const VkBuffer &other) {
    this->buffer = other.buffer;
    this->memory = other.memory;
    this->device = other.device;
  }

  VkBuffer(VkBuffer &&other) {
    this->buffer = other.buffer;
    this->memory = other.memory;
    this->device = other.device;
  }

  VkBuffer &operator=(const VkBuffer &other) {
    this->buffer = other.buffer;
    this->memory = other.memory;
    this->device = other.device;

    return *this;
  }

  auto destroy() const {
    device.freeMemory(memory);
    device.destroyBuffer(buffer);
  }

  vk::Buffer buffer;
  vk::DeviceMemory memory;

private:
  vk::Device device;
};

extern template VkBuffer<Vertex>;
extern template VkBuffer<uint16_t>;

} // namespace mov
