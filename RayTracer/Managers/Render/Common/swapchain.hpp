#pragma once
#include <vulkan/vulkan.hpp>

class RenderPass;
class PhysicalDevice;
class LogicalDevice;

class Swapchain
{
public:
    Void create(const LogicalDevice& logicalDevice,
                const PhysicalDevice& physicalDevice,
                const VkSurfaceKHR& surface,
                const VkAllocationCallbacks* allocator);

    Void create_framebuffers(const LogicalDevice& logicalDevice,
							 const RenderPass& renderPass,
							 const VkAllocationCallbacks* allocator);

    VkFramebuffer get_framebuffer(UInt64 number) const;
    const VkSwapchainKHR &get_swapchain() const;
    VkFormat get_image_format() const;
    const UVector2 &get_extent() const;
    const DynamicArray<VkImageView> &get_image_views() const;
    const UInt32 &get_image_index() const;
    Void set_image_index(UInt32 imageIndex);

    Void clear(const LogicalDevice& device, const VkAllocationCallbacks* allocator);

private:
    VkSwapchainKHR              swapchain;
    DynamicArray<VkImage>       images;
    DynamicArray<VkImageView>   imageViews;
    DynamicArray<VkFramebuffer> framebuffers;
    VkFormat                    imageFormat;
    UVector2                    extent;
    UInt32                      imageIndex;

    Void create_image_views(const LogicalDevice& logicalDevice, const VkAllocationCallbacks* allocator);

    VkSurfaceFormatKHR choose_swap_surface_format(const DynamicArray<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR choose_swap_present_mode(const DynamicArray<VkPresentModeKHR>& availablePresentModes);
    UVector2 choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities);
};
