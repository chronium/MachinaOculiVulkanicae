#pragma once

#include <mov/Mesh.hpp>
#include <mov/Transform.hpp>

#include <vulkan/vulkan.hpp>

namespace mov {

class GameObject {
public:
  GameObject() = default;
  virtual ~GameObject() = default;

  GameObject(const Mesh &mesh)
      : transform(Transform::identity()), meshes_(std::vector<Mesh>()) {
    meshes_.push_back(mesh);
  }

  virtual void draw(vk::CommandBuffer, vk::PipelineLayout);
  virtual void draw(vk::CommandBuffer, vk::PipelineLayout, glm::mat4);

  virtual void destroy();

  Transform transform;

private:
  std::vector<Mesh> meshes_;
};

}; // namespace mov