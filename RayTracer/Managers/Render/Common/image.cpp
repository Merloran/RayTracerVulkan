#include "image.hpp"

#include "physical_device.hpp"
#include "logical_device.hpp"

Void Image::create(const PhysicalDevice& physicalDevice, const LogicalDevice& logicalDevice, const UVector2& size, UInt32 mipLevel, VkSampleCountFlagBits samplesCount, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImageAspectFlags aspectFlags, const VkAllocationCallbacks* allocator)
{
    if (!(physicalDevice.get_format_properties(format).optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
    {
        SPDLOG_ERROR("Texture iamge format not support linear blitting, mip levels set to 1");
        this->mipLevel = 1;
    } else {
        this->mipLevel = mipLevel;
    }

    this->format       = format;
    this->samplesCount = samplesCount;
    this->size         = size;
    this->usage        = usage;
    this->tiling       = tiling;
    this->properties   = properties;
    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width  = size.x;
    imageInfo.extent.height = size.y;
    imageInfo.extent.depth  = 1;
    imageInfo.mipLevels     = this->mipLevel;
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

    create_view(logicalDevice, aspectFlags, allocator);
}

Void Image::create_view(const LogicalDevice& logicalDevice, VkImageAspectFlags aspectFlags, const VkAllocationCallbacks* allocator)
{
    this->aspectFlags = aspectFlags;
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                           = image;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                          = format;
    viewInfo.subresourceRange.aspectMask     = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel   = 0; //TODO: think of it
    viewInfo.subresourceRange.levelCount     = mipLevel;
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
    samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.pNext                   = nullptr;
    samplerInfo.flags                   = 0;
    samplerInfo.magFilter               = VK_FILTER_LINEAR;
    samplerInfo.minFilter               = VK_FILTER_LINEAR;
    if (format == VK_FORMAT_R8G8B8A8_UNORM || format == VK_FORMAT_R8G8B8A8_SRGB)
    {
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    } else {
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    }
    samplerInfo.anisotropyEnable        = VK_TRUE;
    samplerInfo.maxAnisotropy           = physicalDevice.get_properties().limits.maxSamplerAnisotropy;
    samplerInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable           = VK_FALSE;
    samplerInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias              = 0.0f;
    samplerInfo.minLod                  = 0.0f;
    samplerInfo.maxLod                  = Float32(mipLevel);

    if (vkCreateSampler(logicalDevice.get_device(), &samplerInfo, allocator, &sampler) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create sampler!");
    }
}

Void Image::resize(const PhysicalDevice& physicalDevice, const LogicalDevice& logicalDevice, const UVector2& size, const VkAllocationCallbacks* allocator)
{
    // To prevent clearing sampler to reuse it
	const VkSampler holder = sampler;
    sampler = nullptr;
    clear(logicalDevice, allocator);
    create(physicalDevice, logicalDevice, size, mipLevel, samplesCount, format, tiling, usage, properties, aspectFlags, allocator);
    sampler = holder;
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

UInt32 Image::get_mip_level() const
{
    return mipLevel;
}

const UVector2& Image::get_size() const
{
    return size;
}

VkImageLayout Image::get_current_layout() const
{
    return currentLayout;
}

VkImageAspectFlags Image::get_aspect_flags() const
{
    return aspectFlags;
}

Void Image::set_current_layout(VkImageLayout layout)
{
    currentLayout = layout;
}

VkImageView Image::s_create_view(const LogicalDevice& logicalDevice, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, UInt32 mipLevel, const VkAllocationCallbacks* allocator)
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                           = image;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                          = format;
    viewInfo.subresourceRange.aspectMask     = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = mipLevel;
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
    vkDestroyImageView(logicalDevice.get_device(), view, allocator);
    vkDestroyImage(logicalDevice.get_device(), image, allocator);
    vkFreeMemory(logicalDevice.get_device(), memory, allocator);
}

