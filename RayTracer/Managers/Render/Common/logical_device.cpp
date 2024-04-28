#include "logical_device.hpp"

#include <magic_enum.hpp>

#include "physical_device.hpp"
#include "debug_messenger.hpp"
#include "swapchain.hpp"
//TODO: make it more flexible
Void LogicalDevice::create(const PhysicalDevice& physicalDevice, const DebugMessenger& debugMessenger, const VkAllocationCallbacks* allocator)
{
    const DynamicArray<const Char*>& validationLayers = debugMessenger.get_validation_layers();
    DynamicArray<VkDeviceQueueCreateInfo> queueCreateInfos;
    Set<UInt32> uniqueQueueFamilies = 
    {
    	physicalDevice.get_graphics_family_index(),
    	physicalDevice.get_compute_family_index(),
    	physicalDevice.get_present_family_index()
    };
    
    Array<Float32, 2> priorities = { 1.0f, 1.0f };
    for (const UInt32 queueFamily : uniqueQueueFamilies)
    {
        //QUEUE CREATE INFO
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount       = UInt32(priorities.size());
        queueCreateInfo.pQueuePriorities = priorities.data();
        queueCreateInfos.push_back(queueCreateInfo);
    }

    //PHYSICAL DEVICE FEATURES
    VkPhysicalDeviceRobustness2FeaturesEXT robustness2Features{};
    robustness2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT;
    robustness2Features.pNext = nullptr;
    robustness2Features.nullDescriptor = VK_TRUE;

    VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures{};
    descriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
    descriptorIndexingFeatures.pNext = &robustness2Features;
	descriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing     = VK_TRUE;
    descriptorIndexingFeatures.descriptorBindingStorageImageUpdateAfterBind  = VK_TRUE;
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
        !physicalDevice.are_features_supported(descriptorIndexingFeatures) ||
        !physicalDevice.are_features_supported(robustness2Features))
    {
        return;
    }

    //DEVICE CREATE INFO
    VkDeviceCreateInfo createInfo{};
    const DynamicArray<const Char*>& deviceExtensions = physicalDevice.get_device_extensions();
    createInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount    = UInt32(queueCreateInfos.size());
    createInfo.pQueueCreateInfos       = queueCreateInfos.data();
    createInfo.pEnabledFeatures        = VK_NULL_HANDLE; // Device features 2 is used for this in pNext
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
    vkGetDeviceQueue(device, physicalDevice.get_compute_family_index(), 1, &computeQueue);
}

VkResult LogicalDevice::acquire_next_image(Swapchain& swapchain, VkSemaphore semaphore, VkFence fence, UInt64 timeout) const
{
    UInt32 imageIndex;
    const VkResult result = vkAcquireNextImageKHR(device,
                                                  swapchain.get_swapchain(),
                                                  timeout,
                                                  semaphore,
                                                  fence,
                                                  &imageIndex);
    swapchain.set_image_index(imageIndex);
	if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        SPDLOG_ERROR("Acquire image failed with: {}", magic_enum::enum_name(result));
    }
    return result;
}

VkResult LogicalDevice::wait_for_fences(DynamicArray<VkFence> fences, Bool waitAll, UInt64 timeout) const
{
    return vkWaitForFences(device, 
                           UInt32(fences.size()), 
                           fences.data(),
                           waitAll ? VK_TRUE : VK_FALSE,
                           timeout);
}

VkResult LogicalDevice::wait_for_fence(VkFence fence, Bool waitAll, UInt64 timeout) const
{
    return vkWaitForFences(device, 1, &fence, waitAll ? VK_TRUE : VK_FALSE, timeout);
}

VkResult LogicalDevice::reset_fences(DynamicArray<VkFence> fences) const
{
    return vkResetFences(device, UInt32(fences.size()), fences.data());
}

VkResult LogicalDevice::reset_fence(VkFence fence) const
{
    return vkResetFences(device, 1, &fence);
}

VkResult LogicalDevice::get_fence_status(VkFence fence) const
{
    return vkGetFenceStatus(device, fence);
}

VkResult LogicalDevice::submit_graphics_queue(const DynamicArray<VkSubmitInfo>& infos, VkFence fence) const
{
    VkResult result = vkQueueSubmit(graphicsQueue, UInt32(infos.size()), infos.data(), fence);
    if (result != VK_SUCCESS)
    {
        SPDLOG_ERROR("Submit graphics queue failed with: {}", magic_enum::enum_name(result));
    }
    return result;
}

