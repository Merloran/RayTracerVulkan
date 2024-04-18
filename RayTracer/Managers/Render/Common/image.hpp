#pragma once
#include <vulkan/vulkan.hpp>

class PhysicalDevice;
class LogicalDevice;

class Image
{
public:
    Void create(const PhysicalDevice& physicalDevice,
				const LogicalDevice& logicalDevice,
				const UVector2& size,
	            UInt32 mipLevel,
	            VkSampleCountFlagBits samplesCount,
	            VkFormat format,
	            VkImageTiling tiling,
	            VkImageUsageFlags usage,
	            VkMemoryPropertyFlags properties,
				VkImageAspectFlags aspectFlags,
				const VkAllocationCallbacks* allocator);

	Void create_sampler(const PhysicalDevice& physicalDevice,
						const LogicalDevice& logicalDevice,
						const VkAllocationCallbacks* allocator);

	Void resize(const PhysicalDevice& physicalDevice,
				const LogicalDevice& logicalDevice,
				const UVector2& size,
				const VkAllocationCallbacks* allocator);

	VkImage get_image() const;
	VkFormat get_format() const;
	VkImageView get_view() const;
	VkSampler get_sampler() const;
	UInt32 get_mip_level() const;
	const UVector2& get_size() const;
	VkImageLayout get_current_layout() const;
	VkImageAspectFlags get_aspect_flags() const;
	Void set_current_layout(VkImageLayout layout);

	static VkImageView s_create_view(const LogicalDevice& logicalDevice,
									 VkImage image,
									 VkFormat format,
									 VkImageAspectFlags aspectFlags,
									 UInt32 mipLevel,
									 const VkAllocationCallbacks* allocator);

	Void clear(const LogicalDevice& logicalDevice, const VkAllocationCallbacks* allocator);

private:
	VkImage image;
	VkDeviceMemory memory;
	VkImageView view;
	VkSampler sampler = nullptr;
	UVector2 size;
	VkFormat format;
	VkImageTiling tiling;
	VkImageUsageFlags usage;
	VkImageLayout currentLayout;
	VkImageAspectFlags aspectFlags;
	VkMemoryPropertyFlags properties;
	VkSampleCountFlagBits samplesCount;
	UInt32 mipLevel;

	Void create_view(const LogicalDevice& logicalDevice,
					 VkImageAspectFlags aspectFlags,
					 const VkAllocationCallbacks* allocator);
};

