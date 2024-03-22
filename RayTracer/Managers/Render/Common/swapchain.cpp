#include "../../Display/display_manager.hpp"
#include "../render_manager.hpp"
//TODO: ATTENTION BUG PRONE SHIT!
#include "swapchain.hpp"
#include "logical_device.hpp"
#include "physical_device.hpp"

Void Swapchain::create(const LogicalDevice& logicalDevice, const PhysicalDevice& physicalDevice, const VkSurfaceKHR& surface, const VkAllocationCallbacks* allocator)
{
    const VkSurfaceCapabilitiesKHR& capabilities = physicalDevice.get_capabilities();
    VkSurfaceFormatKHR surfaceFormat = choose_swap_surface_format(physicalDevice.get_formats());
    VkPresentModeKHR   presentMode   = choose_swap_present_mode(physicalDevice.get_present_modes());
    extent                           = choose_swap_extent(capabilities);
    UInt32             imageCount    = capabilities.minImageCount + 1;

    if (capabilities.maxImageCount > 0)
    {
        imageCount = std::min(imageCount, capabilities.maxImageCount);
    }

    VkSwapchainCreateInfoKHR createInfo{};
    Array<UInt32, 2> queueFamilyIndices = 
    {
    	physicalDevice.get_graphics_family_index(),
    	physicalDevice.get_present_family_index()
    };

    createInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface          = surface;
    createInfo.minImageCount    = imageCount;
    createInfo.imageFormat      = surfaceFormat.format;
    createInfo.imageColorSpace  = surfaceFormat.colorSpace;
    createInfo.imageExtent      = VkExtent2D{ extent.x, extent.y };
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (physicalDevice.get_graphics_family_index() != physicalDevice.get_present_family_index())
    {
        createInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = queueFamilyIndices.size();
        createInfo.pQueueFamilyIndices   = queueFamilyIndices.data();
    }
    else {
        createInfo.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0; // Optional
        createInfo.pQueueFamilyIndices   = nullptr; // Optional
    }
    createInfo.preTransform     = capabilities.currentTransform;
    createInfo.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode      = presentMode;
    createInfo.clipped          = VK_TRUE;
    createInfo.oldSwapchain     = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(logicalDevice.get_device(), &createInfo, allocator, &swapchain) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create swap chain!");
    }
    vkGetSwapchainImagesKHR(logicalDevice.get_device(), swapchain, &imageCount, nullptr);
    images.resize(imageCount);
    vkGetSwapchainImagesKHR(logicalDevice.get_device(), swapchain, &imageCount, images.data());
    imageFormat = surfaceFormat.format;
}

const VkSwapchainKHR& Swapchain::get_swapchain()
{
    return swapchain;
}

VkFormat Swapchain::get_image_format() const
{
    return imageFormat;
}

Void Swapchain::create_image_views()
{
    //TODO: move this somewhere its bug prone
    SRenderManager& renderManager = SRenderManager::get();
    imageViews.reserve(imageViews.size());

    for (UInt32 i = 0; i < imageViews.size(); ++i)
    {
        imageViews.emplace_back(renderManager.create_image_view(images[i], imageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1));
    }
}

VkSurfaceFormatKHR Swapchain::choose_swap_surface_format(const DynamicArray<VkSurfaceFormatKHR>& availableFormats)
{
    for (const VkSurfaceFormatKHR& availableFormat : availableFormats)
    { //TODO: change it to RGBA instead of BGRA
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return availableFormat;
        }
    }

    return availableFormats[0];
}

VkPresentModeKHR Swapchain::choose_swap_present_mode(const DynamicArray<VkPresentModeKHR>& availablePresentModes)
{
    for (const VkPresentModeKHR& availablePresentMode : availablePresentModes)
    {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            return availablePresentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

UVector2 Swapchain::choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities)
{
    if (capabilities.currentExtent.width != Limits<UInt32>::max())
    {
        return UVector2{ capabilities.currentExtent.width, capabilities.currentExtent.height };
    } else {
        SDisplayManager& displayManager = SDisplayManager::get();
        const IVector2 size = displayManager.get_framebuffer_size();

        UVector2 actualExtent = static_cast<UVector2>(size);

        actualExtent.x = std::clamp(actualExtent.x,
                                    capabilities.minImageExtent.width,
                                    capabilities.maxImageExtent.width);
        actualExtent.y = std::clamp(actualExtent.y,
                                    capabilities.minImageExtent.height,
                                    capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

Void Swapchain::clear(const LogicalDevice& device, const VkAllocationCallbacks* allocator)
{
    for (VkFramebuffer& framebuffer : framebuffers)
    {
        vkDestroyFramebuffer(device.get_device(), framebuffer, allocator);
    }

    for (VkImageView& imageView : imageViews)
    {
        vkDestroyImageView(device.get_device(), imageView, allocator);
    }

    vkDestroySwapchainKHR(device.get_device(), swapchain, allocator);
}