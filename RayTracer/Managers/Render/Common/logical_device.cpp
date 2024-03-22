#include "logical_device.hpp"

#include "physical_device.hpp"
#include "debug_messenger.hpp"

Void LogicalDevice::create(const PhysicalDevice& physicalDevice, const DebugMessenger& debugMessenger, const VkAllocationCallbacks* allocator)
{
    const DynamicArray<const Char*>& validationLayers = debugMessenger.get_validation_layers();
    DynamicArray<VkDeviceQueueCreateInfo> queueCreateInfos;
    Set<UInt32> uniqueQueueFamilies = 
    {
    	physicalDevice.get_graphics_family_index(),
    	physicalDevice.get_present_family_index()
    };

    const Float32 queuePriority = 1.0f;
    for (const UInt32 queueFamily : uniqueQueueFamilies)
    {
        //QUEUE CREATE INFO
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    //PHYSICAL DEVICE FEATURES
    VkPhysicalDeviceFeatures deviceFeatures{};

    //DEVICE CREATE INFO
    VkDeviceCreateInfo createInfo{};
    const DynamicArray<const Char*>& deviceExtensions = physicalDevice.get_device_extensions();
    createInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount    = UInt32(queueCreateInfos.size());
    createInfo.pQueueCreateInfos       = queueCreateInfos.data();
    createInfo.pEnabledFeatures        = &deviceFeatures;
    createInfo.enabledExtensionCount   = UInt32(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();
    if constexpr (DebugMessenger::ENABLE_VALIDATION_LAYERS)
    {
        createInfo.enabledLayerCount   = UInt32(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount   = 0;
    }

    if (vkCreateDevice(physicalDevice.get_device(), &createInfo, allocator, &device) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create logical device!");
    }
    samples = physicalDevice.get_max_samples();
    vkGetDeviceQueue(device, physicalDevice.get_present_family_index(), 0, &presentQueue);
    vkGetDeviceQueue(device, physicalDevice.get_graphics_family_index(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, physicalDevice.get_compute_family_index(), 0, &computeQueue);
}

const VkDevice& LogicalDevice::get_device() const
{
    return device;
}

VkSampleCountFlagBits LogicalDevice::get_samples() const
{
    return samples;
}

Void LogicalDevice::clear(const VkAllocationCallbacks* allocator)
{
    vkDestroyDevice(device, allocator);
}
