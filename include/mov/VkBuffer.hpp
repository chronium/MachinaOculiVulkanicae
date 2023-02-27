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
    this->device_ = other.device_;
  }

  VkBuffer(VkBuffer &&other) noexcept
  {
    this->buffer = other.buffer;
    this->memory = other.memory;
    this->device_ = other.device_;
  }

  VkBuffer &operator=(const VkBuffer &other) {
    this->buffer = other.buffer;
    this->memory = other.memory;
    this->device_ = other.device_;

    return *this;
  }

  [[nodiscard]] auto destroy() const {
    device_.freeMemory(memory);
    device_.destroyBuffer(buffer);
  }

  vk::Buffer buffer;
  vk::DeviceMemory memory;

private:
  vk::Device device_;
};

extern template VkBuffer<Vertex>;
extern template VkBuffer<uint16_t>;

} // namespace mov
