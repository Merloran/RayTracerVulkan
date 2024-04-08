#include "physical_device.hpp"


Void PhysicalDevice::select_physical_device(VkInstance instance,VkSurfaceKHR surface)
{
    // Check for device with Vulkan support
    UInt32 deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0)
    {
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }

    // Check for device that is suitable
    DynamicArray<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
    for (VkPhysicalDevice device : devices)
    {
        this->device = device;

        if (is_device_suitable(surface))
        {
            setup_max_sample_count();
            break;
        }

        this->device = VK_NULL_HANDLE;
    }

    if (this->device == VK_NULL_HANDLE)
    {
        throw std::runtime_error("failed to find a suitable GPU!");
    }
}

VkPhysicalDevice PhysicalDevice::get_device() const
{
    return device;
}

UInt32 PhysicalDevice::get_graphics_family_index() const
{
    return graphicsFamily.value();
}

UInt32 PhysicalDevice::get_compute_family_index() const
{
    return computeFamily.value();
}

UInt32 PhysicalDevice::get_present_family_index() const
{
    return presentFamily.value();
}

VkSampleCountFlagBits PhysicalDevice::get_max_samples() const
{
    return maxSamples;
}

VkSurfaceCapabilitiesKHR PhysicalDevice::get_capabilities(VkSurfaceKHR surface) const
{
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &capabilities);
    return capabilities;
}

const VkPhysicalDeviceProperties& PhysicalDevice::get_properties() const
{
    return properties;
}

DynamicArray<VkSurfaceFormatKHR> PhysicalDevice::get_formats(VkSurfaceKHR surface) const
{
    UInt32 formatCount;
    DynamicArray<VkSurfaceFormatKHR> formats;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

    if (formatCount != 0)
    {
        formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, formats.data());
    }

    return formats;
}

DynamicArray<VkPresentModeKHR> PhysicalDevice::get_present_modes(VkSurfaceKHR surface) const
{
    DynamicArray<VkPresentModeKHR> presentModes;
    UInt32 presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device,
                                              surface,
                                              &presentModeCount,
                                              nullptr);
    if (presentModeCount != 0)
    {
        presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, presentModes.data());
    }

    return presentModes;
}

const DynamicArray<const Char*>& PhysicalDevice::get_device_extensions() const
{
    return deviceExtensions;
}


VkFormat PhysicalDevice::find_depth_format() const
{
    return find_supported_format({
                                 VK_FORMAT_D32_SFLOAT,
                                 VK_FORMAT_D32_SFLOAT_S8_UINT,
                                 VK_FORMAT_D24_UNORM_S8_UINT
                                 },
                                 VK_IMAGE_TILING_OPTIMAL,
                                VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

VkFormat PhysicalDevice::find_supported_format(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const
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

VkFormatProperties PhysicalDevice::get_format_properties(VkFormat format) const
{
    VkFormatProperties properties;
    vkGetPhysicalDeviceFormatProperties(device, format, &properties);
    return properties;
}

UInt32 PhysicalDevice::find_memory_type(UInt32 typeFilter, VkMemoryPropertyFlags properties) const
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(device, &memProperties);

    for (UInt32 i = 0; i < memProperties.memoryTypeCount; ++i)
    {
        if (typeFilter & (1 << i) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}

Bool PhysicalDevice::is_device_suitable(VkSurfaceKHR surface)
{
    find_queue_families(surface);

    Bool isSwapChainAdequate = false;
    if (check_extension_support())
    {
        isSwapChainAdequate = !get_formats(surface).empty() && !get_present_modes(surface).empty();
    }

    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

    return are_families_valid() &&
           isSwapChainAdequate &&
           supportedFeatures.samplerAnisotropy;
}

Void PhysicalDevice::find_queue_families(VkSurfaceKHR surface)
{
    UInt32 queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    DynamicArray<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    for (UInt32 i = 0; i < UInt32(queueFamilies.size()); ++i)
    {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT &&
            queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
        {
            graphicsFamily = i;
            computeFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if (presentSupport)
        {
            presentFamily = i;
        }

        if (are_families_valid())
        {
            break;
        }
    }
}

Void PhysicalDevice::setup_max_sample_count()
{
    vkGetPhysicalDeviceProperties(device, &properties);
    VkSampleCountFlags counts = properties.limits.framebufferColorSampleCounts
							  & properties.limits.framebufferDepthSampleCounts;


    if (counts & VK_SAMPLE_COUNT_64_BIT)
    {
        maxSamples = VK_SAMPLE_COUNT_64_BIT;
    }
    else if (counts & VK_SAMPLE_COUNT_32_BIT)
    {
        maxSamples = VK_SAMPLE_COUNT_32_BIT;
    }
    else if (counts & VK_SAMPLE_COUNT_16_BIT)
    {
        maxSamples = VK_SAMPLE_COUNT_16_BIT;
    }
    else if (counts & VK_SAMPLE_COUNT_8_BIT)
    {
        maxSamples = VK_SAMPLE_COUNT_8_BIT;
    }
    else if (counts & VK_SAMPLE_COUNT_4_BIT)
    {
        maxSamples = VK_SAMPLE_COUNT_4_BIT;
    }
    else if (counts & VK_SAMPLE_COUNT_2_BIT)
    {
        maxSamples = VK_SAMPLE_COUNT_2_BIT;
    } else {
        maxSamples = VK_SAMPLE_COUNT_1_BIT;
    }
}

Bool PhysicalDevice::are_families_valid() const
{
    return computeFamily.has_value() && graphicsFamily.has_value() && presentFamily.has_value();
}

Bool PhysicalDevice::check_extension_support() const
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