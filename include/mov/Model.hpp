#pragma once

#include <mov/Vertex.hpp>
#include <mov/VkBuffer.hpp>

#include <vulkan/vulkan.hpp>

#include <vector>

namespace mov {

class Model {
public:
  Model() = default;

  Model(const mov::VkBufferProvider provider,
        const std::vector<Vertex> &vertices,
        const std::vector<uint32_t> &indices)
      : vertices_(provider, vk::BufferUsageFlagBits::eVertexBuffer,
                  vertices.data(), vertices.size()),
        indices_(provider, vk::BufferUsageFlagBits::eIndexBuffer,
                 indices.data(), indices.size()),
        index_count_(static_cast<uint32_t>(indices.size())) {}

  Model(const mov::Model &other) {
    this->vertices_ = other.vertices_;
    this->indices_ = other.indices_;
    this->index_count_ = other.index_count_;
  }

  Model &operator=(Model &&other) noexcept {
    this->vertices_ = other.vertices_;
    this->indices_ = other.indices_;
    this->index_count_ = other.index_count_;

    return *this;
  }

  Model &operator=(const Model &other) {
    this->vertices_ = other.vertices_;
    this->indices_ = other.indices_;
    this->index_count_ = other.index_count_;

    return *this;
  }

  auto draw(const vk::CommandBuffer command_buffer) const {
    constexpr vk::DeviceSize offsets[] = {0};

    command_buffer.bindVertexBuffers(0, 1, &vertices_.buffer, offsets);
    command_buffer.bindIndexBuffer(indices_.buffer, 0, vk::IndexType::eUint32);

    command_buffer.drawIndexed(index_count_, 1, 0, 0, 0);
  }

  auto destroy() const {
    vertices_.destroy();
    indices_.destroy();
  }

private:
  VkBuffer<Vertex> vertices_;
  VkBuffer<uint32_t> indices_;

  uint32_t index_count_{0};
};

}; // namespace mov
