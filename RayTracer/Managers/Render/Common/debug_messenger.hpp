#pragma once
#include <vulkan/vulkan_core.h>

class DebugMessenger
{
public:
#ifdef NDEBUG
	static constexpr Bool ENABLE_VALIDATION_LAYERS = false;
#else
	static constexpr Bool ENABLE_VALIDATION_LAYERS = true;
#endif
	Void create(const VkInstance& instance, const VkAllocationCallbacks* allocator);

	[[nodiscard]]
	const VkDebugUtilsMessengerEXT& get_debug_messenger() const;
	[[nodiscard]]
	const DynamicArray<const Char*>& get_validation_layers() const;

	[[nodiscard]]
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

