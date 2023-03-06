#include "Controller.hpp"

#include <vulkan/vulkan.hpp>

namespace mov::core {

void Controller::draw(const vk::CommandBuffer commands,
                      const vk::PipelineLayout pipeline) {
  GameObject::draw(commands, pipeline, transform.matrix() * custom_origin_);
}

}; // namespace mov::core