#pragma once
#include <vulkan/vulkan.hpp>

class DebugMessenger;
class PhysicalDevice;

class LogicalDevice
{
public:
    Void create(const PhysicalDevice& physicalDevice, 
                const DebugMessenger& debugMessenger,
                const VkAllocationCallbacks* allocator);

    const VkDevice& get_device() const;
    VkSampleCountFlagBits get_samples() const;

    Void clear(const VkAllocationCallbacks* allocator);

private:
	VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    VkQueue computeQueue;
    VkSampleCountFlagBits samples;
};
