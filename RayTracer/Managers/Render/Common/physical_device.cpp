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
    for (const VkPhysicalDevice& device : devices)
    {
        this-> device = device;

        if (is_device_suitable(surface))
        {
            get_max_sample_count();
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

const VkSurfaceCapabilitiesKHR& PhysicalDevice::get_capabilities() const
{
    return capabilities;
}

const VkPhysicalDeviceProperties& PhysicalDevice::get_properties() const
{
    return properties;
}

const DynamicArray<VkSurfaceFormatKHR>& PhysicalDevice::get_formats() const
{
    return formats;
}

const DynamicArray<VkPresentModeKHR>& PhysicalDevice::get_present_modes() const
{
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
        has_swapchain_support(surface);
        isSwapChainAdequate = !formats.empty() && !presentModes.empty();
    }

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

Void PhysicalDevice::has_swapchain_support(VkSurfaceKHR surface)
{
    UInt32 formatCount;
    UInt32 presentModeCount;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &capabilities);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

    if (formatCount != 0)
    {
        formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device,
                                             surface,
                                             &formatCount,
                                             formats.data());
    }

    vkGetPhysicalDeviceSurfacePresentModesKHR(device,
                                              surface,
                                              &presentModeCount,
                                              nullptr);

    if (presentModeCount != 0)
    {
        presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device,
                                                  surface,
                                                  &presentModeCount,
                                                  presentModes.data());
    }
}

Void PhysicalDevice::get_max_sample_count()
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
    if (counts & VK_SAMPLE_COUNT_16_BIT)
    {
        maxSamples = VK_SAMPLE_COUNT_16_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_8_BIT)
    {
        maxSamples = VK_SAMPLE_COUNT_8_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_4_BIT)
    {
        maxSamples = VK_SAMPLE_COUNT_4_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_2_BIT)
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