#pragma once
#include "../../Resource/Common/handle.hpp"
#include <vulkan/vulkan.hpp>

class LogicalDevice;


struct DescriptorSetInfo
{
	DynamicArray<VkDescriptorSetLayoutBinding> bindings;
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
	Optional<VkDescriptorImageInfo> imageInfo;
	Optional<VkDescriptorBufferInfo> bufferInfo;
	Optional<VkBufferView> texelBufferView;
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

	[[nodiscard]]
	const Handle<DescriptorSetData>& get_set_data_handle_by_name(const String& name)  const;
	DescriptorSetData& get_set_data_by_name(const String& name);
	DescriptorSetData& get_set_data_by_handle(const Handle<DescriptorSetData> handle);
	const DynamicArray<VkDescriptorSetLayout>& get_layouts() const;

	Void setup_sets(const DynamicArray<DescriptorSetupInfo>& infos, const LogicalDevice& logicalDevice);

	Void clear(const LogicalDevice& logicalDevice, const VkAllocationCallbacks* allocator);

private:
	VkDescriptorPool pool = nullptr;
	DynamicArray<VkDescriptorPoolSize> sizes;
	HashMap<String, Handle<DescriptorSetData>> nameToIdSetData;
	DynamicArray<DescriptorSetData> setData;
	DynamicArray<VkDescriptorSetLayout> layouts;
	DynamicArray<VkDescriptorSet> sets;

	Bool is_resource_compatible(const DescriptorResourceInfo& resource, const VkWriteDescriptorSet& write) const;
};

