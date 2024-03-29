#include "render_pass.hpp"

#include "physical_device.hpp"
#include "logical_device.hpp"
#include "swapchain.hpp"
#include "image.hpp"


Void RenderPass::create(const PhysicalDevice& physicalDevice, const LogicalDevice& logicalDevice, const Swapchain& swapchain, const VkAllocationCallbacks* allocator)
{
    // Swapchain image
    VkAttachmentDescription colorAttachmentResolve{};
    colorAttachmentResolve.format         = swapchain.get_image_format();
    colorAttachmentResolve.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttachmentResolve.loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentResolve.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachmentResolve.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachmentResolve.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentResolveRef{};
    colorAttachmentResolveRef.attachment = 0;
    colorAttachmentResolveRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;


    VkAttachmentDescription colorAttachment{};
    colorAttachment.format         = swapchain.get_image_format();
    colorAttachment.samples        = logicalDevice.get_samples();
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 1;
    colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;


    VkAttachmentDescription depthAttachment{};
    depthAttachment.format         = physicalDevice.find_depth_format();
    depthAttachment.samples        = logicalDevice.get_samples();
    depthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 2;
    depthAttachmentRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;


    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;
    subpass.pResolveAttachments     = &colorAttachmentResolveRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass    = 0;
    dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    // Creation order of these images must match attachments order
    create_images(physicalDevice, logicalDevice, swapchain, allocator);
    Array<VkAttachmentDescription, 3> attachments = { colorAttachmentResolve, colorAttachment, depthAttachment };
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = UInt32(attachments.size());
    renderPassInfo.pAttachments    = attachments.data();
    renderPassInfo.subpassCount    = 1;
    renderPassInfo.pSubpasses      = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies   = &dependency;

    if (vkCreateRenderPass(logicalDevice.get_device(), &renderPassInfo, allocator, &renderPass) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create render pass!");
    }
}

Void RenderPass::create_images(const PhysicalDevice& physicalDevice, const LogicalDevice& logicalDevice, const Swapchain& swapchain, const VkAllocationCallbacks* allocator)
{
    create_color_image(physicalDevice, logicalDevice, swapchain, allocator);
    create_depth_image(physicalDevice, logicalDevice, swapchain, allocator);
}

Void RenderPass::create_color_image(const PhysicalDevice& physicalDevice, const LogicalDevice& logicalDevice, const Swapchain& swapchain, const VkAllocationCallbacks* allocator)
{
    Image& image = images.emplace_back();

    image.create(physicalDevice,
                 logicalDevice,
                 swapchain.get_extent(),
                 1,
                 logicalDevice.get_samples(),
                 swapchain.get_image_format(),
                 VK_IMAGE_TILING_OPTIMAL,
                 VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 allocator);
    image.create_view(logicalDevice, VK_IMAGE_ASPECT_COLOR_BIT, allocator);
}

Void RenderPass::create_depth_image(const PhysicalDevice& physicalDevice, const LogicalDevice& logicalDevice, const Swapchain& swapchain, const VkAllocationCallbacks* allocator)
{
    Image& image = images.emplace_back();

    image.create(physicalDevice,
                 logicalDevice,
                 swapchain.get_extent(),
                 1,
                 logicalDevice.get_samples(),
                 physicalDevice.find_depth_format(),
                 VK_IMAGE_TILING_OPTIMAL,
                 VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 nullptr);
    image.create_view(logicalDevice, VK_IMAGE_ASPECT_DEPTH_BIT, nullptr);
}

const VkRenderPass& RenderPass::get_render_pass() const
{
    return renderPass;
}

const DynamicArray<Image>& RenderPass::get_images() const
{
    return images;
}

Void RenderPass::clear_images(const LogicalDevice& logicalDevice, const VkAllocationCallbacks* allocator)
{
    for (Image& image : images)
    {
        image.clear(logicalDevice, allocator);
    }
    images.clear();
}

Void RenderPass::clear(const LogicalDevice& logicalDevice, const VkAllocationCallbacks* allocator)
{
    clear_images(logicalDevice, allocator);

    vkDestroyRenderPass(logicalDevice.get_device(), renderPass, allocator);
}
