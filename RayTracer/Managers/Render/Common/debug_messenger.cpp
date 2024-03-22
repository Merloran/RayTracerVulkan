#include "debug_messenger.hpp"

#include <magic_enum.hpp>

Void DebugMessenger::create(const VkInstance& instance, const VkAllocationCallbacks* allocator)
{
    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    fill_debug_messenger_create_info(createInfo);

    if (create_debug_messenger(instance, &createInfo, allocator, &debugMessenger) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to set up debug messenger!");
    }

}

const VkDebugUtilsMessengerEXT& DebugMessenger::get_debug_messenger() const
{
    return debugMessenger;
}

const DynamicArray<const Char*>& DebugMessenger::get_validation_layers() const
{
    return validationLayers;
}

Void DebugMessenger::fill_debug_messenger_create_info(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
                               | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                               | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                           | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                           | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

    createInfo.pfnUserCallback = s_debug_callback;
}

Bool DebugMessenger::check_validation_layer_support() const
{
    UInt32 layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    DynamicArray<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const Char* layerName : validationLayers)
    {
        Bool layerFound = false;

        for (const VkLayerProperties& layerProperties : availableLayers)
        {
            if (strcmp(layerName, layerProperties.layerName) == 0)
            {
                layerFound = true;
                break;
            }
        }

        if (!layerFound)
        {
            return false;
        }
    }

    return true;
}

VkResult DebugMessenger::create_debug_messenger(const VkInstance& instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
{
    const auto function = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
    if (function != nullptr)
    {
        return function(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL DebugMessenger::s_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, Void* pUserData)
{
    switch (messageSeverity)
    {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        {
            SPDLOG_INFO("Validation layer: {}", pCallbackData->pMessage);
            break;
        }
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        {
            SPDLOG_WARN("Validation layer: {}", pCallbackData->pMessage);
            break;
        }
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        {
            SPDLOG_ERROR("Validation layer: {}", pCallbackData->pMessage);
            break;
        }
        default:
        {
            SPDLOG_INFO("Not supported severity: {}\n {}", magic_enum::enum_name(messageSeverity), pCallbackData->pMessage);
            break;
        }
    }

    return VK_FALSE;
}

Void DebugMessenger::clear(VkInstance& instance, const VkAllocationCallbacks* allocator)
{
	const auto function = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
    if (function != nullptr)
    {
        function(instance, debugMessenger, allocator);
    }
}
