#pragma once
#include <vulkan/vulkan.h>

struct PhysicalDevice;

class LogicalDevice
{
public:
    Void create(const PhysicalDevice& physicalDevice, const VkAllocationCallbacks* allocator);

    const VkDevice& get_device() const;
    VkSampleCountFlagBits get_samples() const;

    Void clear();

private:
	VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    VkQueue computeQueue;
    VkSampleCountFlagBits samples;
};
