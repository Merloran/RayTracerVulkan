#pragma once
#include "Common/debug_messenger.hpp"
#include "Common/physical_device.hpp"
#include "Common/logical_device.hpp"
#include "Common/swapchain.hpp"
#include "Common/render_pass.hpp"

#include <vulkan/vulkan.h>



class SRenderManager
{
public:
#ifdef NDEBUG
	static constexpr Bool ENABLE_VALIDATION_LAYERS = false;
#else
	static constexpr Bool ENABLE_VALIDATION_LAYERS = true;
#endif

	SRenderManager(SRenderManager&) = delete;
	static SRenderManager& get();

	Void startup();

	VkImageView create_image_view(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, UInt32 mipLevels);
	const DynamicArray<const Char*>& get_validation_layers();

	Void shutdown();

private:
	SRenderManager()  = default;
	~SRenderManager() = default;

	DynamicArray<const Char*> validationLayers =
	{
		"VK_LAYER_KHRONOS_validation"
	};

	VkInstance instance;
	DebugMessenger debugMessenger;
	VkSurfaceKHR surface; //TODO: Maybe move to display manager

	PhysicalDevice physicalDevice;
	LogicalDevice logicalDevice;
	Swapchain swapchain;
	RenderPass renderPass;



	Void create_vulkan_instance();
	//TODO: Maybe move to display manager
	DynamicArray<const Char*> get_required_extensions();
	Void create_surface();


	Void pick_physical_device(PhysicalDevice& selectedDevice);
	Bool is_device_suitable(PhysicalDevice &device);
	Void find_queue_families(PhysicalDevice &device);
	Void has_swapchain_support(PhysicalDevice &device);
	Void get_max_sample_count(PhysicalDevice &device);
};
