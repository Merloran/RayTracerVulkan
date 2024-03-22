#pragma once
#include <vulkan/vulkan.hpp>

struct CommandBuffer
{
	VkCommandBuffer buffer;
	String name;
};
