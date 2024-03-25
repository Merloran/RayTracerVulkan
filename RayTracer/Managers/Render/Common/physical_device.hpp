#pragma once
#include <vulkan/vulkan.hpp>

class PhysicalDevice
{
public:

    Void select_physical_device(VkInstance instance, VkSurfaceKHR surface);

    [[nodiscard]]
    VkPhysicalDevice get_device() const;
    [[nodiscard]]
	UInt32 get_graphics_family_index() const;
    [[nodiscard]]
	UInt32 get_compute_family_index() const;
    [[nodiscard]]
	UInt32 get_present_family_index() const;
    [[nodiscard]]
    VkSampleCountFlagBits get_max_samples() const;
    [[nodiscard]]
    const VkSurfaceCapabilitiesKHR &get_capabilities() const;
    [[nodiscard]]
    const VkPhysicalDeviceProperties &get_properties() const;
    [[nodiscard]]
    const DynamicArray<VkSurfaceFormatKHR> &get_formats() const;
    [[nodiscard]]
    const DynamicArray<VkPresentModeKHR> &get_present_modes() const;
    [[nodiscard]]
    const DynamicArray<const Char*> &get_device_extensions() const;
    [[nodiscard]]
    VkFormat find_depth_format() const;
	[[nodiscard]]
    VkFormat find_supported_format(const std::vector<VkFormat>& candidates,
                                   VkImageTiling tiling,
                                   VkFormatFeatureFlags features) const;
	[[nodiscard]]
    UInt32 find_memory_type(UInt32 typeFilter, VkMemoryPropertyFlags properties) const;

private:
	VkPhysicalDevice device;
    VkSampleCountFlagBits maxSamples;
    VkSurfaceCapabilitiesKHR capabilities;
    VkPhysicalDeviceFeatures supportedFeatures;
    VkPhysicalDeviceProperties properties;
    Optional<UInt32> computeFamily;
    Optional<UInt32> graphicsFamily;
    Optional<UInt32> presentFamily;
    DynamicArray<VkSurfaceFormatKHR> formats;
    DynamicArray<VkPresentModeKHR> presentModes;
    const DynamicArray<const Char*> deviceExtensions =
    {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    Bool is_device_suitable(VkSurfaceKHR surface);
    Void find_queue_families(VkSurfaceKHR surface);
    Void has_swapchain_support(VkSurfaceKHR surface);
    Void setup_max_sample_count();
    Bool are_families_valid() const;
    Bool check_extension_support() const;
};