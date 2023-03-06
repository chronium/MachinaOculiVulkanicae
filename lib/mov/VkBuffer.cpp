#include <mov/VkBuffer.hpp>
#include <mov/VkUtils.hpp>

#include <vulkan/vulkan.hpp>

namespace mov {

auto VkBufferProvider::create_buffer(
    const vk::DeviceSize size, const vk::BufferUsageFlags usage,
    const vk::MemoryPropertyFlags properties) const
    -> std::tuple<vk::Buffer, vk::DeviceMemory> {
  const auto buffer = device_.createBuffer(
      vk::BufferCreateInfo().setSize(size).setUsage(usage).setSharingMode(
          vk::SharingMode::eExclusive));

  const auto requirements = device_.getBufferMemoryRequirements(buffer);

  const auto memory = device_.allocateMemory(
      vk::MemoryAllocateInfo()
          .setAllocationSize(requirements.size)
          .setMemoryTypeIndex(find_memory_type(
              physical_device_, requirements.memoryTypeBits, properties)));
  device_.bindBufferMemory(buffer, memory, 0);

  return {buffer, memory};
}

auto VkBufferProvider::copy_buffer(const vk::Buffer src, const vk::Buffer dst,
                                   const vk::DeviceSize size) const {
  const auto command_buffer = device_.allocateCommandBuffers(
      vk::CommandBufferAllocateInfo()
          .setLevel(vk::CommandBufferLevel::ePrimary)
          .setCommandPool(command_pool_)
          .setCommandBufferCount(1))[0];

  command_buffer.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

  vk::BufferCopy copy_region{};
  copy_region.setSrcOffset(0).setDstOffset(0).setSize(size);
  command_buffer.copyBuffer(src, dst, copy_region);

  command_buffer.end();

  queue_.submit(vk::SubmitInfo().setCommandBuffers(command_buffer));
  queue_.waitIdle();

  device_.freeCommandBuffers(command_pool_, command_buffer);
}

template <typename T>
auto create_buffer(const VkBufferProvider provider,
                   const vk::BufferUsageFlags usage, const T *data,
                   const std::size_t count)
    -> std::tuple<vk::Buffer, vk::DeviceMemory> {
  const vk::DeviceSize buffer_size = sizeof data[0] * count;

  const auto [staging_buffer, staging_memory] =
      provider.create_buffer(buffer_size, vk::BufferUsageFlagBits::eTransferSrc,
                             vk::MemoryPropertyFlagBits::eHostVisible |
                                 vk::MemoryPropertyFlagBits::eHostCoherent);

  const auto dest = device(provider).mapMemory(staging_memory, 0, buffer_size);
  memcpy_s(dest, buffer_size, data, buffer_size);
  device(provider).unmapMemory(staging_memory);

  const auto [buffer, memory] = provider.create_buffer(
      buffer_size, vk::BufferUsageFlagBits::eTransferDst | usage,
      vk::MemoryPropertyFlagBits::eDeviceLocal);
  provider.copy_buffer(staging_buffer, buffer, buffer_size);

  device(provider).destroyBuffer(staging_buffer);
  device(provider).freeMemory(staging_memory);

  return {buffer, memory};
}

VkBuffer<Vertex>::VkBuffer(const VkBufferProvider provider,
                           const vk::BufferUsageFlags usage, const Vertex *data,
                           const std::size_t count)
    : provider_(provider) {
  auto [dst_buffer, dst_memory] = create_buffer(provider, usage, data, count);

  buffer = dst_buffer;
  memory = dst_memory;
}

VkBuffer<uint16_t>::VkBuffer(const VkBufferProvider provider,
                             const vk::BufferUsageFlags usage,
                             const uint16_t *data, const std::size_t count)
    : provider_(provider) {
  auto [dst_buffer, dst_memory] = create_buffer(provider, usage, data, count);

  buffer = dst_buffer;
  memory = dst_memory;
}

VkBuffer<uint32_t>::VkBuffer(const VkBufferProvider provider,
                             const vk::BufferUsageFlags usage,
                             const uint32_t *data, const std::size_t count)
    : provider_(provider) {
  auto [dst_buffer, dst_memory] = create_buffer(provider, usage, data, count);

  buffer = dst_buffer;
  memory = dst_memory;
}

} // namespace mov
