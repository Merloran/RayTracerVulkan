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
        queueCreateInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount       = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    //PHYSICAL DEVICE FEATURES
    VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures{};
    descriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
    descriptorIndexingFeatures.pNext = nullptr;
	descriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing     = VK_TRUE;
    descriptorIndexingFeatures.descriptorBindingSampledImageUpdateAfterBind  = VK_TRUE;
    descriptorIndexingFeatures.shaderUniformBufferArrayNonUniformIndexing    = VK_TRUE;
    descriptorIndexingFeatures.descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE;
    descriptorIndexingFeatures.shaderStorageBufferArrayNonUniformIndexing    = VK_TRUE;
    descriptorIndexingFeatures.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
    descriptorIndexingFeatures.descriptorBindingPartiallyBound               = VK_TRUE;
    descriptorIndexingFeatures.runtimeDescriptorArray                        = VK_TRUE;

    VkPhysicalDeviceFeatures2 deviceFeatures{};
    deviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    deviceFeatures.pNext = &descriptorIndexingFeatures;
    deviceFeatures.features.samplerAnisotropy = VK_TRUE;
    deviceFeatures.features.sampleRateShading = VK_TRUE;
    if (!physicalDevice.are_features_supported(deviceFeatures.features) ||
        !physicalDevice.are_features_supported(descriptorIndexingFeatures))
    {
        return;
    }

    //DEVICE CREATE INFO
    VkDeviceCreateInfo createInfo{};
    const DynamicArray<const Char*>& deviceExtensions = physicalDevice.get_device_extensions();
    createInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount    = UInt32(queueCreateInfos.size());
    createInfo.pQueueCreateInfos       = queueCreateInfos.data();
    createInfo.pEnabledFeatures        = nullptr; // Device features 2 is used for this in pNext
    createInfo.pNext                   = &deviceFeatures;
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
    
    vkGetDeviceQueue(device, physicalDevice.get_present_family_index(), 0, &presentQueue);
    vkGetDeviceQueue(device, physicalDevice.get_graphics_family_index(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, physicalDevice.get_compute_family_index(), 0, &computeQueue);
}

VkDevice LogicalDevice::get_device() const
{
    return device;
}

VkQueue LogicalDevice::get_graphics_queue() const
{
    return graphicsQueue;
}

VkQueue LogicalDevice::get_compute_queue() const
{
    return computeQueue;
}

VkQueue LogicalDevice::get_present_queue() const
{
    return presentQueue;
}

Void LogicalDevice::clear(const VkAllocationCallbacks* allocator)
{
    vkDestroyDevice(device, allocator);
}