VkResult LogicalDevice::submit_graphics_queue(const DynamicArray<VkSemaphore>& waitSemaphores, const DynamicArray<VkPipelineStageFlags>& waitStages, const DynamicArray<VkCommandBuffer>& commandBuffers, const DynamicArray<VkSemaphore>& signalSemaphores, VkFence fence, Void* next) const
{
    VkSubmitInfo info;
    info.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.pNext                = next;
    info.waitSemaphoreCount   = UInt32(waitSemaphores.size());
    info.pWaitSemaphores      = waitSemaphores.data();
    info.pWaitDstStageMask    = waitStages.data();
    info.commandBufferCount   = UInt32(commandBuffers.size());
    info.pCommandBuffers      = commandBuffers.data();
    info.signalSemaphoreCount = UInt32(signalSemaphores.size());
    info.pSignalSemaphores    = signalSemaphores.data();

    VkResult result = vkQueueSubmit(graphicsQueue, 1, &info, fence);
    if (result != VK_SUCCESS)
    {
        SPDLOG_ERROR("Submit graphics queue failed with: {}", magic_enum::enum_name(result));
    }
    return result;
}

VkResult LogicalDevice::submit_graphics_queue(VkSemaphore waitSemaphore, VkPipelineStageFlags waitStage, VkCommandBuffer commandBuffer, VkSemaphore signalSemaphore, VkFence fence, Void* next) const
{
    VkSubmitInfo info;
    info.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.pNext                = next;
    info.waitSemaphoreCount   = waitSemaphore ? 1 : 0;
    info.pWaitSemaphores      = &waitSemaphore;
    info.pWaitDstStageMask    = &waitStage;
    info.commandBufferCount   = commandBuffer ? 1 : 0;
    info.pCommandBuffers      = &commandBuffer;
    info.signalSemaphoreCount = signalSemaphore ? 1 : 0;
    info.pSignalSemaphores    = &signalSemaphore;

    VkResult result = vkQueueSubmit(graphicsQueue, 1, &info, fence);
    if (result != VK_SUCCESS)
    {
        SPDLOG_ERROR("Submit graphics queue failed with: {}", magic_enum::enum_name(result));
    }
    return result;
}

VkResult LogicalDevice::submit_compute_queue(const DynamicArray<VkSubmitInfo>& infos, VkFence fence) const
{
    VkResult result = vkQueueSubmit(computeQueue, UInt32(infos.size()), infos.data(), fence);
    if (result != VK_SUCCESS)
    {
        SPDLOG_ERROR("Submit compute queue failed with: {}", magic_enum::enum_name(result));
    }
    return result;
}

VkResult LogicalDevice::submit_compute_queue(const DynamicArray<VkSemaphore>& waitSemaphores, const DynamicArray<VkPipelineStageFlags>& waitStages, const DynamicArray<VkCommandBuffer>& commandBuffers, const DynamicArray<VkSemaphore>& signalSemaphores, VkFence fence, Void* next) const
{
    VkSubmitInfo info;
    info.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.pNext                = next;
    info.waitSemaphoreCount   = UInt32(waitSemaphores.size());
    info.pWaitSemaphores      = waitSemaphores.data();
    info.pWaitDstStageMask    = waitStages.data();
    info.commandBufferCount   = UInt32(commandBuffers.size());
    info.pCommandBuffers      = commandBuffers.data();
    info.signalSemaphoreCount = UInt32(signalSemaphores.size());
    info.pSignalSemaphores    = signalSemaphores.data();
    
    VkResult result = vkQueueSubmit(computeQueue, 1, &info, fence);
    if (result != VK_SUCCESS)
    {
        SPDLOG_ERROR("Submit compute queue failed with: {}", magic_enum::enum_name(result));
    }
    return result;
}

