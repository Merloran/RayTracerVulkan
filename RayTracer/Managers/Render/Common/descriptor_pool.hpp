#pragma once
#include "../../Resource/Common/handle.hpp"
#include <vulkan/vulkan.hpp>

class LogicalDevice;


struct DescriptorLayoutData
{
	DynamicArray<VkDescriptorSetLayoutBinding> bindings;
	DynamicArray<VkDescriptorBindingFlags> bindingFlags;
	VkDescriptorSetLayoutCreateFlags layoutFlags;
	String name;
	VkDescriptorSetLayout layout;
};

struct DescriptorResourceInfo
{
	DynamicArray<VkDescriptorImageInfo> imageInfos;
	DynamicArray<VkDescriptorBufferInfo> bufferInfos;
	DynamicArray<VkBufferView> texelBufferViews;
};

struct DescriptorSetData
{
	DynamicArray<DescriptorResourceInfo> resources; //TODO: think of remove it
	DynamicArray<VkWriteDescriptorSet> writes;
	String name;
	Handle<DescriptorLayoutData> layoutHandle;
	VkDescriptorSet set;
};

class DescriptorPool
{
public:
	// Recommended to add first binding with highest set number
	Void add_binding(const String& layoutName, 
					 UInt32 setNumber,
					 UInt32 binding,
					 VkDescriptorType descriptorType,
					 UInt32 descriptorCount,
					 VkShaderStageFlags stageFlags,
					 VkDescriptorBindingFlags bindingFlags = 0,
					 VkDescriptorSetLayoutCreateFlags layoutFlags = 0,
					 VkDescriptorPoolCreateFlags poolFlags = 0);

	Void create_layouts(const LogicalDevice& logicalDevice, const VkAllocationCallbacks* allocator);

	Handle<DescriptorSetData> add_set(Handle<DescriptorLayoutData> layoutHandle,
									  const DynamicArray<DescriptorResourceInfo>& resources, 
									  const String &name);

	Void create_sets(const LogicalDevice& logicalDevice, const VkAllocationCallbacks* allocator);
	Void update_set(const LogicalDevice& logicalDevice, 
					const DescriptorResourceInfo& data, 
					Handle<DescriptorSetData> handle, 
					UInt32 arrayElement, 
					UInt64 binding);

	Void set_push_constants(const DynamicArray<VkPushConstantRange> &pushConstants);

	[[nodiscard]]
	Handle<DescriptorLayoutData> get_layout_data_handle_by_name(const String& name) const;
	DescriptorLayoutData& get_layout_data_by_name(const String& name);
	DescriptorLayoutData& get_layout_data_by_handle(const Handle<DescriptorLayoutData> handle);

	[[nodiscard]]
	Handle<DescriptorSetData> get_set_data_handle_by_name(const String& name)  const;
	DescriptorSetData& get_set_data_by_name(const String& name);
	DescriptorSetData& get_set_data_by_handle(const Handle<DescriptorSetData> handle);

	[[nodiscard]]
	const DynamicArray<VkPushConstantRange>& get_push_constants() const;
	DynamicArray<VkDescriptorSetLayout> get_layouts() const;

	Bool are_resources_compatible(const DescriptorLayoutData& layout, const DynamicArray<DescriptorResourceInfo>& resources);

	Void clear(const LogicalDevice& logicalDevice, const VkAllocationCallbacks* allocator);

private:
	VkDescriptorPool pool = VK_NULL_HANDLE;
	VkDescriptorPoolCreateFlags poolFlags = 0;
	DynamicArray<VkDescriptorPoolSize> sizes;

	HashMap<String, Handle<DescriptorLayoutData>> nameToIdLayoutData;
	DynamicArray<DescriptorLayoutData> layoutData;
	DynamicArray<VkPushConstantRange> pushConstants;
	VkDescriptorSetLayout empty;

	HashMap<String, Handle<DescriptorSetData>> nameToIdSetData;
	DynamicArray<DescriptorSetData> setData;

	Void create_empty(const LogicalDevice& logicalDevice, const VkAllocationCallbacks* allocator);
};

