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
    VkSurfaceCapabilitiesKHR get_capabilities(VkSurfaceKHR surface) const;
    [[nodiscard]]
    const VkPhysicalDeviceProperties &get_properties() const;
    [[nodiscard]]
    DynamicArray<VkSurfaceFormatKHR> get_formats(VkSurfaceKHR surface) const;
    [[nodiscard]]
    DynamicArray<VkPresentModeKHR> get_present_modes(VkSurfaceKHR surface) const;
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

    template <typename FeatureType>
    Bool are_features_supported(const FeatureType& requestedFeatures) const
    {
        FeatureType supportedFeatures{};
        supportedFeatures.sType = requestedFeatures.sType;
        supportedFeatures.pNext = nullptr;

        VkPhysicalDeviceFeatures2 deviceFeatures;
        deviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        deviceFeatures.pNext = &supportedFeatures;
        vkGetPhysicalDeviceFeatures2(device, &deviceFeatures);
        
        const UInt32* requestedFeature = reinterpret_cast<const UInt32*>(&requestedFeatures);
        const UInt32* supportedFeature = reinterpret_cast<const UInt32*>(&supportedFeatures);

        Bool result = true;
        const UInt32 elementsCount = sizeof(FeatureType) / sizeof(UInt32);
        const UInt32 firstFeatureOffset = 3;
        for (UInt32 i = firstFeatureOffset; i < elementsCount; ++i)
        {
	        if (requestedFeature[i] > supportedFeature[i])
	        {
                //TODO: think how to get name of feature(structure field)
                SPDLOG_ERROR("Feature {} in order not supported!", (i - firstFeatureOffset));
                result = false;
	        }
        }

        return result;
    }

    template <>
    Bool are_features_supported(const VkPhysicalDeviceFeatures& requestedFeatures) const
    {
        VkPhysicalDeviceFeatures deviceFeatures;
        vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

        const UInt32* requestedFeature = reinterpret_cast<const UInt32*>(&requestedFeatures);
        const UInt32* supportedFeature = reinterpret_cast<const UInt32*>(&deviceFeatures);
        
        Bool result = true;
        const UInt32 elementsCount = sizeof(VkPhysicalDeviceFeatures) / sizeof(UInt32);
        const UInt32 firstFeatureOffset = 0;
        for (UInt32 i = firstFeatureOffset; i < elementsCount; ++i)
        {
            if (requestedFeature[i] > supportedFeature[i])
            {
                //TODO: think how to get name of feature(structure field)
                SPDLOG_ERROR("Feature {} in order not supported!", (i - firstFeatureOffset));
                result = false;
            }
        }

        return result;
    }

private:
	VkPhysicalDevice device;
    VkSampleCountFlagBits maxSamples;
    VkPhysicalDeviceProperties properties;
    Optional<UInt32> computeFamily;
    Optional<UInt32> graphicsFamily;
    Optional<UInt32> presentFamily;
    const DynamicArray<const Char*> deviceExtensions =
    {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
    };

    Bool is_device_suitable(VkSurfaceKHR surface);
    Void find_queue_families(VkSurfaceKHR surface);
    Void setup_max_sample_count();
    Bool are_families_valid() const;
    Bool check_extension_support() const;
};
