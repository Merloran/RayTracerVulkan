#include "buffer.hpp"

#include "logical_device.hpp"
#include "physical_device.hpp"

Void Buffer::create(const PhysicalDevice& physicalDevice, const LogicalDevice& logicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, const VkAllocationCallbacks* allocator)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size        = size;
    bufferInfo.usage       = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    this->size = size;

    if (vkCreateBuffer(logicalDevice.get_device(), &bufferInfo, allocator, &buffer) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(logicalDevice.get_device(), buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memRequirements.size;
    allocInfo.memoryTypeIndex = physicalDevice.find_memory_type(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(logicalDevice.get_device(), &allocInfo, allocator, &memory) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate buffer memory!");
    }

    vkBindBufferMemory(logicalDevice.get_device(), buffer, memory, 0);
}

VkBuffer Buffer::get_buffer() const
{
    return buffer;
}

VkDeviceMemory Buffer::get_memory()
{
    return memory;
}

Void* Buffer::get_mapped_memory()
{
    return mappedMemory;
}

UInt64 Buffer::get_size() const
{
    return size;
}

Void Buffer::clear(const LogicalDevice& logicalDevice, const VkAllocationCallbacks* allocator)
{
    vkDestroyBuffer(logicalDevice.get_device(), buffer, allocator);
    vkFreeMemory(logicalDevice.get_device(), memory, allocator);
}
