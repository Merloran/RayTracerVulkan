#include "descriptor_pool.hpp"

#include "logical_device.hpp"

#include <magic_enum.hpp>

//TODO: remove exceptions!
Void DescriptorPool::create(const DynamicArray<DescriptorSetInfo>& infos, const LogicalDevice& logicalDevice, const VkAllocationCallbacks* allocator)
{
    HashMap<VkDescriptorType, UInt32> typeCounters;
    const UInt64 descriptorsCount = infos.size();
    setData.reserve(descriptorsCount);
    sets.resize(descriptorsCount);
    layouts.resize(descriptorsCount);
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

        for (const VkDescriptorSetLayoutBinding& binding : info.bindings)
        {
	        VkWriteDescriptorSet& write = data.writes.emplace_back();

            write.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstBinding       = binding.binding;
            write.descriptorType   = binding.descriptorType;
            write.descriptorCount  = binding.descriptorCount;
            write.dstArrayElement  = 0;
            write.pBufferInfo      = nullptr;
            write.pImageInfo       = nullptr;
            write.pTexelBufferView = nullptr;

            auto iterator = typeCounters.find(binding.descriptorType);
            if (iterator != typeCounters.end())
            {
                iterator->second += binding.descriptorCount;
            } else {
                typeCounters[binding.descriptorType] = binding.descriptorCount;
            }
        }

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = UInt32(info.bindings.size());
        layoutInfo.pBindings    = info.bindings.data();

        if (vkCreateDescriptorSetLayout(logicalDevice.get_device(), &layoutInfo, allocator, &layouts[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create descriptor set layout!");
        }

        const Handle<DescriptorSetData> handle = { Int32(data.id) };
        nameToIdSetData[data.name] = handle;
    }

    sizes.reserve(typeCounters.size());
    for (const Pair<VkDescriptorType, UInt32> pair : typeCounters)
    {
        VkDescriptorPoolSize& size = sizes.emplace_back();
        size.type            = pair.first;
        size.descriptorCount = pair.second;
    }

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = UInt32(sizes.size());
    poolInfo.pPoolSizes    = sizes.data();
    poolInfo.maxSets       = UInt32(infos.size());

    if (vkCreateDescriptorPool(logicalDevice.get_device(), &poolInfo, allocator, &pool) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create descriptor pool!");
    }

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = pool;
    allocInfo.descriptorSetCount = UInt32(sets.size());
    allocInfo.pSetLayouts        = layouts.data();

    if (vkAllocateDescriptorSets(logicalDevice.get_device(), &allocInfo, sets.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }
}

const Handle<DescriptorSetData>& DescriptorPool::get_set_data_handle_by_name(const String& name) const
{
    const auto& iterator = nameToIdSetData.find(name);
    if (iterator == nameToIdSetData.end() || iterator->second.id < 0)
    {
        SPDLOG_WARN("Set data handle {} not found, returned None.", name);
        return Handle<DescriptorSetData>::sNone;
    }

    return iterator->second;
}

DescriptorSetData& DescriptorPool::get_set_data_by_name(const String& name)
{
    const auto& iterator = nameToIdSetData.find(name);
    if (iterator == nameToIdSetData.end() || iterator->second.id < 0 || iterator->second.id >= Int32(setData.size()))
    {
        SPDLOG_WARN("Set data {} not found, returned default.", name);
        return setData[0];
    }

    return setData[iterator->second.id];
}

DescriptorSetData& DescriptorPool::get_set_data_by_handle(const Handle<DescriptorSetData> handle)
{
    if (handle.id < 0 || handle.id >= Int32(setData.size()))
    {
        SPDLOG_WARN("Set data {} not found, returned default.", handle.id);
        return setData[0];
    }
    return setData[handle.id];
}

const DynamicArray<VkDescriptorSetLayout>& DescriptorPool::get_layouts() const
{
    return layouts;
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

            write.dstSet = sets[data.id];
            if (is_resource_compatible(resource, write))
            {
                if (resource.bufferInfo.has_value())
                {
                    write.pBufferInfo = &resource.bufferInfo.value();
                }
                else if (resource.imageInfo.has_value())
                {
                    write.pImageInfo = &resource.imageInfo.value();
                }
                else if (resource.texelBufferView.has_value())
                {
                    write.pTexelBufferView = &resource.texelBufferView.value();
                } else {
                    SPDLOG_ERROR("Descriptor info not set!");
                    return;
                }
            }
        }

        vkUpdateDescriptorSets(logicalDevice.get_device(),
                               UInt32(data.writes.size()),
                               data.writes.data(),
                               0,
                               nullptr);
    }
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
            if (!resource.imageInfo.has_value())
            {
                return false;
            }
            return true;
		}

		case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
		case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
		{
            if (!resource.texelBufferView.has_value())
            {
                return false;
            }
            return true;
		}

		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
		case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
		{
            if (!resource.bufferInfo.has_value())
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