#include "render_pass.hpp"

#include "physical_device.hpp"
#include "logical_device.hpp"
#include "swapchain.hpp"
#include "image.hpp"

#include <magic_enum.hpp>


Void RenderPass::create(const PhysicalDevice& physicalDevice, const LogicalDevice& logicalDevice, const Swapchain& swapchain, const VkAllocationCallbacks* allocator, VkSampleCountFlagBits samples, Bool depthTest, VkAttachmentLoadOp loadOperation)
{
    DynamicArray<VkAttachmentDescription> attachments;
    isDepthTest = depthTest;
    if (samples > physicalDevice.get_max_samples())
    {
        SPDLOG_ERROR("Sample count {} is not supported by physical device.", magic_enum::enum_name(samples));
        this->samples = VK_SAMPLE_COUNT_1_BIT;
        isMultiSampling = false;
    } else {
        isMultiSampling = !(samples & VK_SAMPLE_COUNT_1_BIT);
        this->samples = samples;
    }
    // Swapchain image
    VkAttachmentDescription colorAttachmentResolve{};
    colorAttachmentResolve.format         = swapchain.get_image_format();
    colorAttachmentResolve.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttachmentResolve.loadOp         = loadOperation;
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
    if (isMultiSampling)
    {
	    colorAttachment.format         = swapchain.get_image_format();
	    colorAttachment.samples        = this->samples;
	    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
	    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
	    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
	    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	    colorAttachmentRef.attachment = UInt32(attachments.size());
	    colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachments.push_back(colorAttachment);
        clearValues.emplace_back().color = { {0.0f, 0.0f, 0.0f, 1.0f} };
    }

    VkAttachmentDescription depthAttachment{};
    VkAttachmentReference depthAttachmentRef{};
    if (isDepthTest)
    {
	    depthAttachment.format         = physicalDevice.find_depth_format();
	    depthAttachment.samples        = this->samples;
	    depthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
	    depthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	    depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	    depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
	    depthAttachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        depthAttachmentRef.attachment = UInt32(attachments.size());
	    depthAttachmentRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachments.push_back(depthAttachment);
        clearValues.emplace_back().depthStencil = { 1.0f, 0 };
    }


    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    if (isDepthTest)
    {
        subpass.pDepthStencilAttachment = &depthAttachmentRef;
    }
    if (isMultiSampling)
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
    if (isDepthTest)
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

    const VkResult result = vkCreateRenderPass(logicalDevice.get_device(), &renderPassInfo, allocator, &renderPass);
    if (result != VK_SUCCESS)
    {
        SPDLOG_ERROR("Render pass creation failed with: {}", magic_enum::enum_name(result));
        return;
    }

    create_framebuffers(logicalDevice, swapchain, allocator);
}

Void RenderPass::create_attachments(const PhysicalDevice& physicalDevice, const LogicalDevice& logicalDevice, const Swapchain& swapchain, const VkAllocationCallbacks* allocator)
{
    if (isMultiSampling)
    {
        create_color_attachment(physicalDevice, logicalDevice, swapchain, allocator);
    }
    if (isDepthTest)
    {
        create_depth_attachment(physicalDevice, logicalDevice, swapchain, allocator);
    }
}

Void RenderPass::create_framebuffers(const LogicalDevice& logicalDevice, const Swapchain& swapchain, const VkAllocationCallbacks* allocator)
{
    const DynamicArray<VkImageView>& swapchainViews = swapchain.get_image_views();
    const UVector2& extent = swapchain.get_extent();
    framebuffers.resize(swapchainViews.size());
    DynamicArray<VkImageView> views;
    // 0 index is for swapchain image view
    views.resize(images.size() + 1);

    for (UInt64 i = 0; i < images.size(); ++i)
    {
        views[i + 1] = images[i].get_view();
    }

    for (UInt64 i = 0; i < swapchainViews.size(); ++i)
    {
        views[0] = swapchainViews[i];

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass      = renderPass;
        framebufferInfo.attachmentCount = UInt32(views.size());
        framebufferInfo.pAttachments    = views.data();
        framebufferInfo.width           = extent.x;
        framebufferInfo.height          = extent.y;
        framebufferInfo.layers          = 1;

        const VkResult result = vkCreateFramebuffer(logicalDevice.get_device(), &framebufferInfo, allocator, &framebuffers[i]);
        if (result != VK_SUCCESS)
        {
            SPDLOG_ERROR("Framebuffer {} creation failed with: {}", i, magic_enum::enum_name(result));
        }
    }
}

VkSampleCountFlagBits RenderPass::get_samples() const
{
    return samples;
}

VkRenderPass RenderPass::get_render_pass() const
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

Bool RenderPass::is_multi_sampling_enabled() const
{
    return isMultiSampling;
}

VkFramebuffer RenderPass::get_framebuffer(UInt64 number) const
{
    if (number > framebuffers.size())
    {
        SPDLOG_ERROR("Failed to get framebuffer {} is out of bounds, returned nullptr", number);
        return nullptr;
    }
    return framebuffers[number];
}

Void RenderPass::clear_framebuffers(const LogicalDevice& logicalDevice, const VkAllocationCallbacks* allocator)
{
    for (VkFramebuffer framebuffer : framebuffers)
    {
        vkDestroyFramebuffer(logicalDevice.get_device(), framebuffer, allocator);
    }
}

Void RenderPass::create_depth_attachment(const PhysicalDevice& physicalDevice, const LogicalDevice& logicalDevice, const Swapchain& swapchain, const VkAllocationCallbacks* allocator)
{
    images.emplace_back().create(physicalDevice,
                                 logicalDevice,
                                 swapchain.get_extent(),
                                 1,
                                 samples,
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
                                 samples,
                                 swapchain.get_image_format(),
                                 VK_IMAGE_TILING_OPTIMAL,
                                 VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                 VK_IMAGE_ASPECT_COLOR_BIT,
                                 allocator);
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
    clear_framebuffers(logicalDevice, allocator);

    vkDestroyRenderPass(logicalDevice.get_device(), renderPass, allocator);
}