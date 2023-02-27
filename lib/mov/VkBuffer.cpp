#include <mov/VkBuffer.hpp>
#include <mov/VkUtils.hpp>

#include <vulkan/vulkan.hpp>

namespace mov {

auto create_buffer(const vk::Device device,
                   const vk::PhysicalDevice physical_device,
                   const vk::DeviceSize size, const vk::BufferUsageFlags usage,
                   const vk::MemoryPropertyFlags properties)
    -> std::tuple<vk::Buffer, vk::DeviceMemory> {
  const auto buffer = device.createBuffer(
      vk::BufferCreateInfo().setSize(size).setUsage(usage).setSharingMode(
          vk::SharingMode::eExclusive));

  const auto requirements = device.getBufferMemoryRequirements(buffer);

  const auto memory = device.allocateMemory(
      vk::MemoryAllocateInfo()
          .setAllocationSize(requirements.size)
          .setMemoryTypeIndex(find_memory_type(
              physical_device, requirements.memoryTypeBits, properties)));
  device.bindBufferMemory(buffer, memory, 0);

  return {buffer, memory};
}

auto copy_buffer(const vk::Device device, const vk::CommandPool command_pool,
                 const vk::Queue graphics_queue, const vk::Buffer src_buffer,
                 const vk::Buffer dst_buffer, const vk::DeviceSize size) {
  const auto command_buffer = device.allocateCommandBuffers(
      vk::CommandBufferAllocateInfo()
          .setLevel(vk::CommandBufferLevel::ePrimary)
          .setCommandPool(command_pool)
          .setCommandBufferCount(1))[0];

  command_buffer.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

  vk::BufferCopy copy_region{};
  copy_region.setSrcOffset(0).setDstOffset(0).setSize(size);
  command_buffer.copyBuffer(src_buffer, dst_buffer, copy_region);

  command_buffer.end();

  graphics_queue.submit(vk::SubmitInfo().setCommandBuffers(command_buffer));
  graphics_queue.waitIdle();

  device.freeCommandBuffers(command_pool, command_buffer);
}

template <typename T>
auto create_buffer(const vk::Device device,
                   const vk::PhysicalDevice physical_device,
                   const vk::CommandPool command_pool, const vk::Queue queue,
                   const vk::BufferUsageFlags usage, const T *data,
                   const std::size_t count)
    -> std::tuple<vk::Buffer, vk::DeviceMemory> {
  const vk::DeviceSize buffer_size = sizeof data[0] * count;

  const auto [staging_buffer, staging_memory] =
      create_buffer(device, physical_device, buffer_size,
                    vk::BufferUsageFlagBits::eTransferSrc,
                    vk::MemoryPropertyFlagBits::eHostVisible |
                        vk::MemoryPropertyFlagBits::eHostCoherent);

  const auto dest = device.mapMemory(staging_memory, 0, buffer_size);
  memcpy_s(dest, buffer_size, data, buffer_size);
  device.unmapMemory(staging_memory);

  const auto [buffer, memory] =
      create_buffer(device, physical_device, buffer_size,
                    vk::BufferUsageFlagBits::eTransferDst | usage,
                    vk::MemoryPropertyFlagBits::eDeviceLocal);
  copy_buffer(device, command_pool, queue, staging_buffer, buffer, buffer_size);

  device.destroyBuffer(staging_buffer);
  device.freeMemory(staging_memory);

  return {buffer, memory};
}

VkBuffer<Vertex>::VkBuffer(const vk::Device device,
                           const vk::PhysicalDevice physical_device,
                           const vk::CommandPool command_pool,
                           const vk::Queue queue,
                           const vk::BufferUsageFlags usage, const Vertex *data,
                           const std::size_t count)
    : device_(device) {
  auto [dst_buffer, dst_memory] = create_buffer(
      device, physical_device, command_pool, queue, usage, data, count);

  buffer = dst_buffer;
  memory = dst_memory;
}

VkBuffer<uint16_t>::VkBuffer(const vk::Device device,
                             const vk::PhysicalDevice physical_device,
                             const vk::CommandPool command_pool,
                             const vk::Queue queue,
                             const vk::BufferUsageFlags usage,
                             const uint16_t *data, const std::size_t count)
    : device_(device) {
  auto [dst_buffer, dst_memory] = create_buffer(
      device, physical_device, command_pool, queue, usage, data, count);

  buffer = dst_buffer;
  memory = dst_memory;
}

} // namespace mov
