#pragma once
#include <vulkan/vulkan.h>

struct PhysicalDevice;
struct LogicalDevice;

class Swapchain
{
public:
    Void create(const LogicalDevice& logicalDevice,
                const PhysicalDevice& physicalDevice,
                const VkSurfaceKHR& surface,
                const VkAllocationCallbacks* allocator);

    const VkSwapchainKHR& get_swapchain();
    VkFormat get_image_format() const;

    Void clear(const LogicalDevice& device, const VkAllocationCallbacks* allocator);

private:
    VkSwapchainKHR              swapchain;
    DynamicArray<VkImage>       images;
    DynamicArray<VkImageView>   imageViews;
    DynamicArray<VkFramebuffer> framebuffers;
    VkFormat                    imageFormat;
    UVector2                    extent;

    Void create_image_views();
    VkSurfaceFormatKHR choose_swap_surface_format(const DynamicArray<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR choose_swap_present_mode(const DynamicArray<VkPresentModeKHR>& availablePresentModes);
    UVector2 choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities);
};
