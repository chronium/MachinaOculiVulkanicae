#include <mov/VkImage.hpp>
#include <mov/VkUtils.hpp>

namespace mov {

auto VkImage::create_view(const vk::Device device, const vk::Image image,
                          const vk::Format format,
                          const vk::ImageAspectFlags aspect) -> vk::ImageView
{
  const auto image_view_info =
      vk::ImageViewCreateInfo()
          .setImage(image)
          .setViewType(vk::ImageViewType::e2D)
          .setFormat(format)
          .setComponents(vk::ComponentMapping()
                             .setR(vk::ComponentSwizzle::eIdentity)
                             .setG(vk::ComponentSwizzle::eIdentity)
                             .setB(vk::ComponentSwizzle::eIdentity)
                             .setA(vk::ComponentSwizzle::eIdentity))
          .setSubresourceRange(vk::ImageSubresourceRange()
                                   .setAspectMask(aspect)
                                   .setBaseMipLevel(0)
                                   .setLevelCount(1)
                                   .setBaseArrayLayer(0)
                                   .setLayerCount(1));

  return device.createImageView(image_view_info);
}

auto VkImage::create_sampler(const vk::Device device,
                             const vk::PhysicalDevice physical_device) -> vk::Sampler
{
  const auto sampler_info =
      vk::SamplerCreateInfo()
          .setMagFilter(vk::Filter::eLinear)
          .setMinFilter(vk::Filter::eLinear)
          .setAddressModeU(vk::SamplerAddressMode::eRepeat)
          .setAddressModeV(vk::SamplerAddressMode::eRepeat)
          .setAddressModeW(vk::SamplerAddressMode::eRepeat)
          .setAnisotropyEnable(VK_TRUE)
          .setMaxAnisotropy(
              physical_device.getProperties().limits.maxSamplerAnisotropy)
          .setBorderColor(vk::BorderColor::eIntOpaqueBlack)
          .setUnnormalizedCoordinates(VK_FALSE)
          .setCompareEnable(VK_FALSE)
          .setCompareOp(vk::CompareOp::eAlways)
          .setMipmapMode(vk::SamplerMipmapMode::eLinear)
          .setMipLodBias(0.0f)
          .setMinLod(0.0f)
          .setMaxLod(0.0f);

  return device.createSampler(sampler_info);
}

VkImage::VkImage(const vk::Device device,
                 const vk::PhysicalDevice physical_device, const uint32_t width,
                 const uint32_t height, const vk::Format format,
                 const vk::ImageTiling tiling,
                 const vk::ImageAspectFlags aspect,
                 const vk::ImageUsageFlags usage,
                 const vk::MemoryPropertyFlags properties)
    : width(width), height(height), device_(device) {
  const auto image_info = vk::ImageCreateInfo()
                              .setImageType(vk::ImageType::e2D)
                              .setExtent(vk::Extent3D(width, height, 1))
                              .setMipLevels(1)
                              .setArrayLayers(1)
                              .setFormat(format)
                              .setTiling(tiling)
                              .setInitialLayout(vk::ImageLayout::eUndefined)
                              .setUsage(usage)
                              .setSharingMode(vk::SharingMode::eExclusive)
                              .setSamples(vk::SampleCountFlagBits::e1);

  image = device.createImage(image_info);

  const auto mem_requirements = device.getImageMemoryRequirements(image);

  const auto alloc_info =
      vk::MemoryAllocateInfo()
          .setAllocationSize(mem_requirements.size)
          .setMemoryTypeIndex(find_memory_type(
              physical_device, mem_requirements.memoryTypeBits, properties));

  memory = device.allocateMemory(alloc_info);

  device.bindImageMemory(image, memory, 0);

  image_view = VkImage::create_view(device, image, format, aspect);
  sampler = VkImage::create_sampler(device, physical_device);

}

}; // namespace mov
