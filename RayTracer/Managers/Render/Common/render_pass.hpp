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
				const VkAllocationCallbacks* allocator);

	Void create_images(const PhysicalDevice& physicalDevice,
					   const LogicalDevice& logicalDevice,
					   const Swapchain& swapchain,
					   const VkAllocationCallbacks* allocator);

	Void create_color_image(const PhysicalDevice& physicalDevice, 
							const LogicalDevice& logicalDevice, 
							const Swapchain& swapchain, 
							const VkAllocationCallbacks* allocator);

	Void create_depth_image(const PhysicalDevice& physicalDevice, 
							const LogicalDevice& logicalDevice, 
							const Swapchain& swapchain, 
							const VkAllocationCallbacks* allocator);

	[[nodiscard]]
	const VkRenderPass &get_render_pass() const;
	[[nodiscard]]
	const DynamicArray<Image>& get_images() const;

	Void clear_images(const LogicalDevice& logicalDevice, const VkAllocationCallbacks* allocator);
	Void clear(const LogicalDevice& logicalDevice, const VkAllocationCallbacks* allocator);

private:
	VkRenderPass renderPass;
	DynamicArray<Image> images;
};

