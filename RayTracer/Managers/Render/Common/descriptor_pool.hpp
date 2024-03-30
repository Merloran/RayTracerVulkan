#pragma once
#include "../../Resource/Common/handle.hpp"
#include <vulkan/vulkan.hpp>

class LogicalDevice;


struct DescriptorSetInfo
{
	DynamicArray<VkDescriptorSetLayoutBinding> bindings;
	DynamicArray<VkDescriptorBindingFlags> bindingFlags;
	VkDescriptorSetLayoutCreateFlags layoutFlags;
	VkDescriptorPoolCreateFlags poolFlags;
	UInt32 count;
	String name;
};

struct DescriptorSetData
{
	DynamicArray<VkWriteDescriptorSet> writes;
	String name;
	UInt32 id;
};

struct DescriptorResourceInfo
{
	DynamicArray<VkDescriptorImageInfo> imageInfos;
	DynamicArray<VkDescriptorBufferInfo> bufferInfos;
	DynamicArray<VkBufferView> texelBufferViews;
};

struct DescriptorSetupInfo
{
	DynamicArray<DescriptorResourceInfo> resources;
	Handle<DescriptorSetData> dataHandle;
};

class DescriptorPool
{
public:
	Void create(const DynamicArray<DescriptorSetInfo>& infos,
				const LogicalDevice& logicalDevice, 
				const VkAllocationCallbacks* allocator);

	Void setup_sets(const DynamicArray<DescriptorSetupInfo>& infos, const LogicalDevice& logicalDevice);

	Void setup_push_constants(const DynamicArray<VkPushConstantRange> &pushConstants);

	[[nodiscard]]
	const Handle<DescriptorSetData>& get_set_data_handle_by_name(const String& name)  const;
	DescriptorSetData& get_set_data_by_name(const String& name);
	DescriptorSetData& get_set_data_by_handle(const Handle<DescriptorSetData> handle);
	VkDescriptorSetLayout& get_set_layout_by_name(const String& name);
	VkDescriptorSetLayout& get_set_layout_by_handle(const Handle<DescriptorSetData> handle);
	VkDescriptorSet& get_set_by_name(const String& name);
	VkDescriptorSet& get_set_by_handle(const Handle<DescriptorSetData> handle);
	const DynamicArray<VkDescriptorSetLayout>& get_layouts() const;
	const DynamicArray<VkPushConstantRange>& get_push_constants() const;


	Void clear(const LogicalDevice& logicalDevice, const VkAllocationCallbacks* allocator);

private:
	VkDescriptorPool pool = nullptr;
	DynamicArray<VkDescriptorPoolSize> sizes;
	HashMap<String, Handle<DescriptorSetData>> nameToIdSetData;
	DynamicArray<DescriptorSetData> setData;
	DynamicArray<VkDescriptorSetLayout> layouts;
	DynamicArray<VkPushConstantRange> pushConstants;
	DynamicArray<VkDescriptorSet> sets;

	Bool is_resource_compatible(const DescriptorResourceInfo& resource, const VkWriteDescriptorSet& write) const;
};