VkResult LogicalDevice::submit_compute_queue(VkSemaphore waitSemaphore, VkPipelineStageFlags waitStage, VkCommandBuffer commandBuffer, VkSemaphore signalSemaphore, VkFence fence, Void* next) const
{
    VkSubmitInfo info{};
    info.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.pNext                = next;
    info.waitSemaphoreCount   = waitSemaphore ? 1 : 0;
    info.pWaitSemaphores      = &waitSemaphore;
    info.pWaitDstStageMask    = &waitStage;
    info.commandBufferCount   = commandBuffer ? 1 : 0;
    info.pCommandBuffers      = &commandBuffer;
    info.signalSemaphoreCount = signalSemaphore ? 1 : 0;
    info.pSignalSemaphores    = &signalSemaphore;
    
    VkResult result = vkQueueSubmit(computeQueue, 1, &info, fence);
    if (result != VK_SUCCESS)
    {
        SPDLOG_ERROR("Submit compute queue failed with: {}", magic_enum::enum_name(result));
    }
    return result;
}

VkResult LogicalDevice::submit_present_queue(const DynamicArray<VkSemaphore>& waitSemaphores, const DynamicArray<Swapchain>& swapchains, VkResult* results, Void* next) const
{
    DynamicArray<VkSwapchainKHR> vkSwapchains;
    vkSwapchains.reserve(swapchains.size());
    DynamicArray<UInt32> imageIndexes;
    imageIndexes.reserve(swapchains.size());
    for (const Swapchain& swapchain : swapchains)
    {
        vkSwapchains.push_back(swapchain.get_swapchain());
        imageIndexes.push_back(swapchain.get_image_index());
    }

    VkPresentInfoKHR info{};
    info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.pNext              = next;
    info.waitSemaphoreCount = UInt32(waitSemaphores.size());
    info.pWaitSemaphores    = waitSemaphores.data();
    info.swapchainCount     = UInt32(swapchains.size());
    info.pSwapchains        = vkSwapchains.data();
    info.pImageIndices      = imageIndexes.data();
    info.pResults           = results;

    VkResult result = vkQueuePresentKHR(presentQueue, &info);
    if (result != VK_SUCCESS && result != VK_ERROR_OUT_OF_DATE_KHR && result != VK_SUBOPTIMAL_KHR)
    {
        SPDLOG_ERROR("Submit present queue failed with: {}", magic_enum::enum_name(result));
    }
    return result;
}

VkResult LogicalDevice::submit_present_queue(const DynamicArray<VkSemaphore>& waitSemaphores, const Swapchain& swapchain, VkResult* result, Void* next) const
{
    VkPresentInfoKHR info{};
    info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.pNext              = next;
    info.waitSemaphoreCount = UInt32(waitSemaphores.size());
    info.pWaitSemaphores    = waitSemaphores.data();
    info.swapchainCount     = 1;
    info.pSwapchains        = &swapchain.get_swapchain();
    info.pImageIndices      = &swapchain.get_image_index();
    info.pResults           = result;

    VkResult presentResult = vkQueuePresentKHR(presentQueue, &info);
    if (presentResult != VK_SUCCESS && presentResult != VK_ERROR_OUT_OF_DATE_KHR && presentResult != VK_SUBOPTIMAL_KHR)
    {
        SPDLOG_ERROR("Submit present queue failed with: {}", magic_enum::enum_name(presentResult));
    }
    return presentResult;
}

VkResult LogicalDevice::submit_present_queue(VkSemaphore waitSemaphore, const Swapchain& swapchain, VkResult* result, Void* next) const
{
    VkPresentInfoKHR info{};
    info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.pNext              = next;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores    = &waitSemaphore;
    info.swapchainCount     = 1;
    info.pSwapchains        = &swapchain.get_swapchain();
    info.pImageIndices      = &swapchain.get_image_index();
    info.pResults           = result;

    VkResult presentResult = vkQueuePresentKHR(presentQueue, &info);
    if (presentResult != VK_SUCCESS && presentResult != VK_ERROR_OUT_OF_DATE_KHR && presentResult != VK_SUBOPTIMAL_KHR)
    {
        SPDLOG_ERROR("Submit present queue failed with: {}", magic_enum::enum_name(presentResult));
    }
    return presentResult;
}

VkResult LogicalDevice::wait_idle() const
{
    return vkDeviceWaitIdle(device);
}

VkResult LogicalDevice::wait_graphics_queue_idle() const
{
    return vkQueueWaitIdle(graphicsQueue);
}

VkResult LogicalDevice::wait_compute_queue_idle() const
{
    return vkQueueWaitIdle(computeQueue);
}

VkResult LogicalDevice::wait_present_queue_idle() const
{
    return vkQueueWaitIdle(presentQueue);
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
