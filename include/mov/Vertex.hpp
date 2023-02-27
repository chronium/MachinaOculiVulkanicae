#pragma once

#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

namespace mov {

struct Vertex {
  glm::vec3 pos;
  glm::vec3 color;

  static auto get_binding_description() -> vk::VertexInputBindingDescription {
    return vk::VertexInputBindingDescription()
        .setBinding(0)
        .setStride(sizeof(Vertex))
        .setInputRate(vk::VertexInputRate::eVertex);
  }

  static auto get_attribute_descriptions()
      -> std::array<vk::VertexInputAttributeDescription, 2> {
    return {vk::VertexInputAttributeDescription()
                .setBinding(0)
                .setLocation(0)
                .setFormat(vk::Format::eR32G32B32Sfloat)
                .setOffset(offsetof(Vertex, pos)),
            vk::VertexInputAttributeDescription()
                .setBinding(0)
                .setLocation(1)
                .setFormat(vk::Format::eR32G32B32Sfloat)
                .setOffset(offsetof(Vertex, color))};
  }
};

}; // namespace mov
