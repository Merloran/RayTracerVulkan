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
	            UInt32 mipLevels,
	            VkSampleCountFlagBits samplesCount,
	            VkFormat format,
	            VkImageTiling tiling,
	            VkImageUsageFlags usage,
	            VkMemoryPropertyFlags properties,
				const VkAllocationCallbacks* allocator);

	Void create_view(const LogicalDevice& logicalDevice,
					 VkImageAspectFlags aspectFlags,
					 const VkAllocationCallbacks* allocator);

	Void create_sampler(const PhysicalDevice& physicalDevice,
						const LogicalDevice& logicalDevice,
						const VkAllocationCallbacks* allocator);

	VkImage get_image() const;
	VkFormat get_format() const;
	VkImageView get_view() const;
	VkSampler get_sampler() const;
	UInt64 get_mip_levels() const;
	const UVector2& get_size() const;
	VkImageLayout get_current_layout() const;
	Void set_current_layout(VkImageLayout layout);

	static VkImageView s_create_view(const LogicalDevice& logicalDevice,
									 VkImage image,
									 VkFormat format,
									 VkImageAspectFlags aspectFlags,
									 UInt32 mipLevels,
									 const VkAllocationCallbacks* allocator);

	Void clear(const LogicalDevice& logicalDevice, const VkAllocationCallbacks* allocator);

private:
	VkImage image;
	VkDeviceMemory memory;
	VkImageView view = nullptr;
	VkSampler sampler = nullptr;
	UVector2 size;
	VkSampleCountFlagBits samplesCount;
	VkImageLayout currentLayout;
	VkFormat format;
	UInt32 mipLevels;
};

