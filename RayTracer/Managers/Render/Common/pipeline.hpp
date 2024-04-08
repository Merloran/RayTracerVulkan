#pragma once
#include <vulkan/vulkan.hpp>

class DescriptorPool;
class RenderPass;
class Shader;
class LogicalDevice;

enum class EPipelineType : UInt8
{
	None = 0U,
	Graphics,
	Compute,
	Count = 2U
};

class Pipeline
{
public:
	Void create_graphics_pipeline(const DescriptorPool& descriptorPool,
								  const RenderPass& renderPass,
								  const DynamicArray<Shader>& shaders,
								  const LogicalDevice& logicalDevice,
								  const VkAllocationCallbacks* allocator);

	Void create_simple_pipeline(const DescriptorPool& descriptorPool,
								const RenderPass& renderPass,
								const DynamicArray<Shader>& shaders,
								const LogicalDevice& logicalDevice,
								const VkAllocationCallbacks* allocator);

	Void create_compute_pipeline(const DescriptorPool& descriptorPool, 
								 const Shader& shader,
								 const LogicalDevice& logicalDevice,
								 const VkAllocationCallbacks* allocator);

	Void recreate_pipeline(const DescriptorPool& descriptorPool,
						   const RenderPass& renderPass,
						   const DynamicArray<Shader>& shaders,
						   const LogicalDevice& logicalDevice,
						   const VkAllocationCallbacks* allocator);

	[[nodiscard]]
	EPipelineType get_type() const;
	[[nodiscard]]
	VkPipeline get_pipeline() const;
	[[nodiscard]]
	VkPipelineCache get_cache() const;
	[[nodiscard]]
	VkPipelineLayout get_layout() const;

	Void clear(const LogicalDevice& logicalDevice, const VkAllocationCallbacks* allocator);

private:
	VkPipelineLayout layout;
	VkPipelineCache cache;
	VkPipeline pipeline;
	EPipelineType type;
	Array<VkDynamicState, 2> dynamicStates =
	{
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	Void get_mesh_binding_descriptions(DynamicArray<VkVertexInputBindingDescription>& descriptions);
	Void get_mesh_attribute_descriptions(DynamicArray<VkVertexInputAttributeDescription>& descriptions);
	
	Void create_descriptor_set_layout_info(const DynamicArray<VkDescriptorSetLayout>& descriptorSetLayouts,
										   const DynamicArray<VkPushConstantRange>& pushConstants,
										   const LogicalDevice& logicalDevice,
										   const VkAllocationCallbacks* allocator);
	Bool create_shader_stage_info(const Shader& shader, VkPipelineShaderStageCreateInfo& info);
};
