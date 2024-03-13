#pragma once
#include <vulkan/vulkan_core.h>

struct PhysicalDevice
{
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

    Bool are_families_valid() const
    {
        return computeFamily.has_value() && graphicsFamily.has_value() && presentFamily.has_value();
    }


    Bool check_extension_support() const
    {
        UInt32 extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        DynamicArray<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        Set<String> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

        for (const VkExtensionProperties& extension : availableExtensions)
        {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }

    VkFormat find_depth_format() const
    {
        return find_supported_format({
                                     VK_FORMAT_D32_SFLOAT,
                                     VK_FORMAT_D32_SFLOAT_S8_UINT,
                                     VK_FORMAT_D24_UNORM_S8_UINT
                                     },
                                     VK_IMAGE_TILING_OPTIMAL,
                                    VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    }

    VkFormat find_supported_format(const std::vector<VkFormat>& candidates,
                                   VkImageTiling tiling,
                                   VkFormatFeatureFlags features) const
    {
        for (const VkFormat format : candidates)
        {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(device, format, &props);

            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
            {
                return format;
            }
            else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
            {
                return format;
            }
        }

        throw std::runtime_error("failed to find supported format!");
    }
};