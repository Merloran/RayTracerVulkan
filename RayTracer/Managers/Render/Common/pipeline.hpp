#pragma once
#include <vulkan/vulkan.h>

class RenderPass;
struct Mesh;
class Shader;
class LogicalDevice;

class Pipeline
{
public:
	Void create_graphics_pipeline(const RenderPass& renderPass,
								  const DynamicArray<Shader>& shaders,
								  const LogicalDevice& logicalDevice,
								  const VkAllocationCallbacks* allocator);
	Void create_compute_pipeline(const Shader& shader,
								 const LogicalDevice& logicalDevice,
								 const VkAllocationCallbacks* allocator);

	[[nodiscard]]
	const VkPipeline& get_pipeline() const;

	Void clear(const LogicalDevice& logicalDevice, const VkAllocationCallbacks* allocator);

private:
	VkPipelineLayout layout;
	VkPipeline pipeline;
	Array<VkDynamicState, 2> dynamicStates =
	{
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	Void get_mesh_binding_descriptions(DynamicArray<VkVertexInputBindingDescription>& descriptions);
	Void get_mesh_attribute_descriptions(DynamicArray<VkVertexInputAttributeDescription>& descriptions);
	
	Void create_descriptor_set_layout_info(const DynamicArray<VkDescriptorSetLayout>& descriptorSetLayout,
										   const LogicalDevice& logicalDevice,
										   const VkAllocationCallbacks* allocator);
	Bool create_shader_stage_info(const Shader& shader, VkPipelineShaderStageCreateInfo& info);
};
