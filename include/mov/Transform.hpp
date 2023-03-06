#pragma once

#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace mov {

class Transform {
public:
  Transform() = default;

  glm::mat4 matrix() {
    if (dirty_) {
      matrix_ = glm::translate(
          glm::identity<glm::mat4>(),
          position_) *
      glm::mat4_cast(orientation_);

      dirty_ = false;
    }
    return matrix_;
  }

  Transform &move_abs(const glm::vec3 position) {
    dirty_ = true;
    position_ = position;

    return *this;
  }

  Transform &rotate_abs(const glm::quat orientation) {
    dirty_ = true;
    orientation_ = orientation;

    return *this;
  }

  constexpr static auto identity() {
    return Transform{glm::zero<glm::vec3>(), glm::identity<glm::quat>(),
                     glm::one<glm::vec3>()};
  }

private:
  constexpr Transform(const glm::vec3 position, const glm::quat rotation,
                      const glm::vec3 scale)
      : dirty_(true), position_(position), orientation_(rotation),
        scale_(scale), matrix_(glm::identity<glm::mat4>()) {}

  bool dirty_{};

  glm::vec3 position_{};
  glm::quat orientation_{};
  glm::vec3 scale_{};

  glm::mat4 matrix_{};
};

}; // namespace mov