#pragma once
#include <vulkan/vulkan.hpp>

class Pipeline;
class Swapchain;
class RenderPass;

class CommandBuffer
{
public:
	Void begin(VkCommandBufferUsageFlags flags = 0, 
			   const VkCommandBufferInheritanceInfo* inheritanceInfo = nullptr, 
			   Void* next = nullptr) const;

	Void begin_render_pass(const RenderPass& renderPass, 
						   const Swapchain& swapchain, 
						   UInt64 imageIndex, 
						   VkSubpassContents subpassContents) const;

	Void bind_pipeline(const Pipeline& pipeline) const;

	Void bind_descriptor_set(const Pipeline& pipeline, 
							 VkDescriptorSet set,
							 UInt32 setNumber, 
							 const DynamicArray<UInt32> &dynamicOffsets = DynamicArray<UInt32>()) const;

	template<UInt32 size>
	Void bind_vertex_buffers(UInt32 firstBinding, const Array<VkBuffer, size> &vertexBuffers, const Array<UInt64, size> &offsets) const
	{
		vkCmdBindVertexBuffers(buffer,
							   firstBinding,
							   vertexBuffers.size(),
							   vertexBuffers.data(),
							   offsets.data());
	}

	Void bind_index_buffer(VkBuffer indexBuffer, UInt64 offset, VkIndexType type) const;

	Void draw_indexed(UInt32 indexCount, UInt32 instanceCount, UInt32 firstIndex, Int32 vertexOffset, UInt32 firstInstance) const;

	Void dispatch(const UVector3 &groupCount) const;

	Void set_constants(const Pipeline& pipeline, VkShaderStageFlags stageFlags, UInt32 offset, UInt32 size, Void* data) const;

	Void set_viewports(UInt32 firstViewport, const DynamicArray<VkViewport> &viewports) const;
	Void set_viewport(UInt32 firstViewport, 
					  const FVector2 &position, 
					  const FVector2 &size, 
					  const FVector2 &depthBounds) const;

	Void set_scissors(UInt32 firstScissor, const DynamicArray<VkRect2D> &scissors) const;
	Void set_scissor(UInt32 firstScissor, const IVector2 &position, const UVector2 &size) const;

	Void end_render_pass() const;

	Void end() const;

	Void reset(VkCommandBufferResetFlags flags) const;

	[[nodiscard]]
	const VkCommandBuffer &get_buffer() const;
	[[nodiscard]]
	const String &get_name() const;

	Void set_name(const String& name);
	Void set_buffer(VkCommandBuffer buffer);

private:
	VkCommandBuffer buffer;
	String name;

};
