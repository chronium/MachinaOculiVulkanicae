#pragma once

#include <vulkan/vulkan.hpp>

namespace mov {

class VkImage {
public:
  VkImage() = default;

  VkImage(vk::Device device, vk::PhysicalDevice physical_device, uint32_t width,
          uint32_t height, vk::Format format, vk::ImageTiling tiling,
          vk::ImageAspectFlags aspect, vk::ImageUsageFlags usage,
          vk::MemoryPropertyFlags properties);

  VkImage(const VkImage &other) = delete;
  VkImage(VkImage &&other) = delete;
  VkImage &operator=(const VkImage &other) = delete;

  VkImage &operator=(VkImage &&other) noexcept {
    this->image = other.image;
    this->memory = other.memory;
    this->image_view = other.image_view;
    this->sampler = other.sampler;
    this->width = other.width;
    this->height = other.height;
    this->device_ = other.device_;

    return *this;
  }

  auto destroy() const {
    device_.freeMemory(memory);
    device_.destroySampler(sampler);
    device_.destroyImageView(image_view);
    device_.destroyImage(image);
  }

  vk::Image image;
  vk::DeviceMemory memory;

  vk::ImageView image_view;
  vk::Sampler sampler;

  uint32_t width;
  uint32_t height;

private:
  static vk::ImageView create_view(vk::Device device, vk::Image image,
                                   vk::Format format,
                                   vk::ImageAspectFlags aspect);

  static vk::Sampler create_sampler(vk::Device device,
                                    vk::PhysicalDevice physical_device);

  vk::Device device_;
};

}; // namespace mov