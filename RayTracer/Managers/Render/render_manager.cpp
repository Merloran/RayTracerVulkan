#include "render_manager.hpp"

#include <GLFW/glfw3.h>

#include "../Display/display_manager.hpp"


SRenderManager& SRenderManager::get()
{
	static SRenderManager instance;
	return instance;
}

Void SRenderManager::startup()
{
	SPDLOG_INFO("Render Manager startup.");

    create_vulkan_instance();
    if constexpr (ENABLE_VALIDATION_LAYERS)
    {
        debugMessenger.create(instance);
    }
    create_surface();
    pick_physical_device(physicalDevice);
    logicalDevice.create(physicalDevice, nullptr);
    swapchain.create(logicalDevice, physicalDevice, surface, nullptr);
    renderPass.create(physicalDevice, logicalDevice, swapchain, nullptr);
}

const DynamicArray<const Char*>& SRenderManager::get_validation_layers()
{
    return validationLayers;
}

Void SRenderManager::create_vulkan_instance()
{
    if constexpr (ENABLE_VALIDATION_LAYERS)
    {
        if (!debugMessenger.check_validation_layer_support())
        {
            throw std::runtime_error("validation layers requested, but not available!");
        }
    }

    //APP INFO
    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "Ray Tracer";
    appInfo.applicationVersion = VK_MAKE_API_VERSION(0U, 1U, 0U, 0U);
    appInfo.pEngineName        = "RayEngine";
    appInfo.engineVersion      = VK_MAKE_API_VERSION(0U, 1U, 0U, 0U);
    appInfo.apiVersion         = VK_API_VERSION_1_0;

    //INSTANCE CREATE INFO
    VkInstanceCreateInfo createInfo{};
    const DynamicArray<const Char*> extensions = get_required_extensions();

    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    if constexpr (ENABLE_VALIDATION_LAYERS)
    {
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        debugMessenger.fill_debug_messenger_create_info(debugCreateInfo);
        createInfo.enabledLayerCount = UInt32(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
        createInfo.pNext = &debugCreateInfo;
    } else {
        createInfo.enabledLayerCount = 0U;
        createInfo.pNext = nullptr;
    }
    createInfo.enabledExtensionCount = UInt32(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    //TODO: in the far future think about using custom allocator
    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create instance!");
    }

}

DynamicArray<const Char*> SRenderManager::get_required_extensions()
{
    UInt32 glfwExtensionCount = 0U;
    const Char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    DynamicArray<const Char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if constexpr (ENABLE_VALIDATION_LAYERS)
    {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

Void SRenderManager::create_surface()
{
    SDisplayManager& displayManager = SDisplayManager::get();

    if (glfwCreateWindowSurface(instance, displayManager.get_window(), nullptr, &surface) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create window surface!");
    }
}

Void SRenderManager::pick_physical_device(PhysicalDevice& selectedDevice)
{
    // Check for device with Vulkan support
    UInt32 deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0)
    {
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }

    // Check for device that is suitable
    DynamicArray<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
    for (const VkPhysicalDevice& device : devices)
    {
        selectedDevice.device = device;

        if (is_device_suitable(physicalDevice))
        {
            get_max_sample_count(physicalDevice);
            break;
        }

        selectedDevice.device = VK_NULL_HANDLE;
    }

    if (selectedDevice.device == VK_NULL_HANDLE)
    {
        throw std::runtime_error("failed to find a suitable GPU!");
    }
}

Bool SRenderManager::is_device_suitable(PhysicalDevice &device)
{
    find_queue_families(device);

    Bool isSwapChainAdequate = false;
    if (device.check_extension_support())
    {
        has_swapchain_support(device);
        isSwapChainAdequate = !device.formats.empty() && !device.presentModes.empty();
    }
    
    vkGetPhysicalDeviceFeatures(device.device, &device.supportedFeatures);

    return device.are_families_valid() &&
           isSwapChainAdequate &&
           device.supportedFeatures.samplerAnisotropy;
}

Void SRenderManager::find_queue_families(PhysicalDevice& device)
{
    UInt32 queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device.device, &queueFamilyCount, nullptr);

    DynamicArray<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device.device, &queueFamilyCount, queueFamilies.data());

    for (UInt32 i = 0; i < UInt32(queueFamilies.size()); ++i)
    {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT &&
            queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
        {
            device.graphicsFamily = i;
            device.computeFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device.device, i, surface, &presentSupport);
        if (presentSupport)
        {
            device.presentFamily = i;
        }

        if (device.are_families_valid())
        {
            break;
        }
    }
}

Void SRenderManager::has_swapchain_support(PhysicalDevice& device)
{
    UInt32 formatCount;
    UInt32 presentModeCount;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.device, surface, &device.capabilities);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device.device, surface, &formatCount, nullptr);

    if (formatCount != 0)
    {
        device.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device.device,
                                             surface,
                                             &formatCount,
                                             device.formats.data());
    }

    vkGetPhysicalDeviceSurfacePresentModesKHR(device.device,
                                              surface,
                                              &presentModeCount,
                                              nullptr);

    if (presentModeCount != 0)
    {
        device.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device.device,
                                                  surface,
                                                  &presentModeCount,
                                                  device.presentModes.data());
    }
}

Void SRenderManager::get_max_sample_count(PhysicalDevice& device)
{
    vkGetPhysicalDeviceProperties(device.device, &device.properties);
    VkSampleCountFlags counts = device.properties.limits.framebufferColorSampleCounts
							  & device.properties.limits.framebufferDepthSampleCounts;


    if (counts & VK_SAMPLE_COUNT_64_BIT)
    {
	    device.maxSamples = VK_SAMPLE_COUNT_64_BIT;
    }
    else if (counts & VK_SAMPLE_COUNT_32_BIT)
    {
        device.maxSamples = VK_SAMPLE_COUNT_32_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_16_BIT)
    {
        device.maxSamples = VK_SAMPLE_COUNT_16_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_8_BIT)
    {
        device.maxSamples = VK_SAMPLE_COUNT_8_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_4_BIT)
    {
        device.maxSamples = VK_SAMPLE_COUNT_4_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_2_BIT)
    {
        device.maxSamples = VK_SAMPLE_COUNT_2_BIT;
    } else {
        device.maxSamples = VK_SAMPLE_COUNT_1_BIT;
    }
}

VkImageView SRenderManager::create_image_view(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, UInt32 mipLevels)
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(logicalDevice.get_device(), &viewInfo, nullptr, &imageView) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create texture image view!");
    }

    return imageView;
}

Void SRenderManager::shutdown()
{
    SPDLOG_INFO("Render Manager shutdown.");

    renderPass.clear(logicalDevice, nullptr);
    swapchain.clear(logicalDevice, nullptr);
    logicalDevice.clear();

    if constexpr (ENABLE_VALIDATION_LAYERS)
    {
        debugMessenger.clear(instance, nullptr);
    }

    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
}