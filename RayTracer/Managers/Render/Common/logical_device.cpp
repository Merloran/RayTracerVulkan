#include "logical_device.hpp"

#include "physical_device.hpp"
#include "../render_manager.hpp"

Void LogicalDevice::create(const PhysicalDevice& physicalDevice, const VkAllocationCallbacks* allocator)
{
    SRenderManager& renderManager = SRenderManager::get();
    const DynamicArray<const Char*>& validationLayers = renderManager.get_validation_layers();
    DynamicArray<VkDeviceQueueCreateInfo> queueCreateInfos;
    Set<UInt32> uniqueQueueFamilies = { physicalDevice.graphicsFamily.value(), physicalDevice.presentFamily.value() };

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
    createInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount    = UInt32(queueCreateInfos.size());
    createInfo.pQueueCreateInfos       = queueCreateInfos.data();
    createInfo.pEnabledFeatures        = &deviceFeatures;
    createInfo.enabledExtensionCount   = UInt32(physicalDevice.deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = physicalDevice.deviceExtensions.data();
    if constexpr (SRenderManager::ENABLE_VALIDATION_LAYERS)
    {
        createInfo.enabledLayerCount   = UInt32(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    }
    else {
        createInfo.enabledLayerCount   = 0;
    }

    if (vkCreateDevice(physicalDevice.device, &createInfo, allocator, &device) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create logical device!");
    }
    samples = physicalDevice.maxSamples;
    vkGetDeviceQueue(device, physicalDevice.presentFamily.value(), 0, &presentQueue);
    vkGetDeviceQueue(device, physicalDevice.graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, physicalDevice.computeFamily.value(), 0, &computeQueue);
}

const VkDevice& LogicalDevice::get_device() const
{
    return device;
}

VkSampleCountFlagBits LogicalDevice::get_samples() const
{
    return samples;
}

Void LogicalDevice::clear()
{
    vkDestroyDevice(device, nullptr);
}
