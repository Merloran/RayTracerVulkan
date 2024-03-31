#include "descriptor_pool.hpp"

#include "logical_device.hpp"

#include <magic_enum.hpp>

//TODO: remove exceptions!
Void DescriptorPool::create(const DynamicArray<DescriptorSetInfo>& infos, const LogicalDevice& logicalDevice, const VkAllocationCallbacks* allocator)
{
    const UInt64 descriptorsCount = infos.size();
    VkDescriptorPoolCreateFlags poolFlags = 0;
    setData.reserve(descriptorsCount);
    sets.resize(descriptorsCount);
    layouts.resize(descriptorsCount);
    DynamicArray<UInt32> counts;
    counts.resize(infos.size());
    for (UInt64 i = 0; i < descriptorsCount; ++i)
    {
        const DescriptorSetInfo& info = infos[i];
        auto iterator = nameToIdSetData.find(info.name);
        if (iterator != nameToIdSetData.end())
        {
            SPDLOG_ERROR("Duplicated descriptor set name: {}, descriptor set skipped.", info.name);
            continue;
        }
        DescriptorSetData& data = setData.emplace_back();
        data.name = info.name;
        data.id   = UInt32(i);
        counts[i] = info.count;
        poolFlags |= info.poolFlags;
        
        data.writes.reserve(info.bindings.size());
        for (const VkDescriptorSetLayoutBinding& binding : info.bindings)
        {
            VkWriteDescriptorSet& write = data.writes.emplace_back();

            write.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstBinding       = binding.binding;
            write.descriptorType   = binding.descriptorType;
            // write.descriptorCount  = binding.descriptorCount;
            write.pBufferInfo      = nullptr;
            write.pImageInfo       = nullptr;
            write.pTexelBufferView = nullptr;
            write.pNext            = nullptr;

            VkDescriptorPoolSize& size = sizes.emplace_back();
            size.type            = binding.descriptorType;
            size.descriptorCount = binding.descriptorCount;
        }

        VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags{};
        bindingFlags.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        bindingFlags.pNext         = nullptr;
        bindingFlags.bindingCount  = UInt32(info.bindingFlags.size());
        bindingFlags.pBindingFlags = info.bindingFlags.data();
        
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = UInt32(info.bindings.size());
        layoutInfo.pBindings    = info.bindings.data();
        layoutInfo.pNext        = &bindingFlags;
        layoutInfo.flags        = info.layoutFlags;

        if (vkCreateDescriptorSetLayout(logicalDevice.get_device(), &layoutInfo, allocator, &layouts[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create descriptor set layout!");
        }

        const Handle<DescriptorSetData> handle = { Int32(data.id) };
        nameToIdSetData[data.name] = handle;
    }

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.pNext         = nullptr;
    poolInfo.poolSizeCount = UInt32(sizes.size());
    poolInfo.pPoolSizes    = sizes.data();
    poolInfo.maxSets       = UInt32(infos.size());
    poolInfo.flags         = poolFlags;

    if (vkCreateDescriptorPool(logicalDevice.get_device(), &poolInfo, allocator, &pool) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create descriptor pool!");
    }
    
    VkDescriptorSetVariableDescriptorCountAllocateInfoEXT countAllocateInfo{};
    countAllocateInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT;
    countAllocateInfo.pNext              = nullptr;
    countAllocateInfo.descriptorSetCount = UInt32(counts.size());
    countAllocateInfo.pDescriptorCounts  = counts.data();

    VkDescriptorSetAllocateInfo allocateInfo{};
    allocateInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateInfo.pNext              = &countAllocateInfo;
    allocateInfo.descriptorPool     = pool;
    allocateInfo.descriptorSetCount = UInt32(sets.size());
    allocateInfo.pSetLayouts        = layouts.data();

    if (vkAllocateDescriptorSets(logicalDevice.get_device(), &allocateInfo, sets.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }
}

Void DescriptorPool::setup_sets(const DynamicArray<DescriptorSetupInfo>& infos, const LogicalDevice& logicalDevice)
{
    for (const DescriptorSetupInfo& info : infos)
    {
        DescriptorSetData& data = setData[info.dataHandle.id];

        for (UInt64 i = 0; i < info.resources.size(); ++i)
        {
            VkWriteDescriptorSet& write = data.writes[i];
            const DescriptorResourceInfo& resource = info.resources[i];
            write.dstArrayElement = UInt32(i);
            write.dstSet          = sets[data.id];
            write.descriptorCount = UInt32(resource.bufferInfos.size()
        	                      + resource.imageInfos.size()
        	                      + resource.texelBufferViews.size());
            if (is_resource_compatible(resource, write))
            {
                write.pBufferInfo      = resource.bufferInfos.data();
                write.pImageInfo       = resource.imageInfos.data();
                write.pTexelBufferView = resource.texelBufferViews.data();
                
            } else {
                SPDLOG_ERROR("Descriptor info not set!");
                return;
            }
        }

        vkUpdateDescriptorSets(logicalDevice.get_device(),
                               UInt32(data.writes.size()),
                               data.writes.data(),
                               0,
                               nullptr);
    }
}

Void DescriptorPool::setup_push_constants(const DynamicArray<VkPushConstantRange>& pushConstants)
{
    this->pushConstants = pushConstants;
}

const Handle<DescriptorSetData>& DescriptorPool::get_set_data_handle_by_name(const String& name) const
{
    const auto& iterator = nameToIdSetData.find(name);
    if (iterator == nameToIdSetData.end() || iterator->second.id < 0)
    {
        SPDLOG_ERROR("Set data handle {} not found, returned None.", name);
        return Handle<DescriptorSetData>::sNone;
    }

    return iterator->second;
}

DescriptorSetData& DescriptorPool::get_set_data_by_name(const String& name)
{
    const auto& iterator = nameToIdSetData.find(name);
    if (iterator == nameToIdSetData.end() || iterator->second.id < 0 || iterator->second.id >= Int32(setData.size()))
    {
        SPDLOG_ERROR("Set data {} not found, returned default.", name);
        return setData[0];
    }

    return setData[iterator->second.id];
}

DescriptorSetData& DescriptorPool::get_set_data_by_handle(const Handle<DescriptorSetData> handle)
{
    if (handle.id < 0 || handle.id >= Int32(setData.size()))
    {
        SPDLOG_ERROR("Set data {} not found, returned default.", handle.id);
        return setData[0];
    }
    return setData[handle.id];
}

VkDescriptorSetLayout& DescriptorPool::get_set_layout_by_name(const String& name)
{
    return layouts[get_set_data_by_name(name).id];
}

VkDescriptorSetLayout& DescriptorPool::get_set_layout_by_handle(const Handle<DescriptorSetData> handle)
{
    return layouts[get_set_data_by_handle(handle).id];
}

VkDescriptorSet& DescriptorPool::get_set_by_name(const String& name)
{
    return sets[get_set_data_by_name(name).id];
}

VkDescriptorSet& DescriptorPool::get_set_by_handle(const Handle<DescriptorSetData> handle)
{
    return sets[get_set_data_by_handle(handle).id];
}

const DynamicArray<VkDescriptorSetLayout>& DescriptorPool::get_layouts() const
{
    return layouts;
}

const DynamicArray<VkPushConstantRange>& DescriptorPool::get_push_constants() const
{
    return pushConstants;
}

Bool DescriptorPool::is_resource_compatible(const DescriptorResourceInfo& resource, const VkWriteDescriptorSet& write) const
{
	switch (write.descriptorType)
	{
        case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
		case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
		case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
		case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        case VK_DESCRIPTOR_TYPE_SAMPLER:
		{
            if (resource.imageInfos.empty())
            {
                return false;
            }
            return true;
		}

		case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
		case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
		{
            if (resource.texelBufferViews.empty())
            {
                return false;
            }
            return true;
		}

		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
		case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
		{
            if (resource.bufferInfos.empty())
            {
                return false;
            }
            return true;
		}
            
		default:
		{
            SPDLOG_ERROR("Not supported descriptor type: {}", magic_enum::enum_name(write.descriptorType));
            return false;
		}
	}
}

Void DescriptorPool::clear(const LogicalDevice& logicalDevice, const VkAllocationCallbacks* allocator)
{
    vkDestroyDescriptorPool(logicalDevice.get_device(), pool, allocator);
    for (VkDescriptorSetLayout& layout : layouts)
    {
        vkDestroyDescriptorSetLayout(logicalDevice.get_device(), layout, allocator);
    }
}