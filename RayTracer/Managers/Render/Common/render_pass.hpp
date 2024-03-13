#pragma once
#include <vulkan/vulkan.h>

struct PhysicalDevice;
class LogicalDevice;
class Swapchain;

class RenderPass
{
public:
	Void create(const PhysicalDevice& physicalDevice, 
				const LogicalDevice& logicalDevice, 
				const Swapchain& swapchain, 
				const VkAllocationCallbacks* allocator);

	const VkRenderPass &get_render_pass() const;

	Void clear(const LogicalDevice& logicalDevice, const VkAllocationCallbacks* allocator);

private:
	VkRenderPass renderPass;

};

