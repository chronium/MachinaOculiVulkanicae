#include <mov/GameObject.hpp>
#include <mov/PushConstants.hpp>

namespace mov {

void GameObject::draw(const vk::CommandBuffer commands,
                      const vk::PipelineLayout pipeline) {
  draw(commands, pipeline, transform.matrix());
}

void GameObject::draw(const vk::CommandBuffer commands,
                      const vk::PipelineLayout pipeline,
                      const glm::mat4 matrix) {
  for (auto &mesh : meshes_) {
    PushConstants push_constants{matrix};

    commands.pushConstants(pipeline, vk::ShaderStageFlagBits::eVertex, 0,
                           sizeof PushConstants, &push_constants);

    mesh.draw(commands);
  }
}

void GameObject::destroy() {
  for (auto &mesh : meshes_)
    mesh.destroy();
}

}; // namespace mov