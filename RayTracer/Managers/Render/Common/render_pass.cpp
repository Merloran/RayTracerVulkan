#include "render_pass.hpp"

#include "physical_device.hpp"
#include "logical_device.hpp"
#include "swapchain.hpp"
#include "image.hpp"


Void RenderPass::create(const PhysicalDevice& physicalDevice, const LogicalDevice& logicalDevice, const Swapchain& swapchain, const VkAllocationCallbacks* allocator, Bool depthTest)
{
    DynamicArray<VkAttachmentDescription> attachments;
    isDepthTest = depthTest;
    Bool multiSampling = logicalDevice.is_multi_sampling_enabled();
    // Swapchain image
    VkAttachmentDescription colorAttachmentResolve{};
    colorAttachmentResolve.format         = swapchain.get_image_format();
    colorAttachmentResolve.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttachmentResolve.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR; //VK_ATTACHMENT_LOAD_OP_DONT_CARE
    colorAttachmentResolve.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentResolve.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachmentResolve.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachmentResolve.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentResolveRef{};
    colorAttachmentResolveRef.attachment = UInt32(attachments.size());
    colorAttachmentResolveRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments.push_back(colorAttachmentResolve);
    clearValues.emplace_back().color = { {0.0f, 0.0f, 0.0f, 1.0f} };
    
    VkAttachmentDescription colorAttachment{};
    VkAttachmentReference colorAttachmentRef{};
    if (multiSampling)
    {
	    colorAttachment.format         = swapchain.get_image_format();
	    colorAttachment.samples        = logicalDevice.get_samples();
	    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
	    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
	    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
	    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	    colorAttachmentRef.attachment = UInt32(attachments.size());
	    colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachments.push_back(colorAttachment);
    }

    VkAttachmentDescription depthAttachment{};
    VkAttachmentReference depthAttachmentRef{};
    if (depthTest)
    {
	    depthAttachment.format         = physicalDevice.find_depth_format();
	    depthAttachment.samples        = logicalDevice.get_samples();
	    depthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
	    depthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	    depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	    depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
	    depthAttachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        depthAttachmentRef.attachment = UInt32(attachments.size());
	    depthAttachmentRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachments.push_back(depthAttachment);
    }


    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    if (depthTest)
    {
        subpass.pDepthStencilAttachment = &depthAttachmentRef;
    }
    if (multiSampling)
    {
        subpass.pColorAttachments   = &colorAttachmentRef;
        subpass.pResolveAttachments = &colorAttachmentResolveRef;
    } else {
        subpass.pColorAttachments = &colorAttachmentResolveRef;
    }

    VkSubpassDependency dependency{};
    dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass    = 0;
    dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    if (depthTest)
    {
        dependency.srcStageMask  |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstStageMask  |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }
    
    // Creation order of these images must match attachments order
    create_attachments(physicalDevice, logicalDevice, swapchain, nullptr);
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

Void RenderPass::create_attachments(const PhysicalDevice& physicalDevice, const LogicalDevice& logicalDevice, const Swapchain& swapchain, const VkAllocationCallbacks* allocator)
{
    if (logicalDevice.is_multi_sampling_enabled())
    {
        create_color_attachment(physicalDevice, logicalDevice, swapchain, allocator);
        clearValues.emplace_back().color = { {0.0f, 0.0f, 0.0f, 1.0f} };
    }
    if (isDepthTest)
    {
        create_depth_attachment(physicalDevice, logicalDevice, swapchain, allocator);
        clearValues.emplace_back().depthStencil = { 1.0f, 0 };
    }
}

const VkRenderPass& RenderPass::get_render_pass() const
{
    return renderPass;
}

const DynamicArray<Image>& RenderPass::get_images() const
{
    return images;
}

const DynamicArray<VkClearValue>& RenderPass::get_clear_values() const
{
    return clearValues;
}

Bool RenderPass::is_depth_test_enabled() const
{
    return isDepthTest;
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

Void RenderPass::create_depth_attachment(const PhysicalDevice& physicalDevice, const LogicalDevice& logicalDevice, const Swapchain& swapchain, const VkAllocationCallbacks* allocator)
{
    images.emplace_back().create(physicalDevice,
                                 logicalDevice,
                                 swapchain.get_extent(),
                                 1,
                                 logicalDevice.get_samples(),
                                 physicalDevice.find_depth_format(),
                                 VK_IMAGE_TILING_OPTIMAL,
                                 VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                 VK_IMAGE_ASPECT_DEPTH_BIT,
                                 nullptr);
}

Void RenderPass::create_color_attachment(const PhysicalDevice& physicalDevice, const LogicalDevice& logicalDevice, const Swapchain& swapchain, const VkAllocationCallbacks* allocator)
{
    images.emplace_back().create(physicalDevice,
                                 logicalDevice,
                                 swapchain.get_extent(),
                                 1,
                                 logicalDevice.get_samples(),
                                 swapchain.get_image_format(),
                                 VK_IMAGE_TILING_OPTIMAL,
                                 VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                 VK_IMAGE_ASPECT_COLOR_BIT,
                                 allocator);
}
