#pragma once
#include <vulkan/vulkan.hpp>

class Swapchain;
class DebugMessenger;
class PhysicalDevice;

class LogicalDevice
{
public:
    Void create(const PhysicalDevice& physicalDevice, 
                const DebugMessenger& debugMessenger,
                const VkAllocationCallbacks* allocator);

    VkResult acquire_next_image(Swapchain& swapchain, 
                                VkSemaphore semaphore, 
                                VkFence fence = VK_NULL_HANDLE, 
                                UInt64 timeout = Limits<UInt64>::max()) const;

    VkResult wait_for_fences(DynamicArray<VkFence> fences, Bool waitAll, UInt64 timeout = Limits<UInt64>::max()) const;
    VkResult wait_for_fence(VkFence fence, Bool waitAll, UInt64 timeout = Limits<UInt64>::max()) const;

    VkResult reset_fences(DynamicArray<VkFence> fences) const;
    VkResult reset_fence(VkFence fence) const;

    VkResult submit_graphics_queue(const DynamicArray<VkSubmitInfo> &infos, VkFence fence) const;
    VkResult submit_graphics_queue(const DynamicArray<VkSemaphore> &waitSemaphores, 
                                   const DynamicArray<VkPipelineStageFlags> &waitStages, 
                                   const DynamicArray<VkCommandBuffer> &commandBuffers, 
                                   const DynamicArray<VkSemaphore> &signalSemaphores, 
                                   VkFence fence, 
                                   Void* next = nullptr) const;
    VkResult submit_graphics_queue(VkSemaphore waitSemaphore,
                                   VkPipelineStageFlags waitStage,
                                   VkCommandBuffer commandBuffer,
                                   VkSemaphore signalSemaphore,
                                   VkFence fence,
                                   Void* next = nullptr) const;

    VkResult submit_compute_queue(const DynamicArray<VkSubmitInfo>& infos, VkFence fence) const;
    VkResult submit_compute_queue(const DynamicArray<VkSemaphore>& waitSemaphores,
                                  const DynamicArray<VkPipelineStageFlags>& waitStages,
                                  const DynamicArray<VkCommandBuffer>& commandBuffers,
                                  const DynamicArray<VkSemaphore>& signalSemaphores,
                                  VkFence fence,
                                  Void* next = nullptr) const;
    VkResult submit_compute_queue(VkSemaphore waitSemaphore,
                                  VkPipelineStageFlags waitStage,
                                  VkCommandBuffer commandBuffer,
                                  VkSemaphore signalSemaphore,
                                  VkFence fence,
                                  Void* next = nullptr) const;

    VkResult submit_present_queue(const DynamicArray<VkSemaphore> &waitSemaphores,
                                  const DynamicArray<Swapchain> &swapchains, 
                                  VkResult *results = nullptr,
                                  Void *next = nullptr) const;
    VkResult submit_present_queue(const DynamicArray<VkSemaphore> &waitSemaphores,
                                  const Swapchain &swapchain, 
                                  VkResult *result = nullptr,
                                  Void *next = nullptr) const;
    VkResult submit_present_queue(VkSemaphore waitSemaphore,
                                  const Swapchain &swapchain, 
                                  VkResult *result = nullptr,
                                  Void *next = nullptr) const;
    
    VkResult wait_idle() const;
    VkResult wait_graphics_queue_idle() const;
    VkResult wait_compute_queue_idle() const;
    VkResult wait_present_queue_idle() const;

    VkDevice get_device() const;
    VkQueue get_graphics_queue() const;
    VkQueue get_compute_queue() const;
    VkQueue get_present_queue() const;

    Void clear(const VkAllocationCallbacks* allocator);

private:
	VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    VkQueue computeQueue;
};
