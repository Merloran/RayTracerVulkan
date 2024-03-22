#include "render_manager.hpp"

#include <filesystem>
#include <fstream>
#include <GLFW/glfw3.h>

#include "../Display/display_manager.hpp"
#include "../Resource/Common/handle.hpp"
#include "Common/command_buffer.hpp"
#include "Common/shader.hpp"


SRenderManager& SRenderManager::get()
{
	static SRenderManager instance;
	return instance;
}

Void SRenderManager::startup()
{
	SPDLOG_INFO("Render Manager startup.");

    create_vulkan_instance();
    if constexpr (DebugMessenger::ENABLE_VALIDATION_LAYERS)
    {
        debugMessenger.create(instance, nullptr);
    }
    create_surface();
    physicalDevice.select_physical_device(instance, surface);
    logicalDevice.create(physicalDevice, debugMessenger, nullptr);
    //Shaders should be created after logical device
    Handle<Shader> vert = load_shader(SHADERS_PATH + "Shader.vert", EShaderType::Vertex);
    Handle<Shader> frag = load_shader(SHADERS_PATH + "Shader.frag", EShaderType::Fragment);
    DynamicArray<Shader> shaders;
    shaders.emplace_back(get_shader_by_handle(vert));
    shaders.emplace_back(get_shader_by_handle(frag));
    swapchain.create(logicalDevice, physicalDevice, surface, nullptr);
    renderPass.create(physicalDevice, logicalDevice, swapchain, nullptr);
    setup_graphics_descriptors();
    create_command_pool(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    
    
    graphicsPipeline.create_graphics_pipeline(descriptorPool, renderPass, shaders, logicalDevice, nullptr);
    // computePipeline.create_compute_pipeline(, logicalDevice, nullptr);
}

const Handle<Shader>& SRenderManager::get_shader_handle_by_name(const String& name) const
{
    const auto& iterator = nameToIdShaders.find(name);
    if (iterator == nameToIdShaders.end() || iterator->second.id < 0)
    {
        SPDLOG_WARN("Shader handle {} not found, returned None.", name);
        return Handle<Shader>::sNone;
    }

    return iterator->second;
}

Shader& SRenderManager::get_shader_by_name(const String& name)
{
    const auto& iterator = nameToIdShaders.find(name);
    if (iterator == nameToIdShaders.end() || iterator->second.id < 0 || iterator->second.id >= Int32(shaders.size()))
    {
        SPDLOG_WARN("Shader {} not found, returned default.", name);
        return shaders[0];
    }

    return shaders[iterator->second.id];
}

Shader& SRenderManager::get_shader_by_handle(const Handle<Shader> handle)
{
    if (handle.id < 0 || handle.id >= Int32(shaders.size()))
    {
        SPDLOG_WARN("Shader {} not found, returned default.", handle.id);
        return shaders[0];
    }
    return shaders[handle.id];
}

const Handle<CommandBuffer>& SRenderManager::get_command_buffer_handle_by_name(const String& name) const
{
    const auto& iterator = nameToIdCommandBuffers.find(name);
    if (iterator == nameToIdCommandBuffers.end() || iterator->second.id < 0)
    {
        SPDLOG_WARN("Command buffer handle {} not found, returned None.", name);
        return Handle<CommandBuffer>::sNone;
    }

    return iterator->second;
}

CommandBuffer& SRenderManager::get_command_buffer_by_name(const String& name)
{
    const auto& iterator = nameToIdCommandBuffers.find(name);
    if (iterator == nameToIdCommandBuffers.end() || iterator->second.id < 0 || 
        iterator->second.id >= Int32(commandBuffers.size()))
    {
        SPDLOG_WARN("Command buffer {} not found, returned default.", name);
        return commandBuffers[0];
    }

    return commandBuffers[iterator->second.id];
}

CommandBuffer& SRenderManager::get_command_buffer_by_handle(const Handle<CommandBuffer> handle)
{
    if (handle.id < 0 || handle.id >= Int32(shaders.size()))
    {
        SPDLOG_WARN("Command buffer {} not found, returned default.", handle.id);
        return commandBuffers[0];
    }
    return commandBuffers[handle.id];
}

Handle<Shader> SRenderManager::load_shader(const String& filePath, const EShaderType shaderType)
{
    const UInt64 shaderId = shaders.size();
    Shader& shader        = shaders.emplace_back();

    const std::filesystem::path path(filePath);
    const String destinationPath = (SHADERS_PATH / path.filename()).string() + COMPILED_SHADER_EXTENSION;
    shader.create(filePath, destinationPath, GLSL_COMPILER_PATH, shaderType, logicalDevice, nullptr);

    const Handle<Shader> handle{Int32(shaderId)};
    nameToIdShaders[shader.get_file_path()] = handle;

    return handle;
}

Void SRenderManager::create_vulkan_instance()
{
    if constexpr (DebugMessenger::ENABLE_VALIDATION_LAYERS)
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
    if constexpr (DebugMessenger::ENABLE_VALIDATION_LAYERS)
    {
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        debugMessenger.fill_debug_messenger_create_info(debugCreateInfo);
        const DynamicArray<const Char*>& validationLayers = debugMessenger.get_validation_layers();
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

    if constexpr (DebugMessenger::ENABLE_VALIDATION_LAYERS)
    {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

Void SRenderManager::create_surface()
{
	const SDisplayManager& displayManager = SDisplayManager::get();

    if (glfwCreateWindowSurface(instance, displayManager.get_window(), nullptr, &surface) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create window surface!");
    }
}

Void SRenderManager::create_command_pool(VkCommandPoolCreateFlagBits flags)
{
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags            = flags;
    poolInfo.queueFamilyIndex = physicalDevice.get_graphics_family_index();

    if (vkCreateCommandPool(logicalDevice.get_device(), &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create command pool!");
    }
}

Void SRenderManager::create_command_buffers(VkCommandBufferLevel level, const DynamicArray<String>& names)
{
    commandBuffers.reserve(names.size());
    DynamicArray<VkCommandBuffer> buffers;
    buffers.resize(names.size());
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = commandPool;
    allocInfo.level              = level;
    allocInfo.commandBufferCount = UInt32(buffers.size());

    if (vkAllocateCommandBuffers(logicalDevice.get_device(), &allocInfo, buffers.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate command buffers!");
    }

    for (UInt64 i = 0; i < names.size(); ++i)
    {
        CommandBuffer& buffer = commandBuffers.emplace_back();
        buffer.buffer = buffers[i];
        buffer.name = names[i];
    }
}

Void SRenderManager::setup_graphics_descriptors()
{
    DynamicArray<DescriptorSetInfo> infos;
    DescriptorSetInfo& info = infos.emplace_back();
    info.name = "GraphicsDescriptorSet";
    info.bindings.reserve(4);
    VkDescriptorSetLayoutBinding& uniform  = info.bindings.emplace_back();
    VkDescriptorSetLayoutBinding& image    = info.bindings.emplace_back();
    VkDescriptorSetLayoutBinding& storage1 = info.bindings.emplace_back();
    VkDescriptorSetLayoutBinding& storage2 = info.bindings.emplace_back();

    uniform.binding         = 0;
    uniform.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uniform.descriptorCount = 1;
    uniform.stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;

    image.binding         = 1;
    image.descriptorCount = 1;
    image.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    image.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

    storage1.binding         = 2;
    storage1.descriptorCount = 1;
    storage1.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    storage1.stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;

    storage2.binding         = 3;
    storage2.descriptorCount = 1;
    storage2.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    storage2.stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;


    descriptorPool.create(infos, logicalDevice, nullptr);



    // descriptorPool.setup_sets(infos, logicalDevice, nullptr);
    
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

    vkDestroyCommandPool(logicalDevice.get_device(), commandPool, nullptr);
    descriptorPool.clear(logicalDevice, nullptr);
    computePipeline.clear(logicalDevice, nullptr);
    graphicsPipeline.clear(logicalDevice, nullptr);
    renderPass.clear(logicalDevice, nullptr);
    swapchain.clear(logicalDevice, nullptr);

    for (Shader& shader : shaders)
    {
        shader.clear(logicalDevice, nullptr);
    }
    logicalDevice.clear(nullptr);

    if constexpr (DebugMessenger::ENABLE_VALIDATION_LAYERS)
    {
        debugMessenger.clear(instance, nullptr);
    }

    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
}