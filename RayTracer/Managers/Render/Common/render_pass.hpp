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
				const VkAllocationCallbacks* allocator,
				VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT,
				Bool depthTest = true);

	Void create_attachments(const PhysicalDevice& physicalDevice,
							const LogicalDevice& logicalDevice,
							const Swapchain& swapchain,
							const VkAllocationCallbacks* allocator);

	[[nodiscard]]
	VkSampleCountFlagBits get_samples() const;
	[[nodiscard]]
	VkRenderPass get_render_pass() const;
	[[nodiscard]]
	const DynamicArray<Image>& get_images() const;
	[[nodiscard]]
	const DynamicArray<VkClearValue>& get_clear_values() const;
	[[nodiscard]]
	Bool is_depth_test_enabled() const;
	[[nodiscard]]
	Bool is_multi_sampling_enabled() const;

	Void clear_images(const LogicalDevice& logicalDevice, const VkAllocationCallbacks* allocator);
	Void clear(const LogicalDevice& logicalDevice, const VkAllocationCallbacks* allocator);

private:
	VkRenderPass renderPass;
	DynamicArray<Image> images;
	VkSampleCountFlagBits samples;
	Bool isDepthTest, isMultiSampling;
	DynamicArray<VkClearValue> clearValues;

	Void create_depth_attachment(const PhysicalDevice& physicalDevice,
								 const LogicalDevice& logicalDevice,
								 const Swapchain& swapchain,
								 const VkAllocationCallbacks* allocator);
	Void create_color_attachment(const PhysicalDevice& physicalDevice,
								 const LogicalDevice& logicalDevice,
								 const Swapchain& swapchain,
								 const VkAllocationCallbacks* allocator);
};

