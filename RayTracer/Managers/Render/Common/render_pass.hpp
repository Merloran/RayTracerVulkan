#pragma once
#include <vulkan/vulkan.hpp>

template<typename Type>
struct Handle;
class Image;
class PhysicalDevice;
class LogicalDevice;
class Swapchain;

class RenderPass
{
public:
	Void create(const PhysicalDevice& physicalDevice,
				const LogicalDevice& logicalDevice,
				const Swapchain& swapchain,
				Handle<Image> colorImageHandle,
				Handle<Image> depthImageHandle,
				const VkAllocationCallbacks* allocator);

	const VkRenderPass &get_render_pass() const;
	const DynamicArray<Handle<Image>> &get_image_order() const;

	Void clear(const LogicalDevice& logicalDevice, const VkAllocationCallbacks* allocator);

private:
	VkRenderPass renderPass;
	DynamicArray<Handle<Image>> imageOrder;
};

