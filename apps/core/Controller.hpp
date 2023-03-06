#pragma once

#include <mov/GameObject.hpp>

#include <vulkan/vulkan.hpp>

namespace mov::core {
class Controller final : public GameObject {
public:
  Controller() = default;

  Controller(const mov::Mesh &mesh)
      : GameObject(mesh), custom_origin_(glm::identity<glm::mat4>()) {
    custom_origin_ =
        glm::translate(glm::rotate(glm::identity<glm::mat4>(),
                                   static_cast<float>(glm::radians(-20.6)),
                                   glm::vec3(1.f, 0.f, 0.f)),
                       -glm::vec3(-0.007, -0.00182941, 0.1019482));
  }

  void draw(vk::CommandBuffer, vk::PipelineLayout) override;

  ~Controller() override = default;

private:
  glm::mat4 custom_origin_;
};
}; // namespace mov::core
