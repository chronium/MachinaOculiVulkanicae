#pragma once

#include <mov/Vertex.hpp>

#include <vulkan/vulkan.hpp>

namespace mov {

template <typename T> class VkBuffer;

class VkBufferProvider {
public:
  VkBufferProvider() = default;

  VkBufferProvider(const vk::Device device,
                   const vk::PhysicalDevice physical_device,
                   const vk::CommandPool command_pool, const vk::Queue queue)
      : device_(device), physical_device_(physical_device),
        command_pool_(command_pool), queue_(queue) {}

  [[nodiscard]] auto create_buffer(vk::DeviceSize size,
                                   vk::BufferUsageFlags usage,
                                   vk::MemoryPropertyFlags properties) const
      -> std::tuple<vk::Buffer, vk::DeviceMemory>;

  auto copy_buffer(vk::Buffer src, vk::Buffer dst, vk::DeviceSize size) const;

private:
  vk::Device device_;
  vk::PhysicalDevice physical_device_;
  vk::CommandPool command_pool_;
  vk::Queue queue_;

  friend vk::Device device(const VkBufferProvider &provider) {
    return provider.device_;
  }

  template <typename T> friend class VkBuffer;
};

template <typename T> class VkBuffer {
public:
  VkBuffer() {}

  VkBuffer(VkBufferProvider provider, vk::BufferUsageFlags usage, const T *data,
           std::size_t count);

  VkBuffer(const VkBuffer &other) {
    this->buffer = other.buffer;
    this->memory = other.memory;
    this->provider_ = other.provider_;
  }

  VkBuffer(VkBuffer &&other) noexcept {
    this->buffer = other.buffer;
    this->memory = other.memory;
    this->provider_ = other.provider_;
  }

  VkBuffer &operator=(const VkBuffer &other) {
    this->buffer = other.buffer;
    this->memory = other.memory;
    this->provider_ = other.provider_;

    return *this;
  }

  [[nodiscard]] auto destroy() const {
    provider_.device_.freeMemory(memory);
    provider_.device_.destroyBuffer(buffer);
  }

  vk::Buffer buffer;
  vk::DeviceMemory memory;

private:
  VkBufferProvider provider_;
};

extern template VkBuffer<Vertex>;
extern template VkBuffer<uint16_t>;
extern template VkBuffer<uint32_t>;

} // namespace mov
