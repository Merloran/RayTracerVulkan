#include "image.hpp"

#include "physical_device.hpp"
#include "logical_device.hpp"

Void Image::create(const PhysicalDevice& physicalDevice, const LogicalDevice& logicalDevice, const UVector2& size, UInt32 mipLevels, VkSampleCountFlagBits samplesCount, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, const VkAllocationCallbacks* allocator)
{
    this->mipLevels    = mipLevels;
    this->format       = format;
    this->samplesCount = samplesCount;
    this->size         = size;
    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width  = size.x;
    imageInfo.extent.height = size.y;
    imageInfo.extent.depth  = 1;
    imageInfo.mipLevels     = mipLevels;
    imageInfo.arrayLayers   = 1;
    imageInfo.format        = format;
    imageInfo.tiling        = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage         = usage;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples       = samplesCount;
    imageInfo.flags         = 0; // Optional
    currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(logicalDevice.get_device(), &imageInfo, allocator, &image) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(logicalDevice.get_device(), image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memRequirements.size;
    allocInfo.memoryTypeIndex = physicalDevice.find_memory_type(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(logicalDevice.get_device(), &allocInfo, allocator, &memory) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate image memory!");
    }

    vkBindImageMemory(logicalDevice.get_device(), image, memory, 0);
}

Void Image::create_view(const LogicalDevice& logicalDevice, VkImageAspectFlags aspectFlags, const VkAllocationCallbacks* allocator)
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                           = image;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                          = format;
    viewInfo.subresourceRange.aspectMask     = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;
    
    if (vkCreateImageView(logicalDevice.get_device(), &viewInfo, allocator, &view) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create image view!");
    }
}

Void Image::create_sampler(const PhysicalDevice& physicalDevice, const LogicalDevice& logicalDevice, const VkAllocationCallbacks* allocator)
{
    VkSamplerCreateInfo samplerInfo{};
    const VkPhysicalDeviceProperties &properties = physicalDevice.get_properties();

    samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter               = VK_FILTER_LINEAR;
    samplerInfo.minFilter               = VK_FILTER_LINEAR;
    samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable        = VK_TRUE;
    samplerInfo.maxAnisotropy           = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable           = VK_FALSE;
    samplerInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias              = 0.0f;
    samplerInfo.minLod                  = 0.0f;
    samplerInfo.maxLod                  = Float32(mipLevels);

    if (vkCreateSampler(logicalDevice.get_device(), &samplerInfo, allocator, &sampler) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create sampler!");
    }
}

VkImage Image::get_image() const
{
    return image;
}

VkFormat Image::get_format() const
{
    return format;
}

VkImageView Image::get_view() const
{
    return view;
}

VkSampler Image::get_sampler() const
{
    return sampler;
}

UInt64 Image::get_mip_levels() const
{
    return mipLevels;
}

const UVector2& Image::get_size() const
{
    return size;
}

VkImageLayout Image::get_current_layout() const
{
    return currentLayout;
}

Void Image::set_current_layout(VkImageLayout layout)
{
    currentLayout = layout;
}

VkImageView Image::s_create_view(const LogicalDevice& logicalDevice, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, UInt32 mipLevels, const VkAllocationCallbacks* allocator)
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                           = image;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                          = format;
    viewInfo.subresourceRange.aspectMask     = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;
    VkImageView view;
    if (vkCreateImageView(logicalDevice.get_device(), &viewInfo, allocator, &view) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create image view!");
    }

    return view;
}

Void Image::clear(const LogicalDevice& logicalDevice, const VkAllocationCallbacks* allocator)
{
    if (sampler != nullptr)
    {
        vkDestroySampler(logicalDevice.get_device(), sampler, allocator);
    }
    if (view != nullptr)
    {
        vkDestroyImageView(logicalDevice.get_device(), view, allocator);
    }
    vkDestroyImage(logicalDevice.get_device(), image, allocator);
    vkFreeMemory(logicalDevice.get_device(), memory, allocator);
}

