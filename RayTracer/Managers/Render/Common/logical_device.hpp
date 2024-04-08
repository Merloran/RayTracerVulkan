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

    VkDevice get_device() const;
    VkSampleCountFlagBits get_samples() const;

    [[nodiscard]]
    Bool is_multi_sampling_enabled() const;
    Void set_samples(VkSampleCountFlagBits samples);
    Void disable_multi_sampling();
    VkQueue get_graphics_queue() const;
    VkQueue get_compute_queue() const;
    VkQueue get_present_queue() const;

    Void clear(const VkAllocationCallbacks* allocator);

private:
	VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    VkQueue computeQueue;
    VkSampleCountFlagBits samples;
};
