#pragma once
#include <vulkan/vulkan.hpp>

class PhysicalDevice;
class LogicalDevice;

class Buffer
{
public:

	Void create(const PhysicalDevice& physicalDevice,
				const LogicalDevice& logicalDevice,
				VkDeviceSize size,
                VkBufferUsageFlags usage,
                VkMemoryPropertyFlags properties,
				const VkAllocationCallbacks* allocator);

	VkBuffer get_buffer() const;
	VkDeviceMemory get_memory() const;
	UInt64 get_size() const;

	Void clear(const LogicalDevice& logicalDevice, const VkAllocationCallbacks* allocator);

private:
	VkBuffer buffer;
    VkDeviceMemory memory;
    UInt64 size;
};

