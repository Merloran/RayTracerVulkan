#pragma once
#include <vulkan/vulkan_core.h>

class DebugMessenger
{
public:
	Void create(const VkInstance& instance, const VkAllocationCallbacks* allocator);

	const VkDebugUtilsMessengerEXT& get_debug_messenger() const;

	Bool check_validation_layer_support() const;
	Void fill_debug_messenger_create_info(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

	static VKAPI_ATTR VkBool32 VKAPI_CALL s_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
														   VkDebugUtilsMessageTypeFlagsEXT messageType,
														   const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
														   Void* pUserData);

	Void clear(VkInstance& instance, const VkAllocationCallbacks* allocator);

private:
	VkDebugUtilsMessengerEXT debugMessenger = nullptr;

	const DynamicArray<const Char*> validationLayers =
	{
		"VK_LAYER_KHRONOS_validation"
	};

	VkResult create_debug_messenger(const VkInstance& instance,
	                                const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
	                                const VkAllocationCallbacks* pAllocator,
	                                VkDebugUtilsMessengerEXT* pDebugMessenger);

};

