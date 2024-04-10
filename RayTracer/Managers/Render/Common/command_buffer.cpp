#include "command_buffer.hpp"

#include <magic_enum.hpp>

#include "pipeline.hpp"
#include "render_pass.hpp"
#include "swapchain.hpp"

Void CommandBuffer::begin(VkCommandBufferUsageFlags flags, const VkCommandBufferInheritanceInfo* inheritanceInfo, Void* next) const
{
    VkCommandBufferBeginInfo beginInfo;
    beginInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.pNext            = next;
    beginInfo.flags            = flags;
    beginInfo.pInheritanceInfo = inheritanceInfo;

    const VkResult result = vkBeginCommandBuffer(buffer, &beginInfo);
    if (result != VK_SUCCESS)
    {
        SPDLOG_ERROR("Command buffer: {}, begin failed with result: {}.", name, magic_enum::enum_name(result));
    }
}

Void CommandBuffer::begin_render_pass(const RenderPass& renderPass, const Swapchain& swapchain, UInt64 imageIndex, VkSubpassContents subpassContents) const
{
    VkRenderPassBeginInfo renderPassInfo{};
    const DynamicArray<VkClearValue>& clearValues = renderPass.get_clear_values();
    const UVector2& extent = swapchain.get_extent();
    renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass        = renderPass.get_render_pass();
    renderPassInfo.framebuffer       = swapchain.get_framebuffer(imageIndex);
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = VkExtent2D{ extent.x, extent.y };
    renderPassInfo.clearValueCount   = UInt32(clearValues.size());
    renderPassInfo.pClearValues      = clearValues.data();

    vkCmdBeginRenderPass(buffer, &renderPassInfo, subpassContents);
}

Void CommandBuffer::bind_pipeline(const Pipeline& pipeline) const
{
    vkCmdBindPipeline(buffer, pipeline.get_bind_point(), pipeline.get_pipeline());
}

Void CommandBuffer::bind_descriptor_set(const Pipeline& pipeline, VkDescriptorSet set, UInt32 setNumber, const DynamicArray<UInt32> &dynamicOffsets) const
{
    vkCmdBindDescriptorSets(buffer,
                            pipeline.get_bind_point(),
                            pipeline.get_layout(),
                            setNumber,
                            1,
                            &set,
                            UInt32(dynamicOffsets.size()),
                            dynamicOffsets.data());
}

Void CommandBuffer::bind_index_buffer(VkBuffer indexBuffer, UInt64 offset, VkIndexType type) const
{
    vkCmdBindIndexBuffer(buffer, indexBuffer, offset, type);
}

Void CommandBuffer::draw_indexed(UInt32 indexCount, UInt32 instanceCount, UInt32 firstIndex, Int32 vertexOffset, UInt32 firstInstance) const
{
    vkCmdDrawIndexed(buffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

Void CommandBuffer::set_constants(const Pipeline& pipeline, VkShaderStageFlags stageFlags, UInt32 offset, UInt32 size, Void* data) const
{
    vkCmdPushConstants(buffer, pipeline.get_layout(), stageFlags, offset, size, data);
}

Void CommandBuffer::set_viewports(UInt32 firstViewport, const DynamicArray<VkViewport>& viewports) const
{
    vkCmdSetViewport(buffer, firstViewport, UInt32(viewports.size()), viewports.data());
}

Void CommandBuffer::set_viewport(UInt32 firstViewport, const FVector2& position, const FVector2& size, const FVector2& depthBounds) const
{
    VkViewport viewport;
    viewport.x        = position.x;
    viewport.y        = position.y;
    viewport.width    = size.x;
    viewport.height   = size.y;
    viewport.minDepth = depthBounds.x;
    viewport.maxDepth = depthBounds.y;
    vkCmdSetViewport(buffer, firstViewport, 1, &viewport);
}

Void CommandBuffer::set_scissors(UInt32 firstScissor, const DynamicArray<VkRect2D>& scissors) const
{
    vkCmdSetScissor(buffer, firstScissor, UInt32(scissors.size()), scissors.data());
}

Void CommandBuffer::set_scissor(UInt32 firstScissor, const IVector2& position, const UVector2& size) const
{
    VkRect2D scissor;
    scissor.offset.x      = position.x;
    scissor.offset.y      = position.y;
    scissor.extent.width  = size.x;
    scissor.extent.height = size.y;
    vkCmdSetScissor(buffer, firstScissor, 1, &scissor);
}

Void CommandBuffer::end_render_pass() const
{
    vkCmdEndRenderPass(buffer);
}

Void CommandBuffer::end() const
{
    const VkResult result = vkEndCommandBuffer(buffer);
    if (result != VK_SUCCESS)
    {
        SPDLOG_ERROR("Command buffer: {}, end failed with result: {}.", name, magic_enum::enum_name(result));
    }
}

Void CommandBuffer::reset(VkCommandBufferResetFlags flags) const
{
	vkResetCommandBuffer(buffer, flags);
}

const VkCommandBuffer& CommandBuffer::get_buffer() const
{
	return buffer;
}

const String& CommandBuffer::get_name() const
{
    return name;
}

Void CommandBuffer::set_name(const String& name)
{
    this->name = name;
}

Void CommandBuffer::set_buffer(VkCommandBuffer buffer)
{
    this->buffer = buffer;
}
