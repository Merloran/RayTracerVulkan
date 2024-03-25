#include "render_manager.hpp"

#include <filesystem>
#include <GLFW/glfw3.h>

#include "../Display/display_manager.hpp"
#include "../Resource/Common/handle.hpp"
#include "../Resource/Common/texture.hpp"
#include "../Resource/Common/mesh.hpp"
#include "Camera/camera.hpp"
#include "Common/command_buffer.hpp"
#include "Common/shader.hpp"
#include "Common/image.hpp"


SRenderManager& SRenderManager::get()
{
	static SRenderManager instance;
	return instance;
}

Void SRenderManager::startup()
{
	SPDLOG_INFO("Render Manager startup.");
    isFrameEven = false;
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
    create_color_image();
    create_depth_image();
    create_uniform_buffer();
    setup_graphics_descriptors();

    create_command_pool(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    create_command_buffers(VK_COMMAND_BUFFER_LEVEL_PRIMARY, { "Graphics1", "Graphics2" });

    renderPass.create(physicalDevice, logicalDevice, swapchain, colorImageHandle, depthImageHandle, nullptr);
    create_framebuffers();


    graphicsPipeline.create_graphics_pipeline(descriptorPool, renderPass, shaders, logicalDevice, nullptr);
    // computePipeline.create_compute_pipeline(, logicalDevice, nullptr);

    create_synchronization_objects();
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
    if (handle.id < 0 || handle.id >= Int32(commandBuffers.size()))
    {
        SPDLOG_WARN("Command buffer {} not found, returned default.", handle.id);
        return commandBuffers[0];
    }
    return commandBuffers[handle.id];
}

Image& SRenderManager::get_image_by_handle(const Handle<Image> handle)
{
    if (handle.id < 0 || handle.id >= Int32(images.size()))
    {
        SPDLOG_WARN("Image {} not found, returned default.", handle.id);
        return images[0];
    }
    return images[handle.id];
}

Buffer& SRenderManager::get_buffer_by_handle(const Handle<Buffer> handle)
{
    if (handle.id < 0 || handle.id >= Int32(buffers.size()))
    {
        SPDLOG_WARN("Buffer {} not found, returned default.", handle.id);
        return buffers[0];
    }
    return buffers[handle.id];
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

Void SRenderManager::generate_mesh_buffers(DynamicArray<Mesh>& meshes)
{
    for (Mesh& mesh : meshes)
    {
        create_mesh_buffers(mesh);
    }
}

Void SRenderManager::create_mesh_buffers(Mesh& mesh)
{
    create_vertex_buffer(mesh);
    create_index_buffer(mesh);
}

Void SRenderManager::draw_frame(Camera& camera, const DynamicArray<Mesh>& meshes)
{
    UInt64 currentFrame = isFrameEven ? 1 : 0;
    vkWaitForFences(logicalDevice.get_device(), 1, &inFlightFences[currentFrame], VK_TRUE, Limits<UInt64>::max());

    UInt32 imageIndex;
    VkResult result = vkAcquireNextImageKHR(logicalDevice.get_device(),
                                            swapchain.get_swapchain(),
                                            Limits<UInt64>::max(),
                                            imageAvailableSemaphores[currentFrame],
                                            VK_NULL_HANDLE,
                                            &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        recreate_swapchain();
        return;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    update_uniform_buffer(UInt32(currentFrame), camera);

    vkResetFences(logicalDevice.get_device(), 1, &inFlightFences[currentFrame]);

    VkCommandBuffer currentBuffer = get_command_buffer_by_name("Graphics" + std::to_string(currentFrame + 1)).buffer;
    vkResetCommandBuffer(currentBuffer, 0);
    record_command_buffer(currentBuffer, imageIndex, meshes);

    VkSubmitInfo submitInfo{};
    VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
    VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount   = 1;
    submitInfo.pWaitSemaphores      = waitSemaphores;
    submitInfo.pWaitDstStageMask    = waitStages;
    submitInfo.commandBufferCount   = 1;
    submitInfo.pCommandBuffers      = &currentBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = signalSemaphores;

    if (vkQueueSubmit(logicalDevice.get_graphics_queue(), 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = signalSemaphores;
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = &swapchain.get_swapchain();
    presentInfo.pImageIndices      = &imageIndex;
    presentInfo.pResults           = nullptr; // Optional

    result = vkQueuePresentKHR(logicalDevice.get_present_queue(), &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || SDisplayManager::get().was_resize_handled())
    {
        recreate_swapchain();
    }
    else if (result != VK_SUCCESS)
    {
        throw std::runtime_error("failed to present swap chain image!");
    }

    isFrameEven = !isFrameEven;
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

    if (glfwCreateWindowSurface(instance, &displayManager.get_window(), nullptr, &surface) != VK_SUCCESS)
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
    for (const String& name : names)
    {
        const auto& iterator = nameToIdCommandBuffers.find(name);
        if (iterator != nameToIdCommandBuffers.end())
        {
            SPDLOG_ERROR("Failed to create command buffers name: {} already exist.", name);
        }
    }
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
	    const Handle<CommandBuffer> handle = { Int32(commandBuffers.size()) };
        nameToIdCommandBuffers[names[i]] = handle;
        CommandBuffer& buffer = commandBuffers.emplace_back();
        buffer.buffer = buffers[i];
        buffer.name = names[i];
    }
}

Void SRenderManager::create_color_image()
{
    if (colorImageHandle.id == Handle<Image>::sNone.id)
    {
        colorImageHandle.id = Int32(images.size());
        images.emplace_back();
    }
    Image& image = images[colorImageHandle.id];

    image.create(physicalDevice,
                 logicalDevice,
                 swapchain.get_extent(),
                 1,
                 logicalDevice.get_samples(),
                 swapchain.get_image_format(),
                 VK_IMAGE_TILING_OPTIMAL,
                 VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 nullptr);
    image.create_view(logicalDevice, VK_IMAGE_ASPECT_COLOR_BIT, nullptr);
}

Void SRenderManager::create_depth_image()
{
    if (depthImageHandle.id == Handle<Image>::sNone.id)
    {
        depthImageHandle.id = Int32(images.size());
        images.emplace_back();
    }
	Image& image = images[depthImageHandle.id];
    
    image.create(physicalDevice,
                 logicalDevice,
                 swapchain.get_extent(),
                 1,
                 logicalDevice.get_samples(),
                 physicalDevice.find_depth_format(),
                 VK_IMAGE_TILING_OPTIMAL,
                 VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 nullptr);
    image.create_view(logicalDevice, VK_IMAGE_ASPECT_DEPTH_BIT, nullptr);
}

Void SRenderManager::create_framebuffers()
{
	const DynamicArray<VkImageView>& swapchainImageViews = swapchain.get_image_views();
    framebuffers.resize(swapchainImageViews.size());
    const DynamicArray<Handle<Image>>& imageOrder = renderPass.get_image_order();
    DynamicArray<VkImageView> views;
    // 1 is for swapchain image view
    views.resize(imageOrder.size() + 1);
    const UVector2& extent = swapchain.get_extent();

    for (UInt64 i = 0; i < imageOrder.size(); ++i)
    {
        views[i + 1] = get_image_by_handle(imageOrder[i]).get_view();
    }

    for (UInt64 i = 0; i < swapchainImageViews.size(); ++i)
    {
        views[0] = swapchainImageViews[i];

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass      = renderPass.get_render_pass();
        framebufferInfo.attachmentCount = UInt32(views.size());
        framebufferInfo.pAttachments    = views.data();
        framebufferInfo.width           = extent.x;
        framebufferInfo.height          = extent.y;
        framebufferInfo.layers          = 1;

        if (vkCreateFramebuffer(logicalDevice.get_device(), &framebufferInfo, nullptr, &framebuffers[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }
}

Void SRenderManager::create_vertex_buffer(Mesh& mesh)
{
	const UInt64 positionsSize = sizeof(mesh.positions[0]) * mesh.positions.size();
	const UInt64 normalsSize   = sizeof(mesh.normals[0]) * mesh.normals.size();
	const UInt64 uvsSize       = sizeof(mesh.uvs[0]) * mesh.uvs.size();
	const VkDeviceSize bufferSize = positionsSize
							      + normalsSize
							      + uvsSize;

    Buffer stagingBuffer;
    stagingBuffer.create(physicalDevice,
                         logicalDevice,
                         bufferSize,
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         nullptr);

    Void* data;
    UInt64 offset = 0;
    vkMapMemory(logicalDevice.get_device(), stagingBuffer.get_memory(), 0, bufferSize, 0, &data);
    UInt8* destination = reinterpret_cast<UInt8*>(data);

    memcpy(destination + offset, mesh.positions.data(), positionsSize);
    offset += positionsSize;
    memcpy(destination + offset, mesh.normals.data(), normalsSize);
    offset += normalsSize;
    memcpy(destination + offset, mesh.uvs.data(), uvsSize);

    vkUnmapMemory(logicalDevice.get_device(), stagingBuffer.get_memory());

    mesh.vertexesHandle.id = UInt32(buffers.size());
    Buffer& vertexBuffer = buffers.emplace_back();

    vertexBuffer.create(physicalDevice,
                        logicalDevice,
                        bufferSize,
                        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                        nullptr);

    copy_buffer(stagingBuffer, vertexBuffer);

    stagingBuffer.clear(logicalDevice, nullptr);
}

Void SRenderManager::create_index_buffer(Mesh& mesh)
{
    VkDeviceSize bufferSize = sizeof(mesh.indexes[0]) * mesh.indexes.size();

    Buffer stagingBuffer;
    stagingBuffer.create(physicalDevice,
                         logicalDevice,
                         bufferSize,
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         nullptr);

    Void* data;
    vkMapMemory(logicalDevice.get_device(), stagingBuffer.get_memory(), 0, bufferSize, 0, &data);
    memcpy(data, mesh.indexes.data(), bufferSize);
    vkUnmapMemory(logicalDevice.get_device(), stagingBuffer.get_memory());

    mesh.indexesHandle.id = UInt32(buffers.size());
    Buffer& indexBuffer = buffers.emplace_back();

    indexBuffer.create(physicalDevice,
                       logicalDevice,
                       bufferSize,
                       VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                       nullptr);

    copy_buffer(stagingBuffer, indexBuffer);

    stagingBuffer.clear(logicalDevice, nullptr);
}

Void SRenderManager::create_uniform_buffer()
{
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);
    uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    for (Buffer& buffer : uniformBuffers)
    {
        buffer.create(physicalDevice,
                      logicalDevice,
                      bufferSize,
                      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      nullptr);
        vkMapMemory(logicalDevice.get_device(), buffer.get_memory(), 0, bufferSize, 0, buffer.get_mapped_memory());
    }
}

Void SRenderManager::create_texture_image(Texture& texture, UInt32 mipLevels)
{
    Buffer stagingBuffer;
    const UInt64 textureSize = texture.size.x * texture.size.y * texture.channels;
    stagingBuffer.create(physicalDevice,
                         logicalDevice,
                         textureSize,
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                         nullptr);

    Void* data;
    vkMapMemory(logicalDevice.get_device(), stagingBuffer.get_memory(), 0, textureSize, 0, &data);
    memcpy(data, texture.data, textureSize);
    vkUnmapMemory(logicalDevice.get_device(), stagingBuffer.get_memory());

    texture.image.id = Int32(images.size());
    Image& textureImage = images.emplace_back();

    VkFormat format;
    if (texture.channels == 3)
    {
        format = VK_FORMAT_R8G8B8_SRGB;
    }
    else if (texture.channels == 4)
    {

        format = VK_FORMAT_R8G8B8A8_SRGB;
    } else {
        SPDLOG_ERROR("Not supported channels count: {} in texture: {}", texture.channels, texture.name);
        return;
    }

    textureImage.create(physicalDevice,
                        logicalDevice,
                        texture.size,
                        mipLevels, 
                        VK_SAMPLE_COUNT_1_BIT,
                        format,
                        VK_IMAGE_TILING_OPTIMAL,
                        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                        nullptr);

    transition_image_layout(textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copy_buffer_to_image(stagingBuffer, textureImage);

    if (mipLevels > 1)
    {
        generate_mipmaps(textureImage);
    }

    textureImage.create_view(logicalDevice, VK_IMAGE_ASPECT_COLOR_BIT, nullptr);
    textureImage.create_sampler(physicalDevice, logicalDevice, nullptr);

    stagingBuffer.clear(logicalDevice, nullptr);
}

Void SRenderManager::setup_graphics_descriptors()
{
    DynamicArray<DescriptorSetInfo> infos;
    infos.reserve(MAX_FRAMES_IN_FLIGHT);
    const UInt64 descriptorCount = 1;
    for (UInt64 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        DescriptorSetInfo& info = infos.emplace_back();
        info.name = "GraphicsDescriptorSet" + std::to_string(i);
        info.bindings.reserve(descriptorCount);

        VkDescriptorSetLayoutBinding& uniform = info.bindings.emplace_back();
        uniform.binding = 0;
        uniform.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uniform.descriptorCount = 1;
        uniform.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        // VkDescriptorSetLayoutBinding& image = info.bindings.emplace_back();
        // image.binding = 1;
        // image.descriptorCount = 1;
        // image.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        // image.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        // VkDescriptorSetLayoutBinding& storage1 = info.bindings.emplace_back();
        // storage1.binding = 2;
        // storage1.descriptorCount = 1;
        // storage1.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        // storage1.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        //
        // VkDescriptorSetLayoutBinding& storage2 = info.bindings.emplace_back();
        // storage2.binding = 3;
        // storage2.descriptorCount = 1;
        // storage2.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        // storage2.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }
    

    descriptorPool.create(infos, logicalDevice, nullptr);


    DynamicArray<DescriptorSetupInfo> setupInfos;
    setupInfos.reserve(MAX_FRAMES_IN_FLIGHT);
    for (UInt64 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        setupInfos.emplace_back().resources.reserve(descriptorCount);
        setupInfos[i].dataHandle = descriptorPool.get_set_data_handle_by_name("GraphicsDescriptorSet" + std::to_string(i));
        DescriptorResourceInfo& uniformBufferResource = setupInfos[i].resources.emplace_back();
        VkDescriptorBufferInfo uniformBufferInfo;
        uniformBufferInfo.buffer = uniformBuffers[i].get_buffer();
        uniformBufferInfo.offset = 0;
        uniformBufferInfo.range  = sizeof(UniformBufferObject);
        uniformBufferResource.bufferInfo = uniformBufferInfo;

        // DescriptorResourceInfo& imageResource = setupInfos[i].resources.emplace_back();
        // VkDescriptorImageInfo imageInfo;
        // imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        // imageInfo.imageView   = nullptr;
        // imageInfo.sampler     = nullptr;
        // imageResource.imageInfo = imageInfo;

        //DescriptorResourceInfo& storageBuffer1Resource = setupInfos[i].resources.emplace_back();
        //VkDescriptorBufferInfo& storageBufferInfoLastFrame = storageBuffer1Resource.bufferInfo.value();
        //storageBufferInfoLastFrame.buffer = shaderStorageBuffers[(i - 1) % MAX_FRAMES_IN_FLIGHT];
        //storageBufferInfoLastFrame.offset = 0;
        //storageBufferInfoLastFrame.range  = sizeof(Particle) * PARTICLE_COUNT;
        //
        //DescriptorResourceInfo& storageBuffer2Resource = setupInfos[i].resources.emplace_back();
        //VkDescriptorBufferInfo& storageBufferInfoCurrentFrame = storageBuffer2Resource.bufferInfo.value();
        //storageBufferInfoCurrentFrame.buffer = shaderStorageBuffers[i];
        //storageBufferInfoCurrentFrame.offset = 0;
        //storageBufferInfoCurrentFrame.range  = sizeof(Particle) * PARTICLE_COUNT;
    }

    descriptorPool.setup_sets(setupInfos, logicalDevice);
}

Void SRenderManager::create_synchronization_objects()
{
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (UInt64 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        if (vkCreateSemaphore(logicalDevice.get_device(), &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(logicalDevice.get_device(), &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(logicalDevice.get_device(), &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS)
        {

            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }
}

Void SRenderManager::update_uniform_buffer(UInt32 currentImage, Camera& camera)
{
    UniformBufferObject ubo{};
    ubo.model = FMatrix4(1.0f);
    const FMatrix4 view = camera.get_view();
    FMatrix4 proj = camera.get_projection(SDisplayManager::get().get_aspect_ratio());
    proj[1][1] *= -1.0f; // Invert Y axis
    ubo.viewProjection = proj * view;
    memcpy(*uniformBuffers[currentImage].get_mapped_memory(), &ubo, sizeof(ubo));
}

Void SRenderManager::record_command_buffer(VkCommandBuffer commandBuffer, UInt32 imageIndex, const DynamicArray<Mesh>& meshes)
{
    UInt64 currentFrame = isFrameEven ? 1 : 0;
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags            = 0; // Optional
    beginInfo.pInheritanceInfo = nullptr; // Optional

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    VkRenderPassBeginInfo renderPassInfo{};
    std::array<VkClearValue, 3> clearValues{};
    clearValues[0].color        = { {0.0f, 0.0f, 0.0f, 1.0f} };
    clearValues[1].color        = { {0.0f, 0.0f, 0.0f, 1.0f} };
    clearValues[2].depthStencil = { 1.0f, 0 };
    const UVector2& extent = swapchain.get_extent();
    renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass        = renderPass.get_render_pass();
    renderPassInfo.framebuffer       = framebuffers[imageIndex];
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = VkExtent2D{ extent.x, extent.y };
    renderPassInfo.clearValueCount   = UInt32(clearValues.size());
    renderPassInfo.pClearValues      = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.get_pipeline());

    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = Float32(extent.x);
    viewport.height   = Float32(extent.y);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = { extent.x, extent.y };
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    VkDescriptorSet set = descriptorPool.get_set_by_name("GraphicsDescriptorSet" + std::to_string(currentFrame));
    vkCmdBindDescriptorSets(commandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            graphicsPipeline.get_layout(),
                            0,
                            1,
                            &set,
                            0,
                            nullptr);

    for (const Mesh& mesh : meshes)
    {
        const VkBuffer vertexBuffer = get_buffer_by_handle(mesh.vertexesHandle).get_buffer();
        const VkBuffer indexBuffer = get_buffer_by_handle(mesh.indexesHandle).get_buffer();
        const UInt64 normalsOffset = mesh.positions.size() * sizeof(mesh.positions[0]);
        const UInt64 uvsOffset = normalsOffset + mesh.normals.size() * sizeof(mesh.normals[0]);
        Array<VkBuffer, 3> vertexBuffers = { vertexBuffer, vertexBuffer, vertexBuffer };
        Array<VkDeviceSize, 3> offsets = { 0, normalsOffset, uvsOffset };
        vkCmdBindVertexBuffers(commandBuffer, 0, 3, vertexBuffers.data(), offsets.data());
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);


        vkCmdDrawIndexed(commandBuffer,
                         UInt32(mesh.indexes.size()),
                         1,
                         0,
                         0,
                         0);
    }

    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to record command buffer!");
    }
}

Void SRenderManager::recreate_swapchain()
{
    SDisplayManager& displayManager = SDisplayManager::get();
    IVector2 windowSize = displayManager.get_framebuffer_size();
    while (windowSize.x < 1 || windowSize.y < 1)
    {
        windowSize = displayManager.get_framebuffer_size();
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(logicalDevice.get_device());

    swapchain.clear(logicalDevice, nullptr);
    get_image_by_handle(colorImageHandle).clear(logicalDevice, nullptr);
    get_image_by_handle(depthImageHandle).clear(logicalDevice, nullptr);

    swapchain.create(logicalDevice, physicalDevice, surface, nullptr);
    create_color_image();
    create_depth_image();
    create_framebuffers();
}

Void SRenderManager::generate_mipmaps(Image& image)
{
    // Check if image format supports linear blitting
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(physicalDevice.get_device(), image.get_format(), &formatProperties);

    if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
    {
        throw std::runtime_error("texture image format does not support linear blitting!");
    }

    VkCommandBuffer commandBuffer;
    begin_quick_commands(commandBuffer);

    VkImageMemoryBarrier barrier{};
    barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image                           = image.get_image();
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;
    barrier.subresourceRange.levelCount     = 1;

	const UVector2 &size = image.get_size();
    Int32 mipWidth  = size.x;
    Int32 mipHeight = size.y;
    const UInt32 mipLevels = UInt32(image.get_mip_levels());

    for (UInt32 i = 1; i < mipLevels; ++i)
    {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout                     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout                     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask                 = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask                 = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 
                             0,
                             0, 
                             nullptr,
                             0, 
                             nullptr,
                             1, 
                             &barrier);

        VkImageBlit blit{};
        blit.srcOffsets[0]                 = { 0, 0, 0 };
        blit.srcOffsets[1]                 = { mipWidth, mipHeight, 1 };
        blit.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel       = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount     = 1;
        blit.dstOffsets[0]                 = { 0, 0, 0 };
        blit.dstOffsets[1]                 = 
        {
        	mipWidth > 1 ? mipWidth / 2 : 1,
        	mipHeight > 1 ? mipHeight / 2 : 1,
        	1
        };
        blit.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel       = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount     = 1;

        vkCmdBlitImage(commandBuffer,
                       image.get_image(), 
                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       image.get_image(), 
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1, 
                       &blit,
                       VK_FILTER_LINEAR);

        barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0,
                             0, 
                             nullptr,
                             0, 
                             nullptr,
                             1, 
                             &barrier);

        if (mipWidth > 1)
        {
            mipWidth /= 2;
        }
        if (mipHeight > 1)
        {
            mipHeight /= 2;
        }

    }

    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 
                         0,
                         0, 
                         nullptr,
                         0, 
                         nullptr,
                         1, 
                         &barrier);

    end_quick_commands(commandBuffer);
}

Void SRenderManager::copy_buffer_to_image(const Buffer& buffer, Image& image)
{
    VkCommandBuffer commandBuffer;
    begin_quick_commands(commandBuffer);

    const UVector2& size = image.get_size();
    VkBufferImageCopy region{};
    region.bufferOffset                    = 0;
    region.bufferRowLength                 = 0;
    region.bufferImageHeight               = 0;
    region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel       = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount     = 1;
    region.imageOffset                     = { 0, 0, 0 };
    region.imageExtent                     = { size.x, size.y, 1 };

    vkCmdCopyBufferToImage(commandBuffer,
                           buffer.get_buffer(),
                           image.get_image(),
                           image.get_current_layout(),
                           1,
                           &region);

    end_quick_commands(commandBuffer);
}

Void SRenderManager::transition_image_layout(Image& image, VkImageLayout newLayout)
{
    VkCommandBuffer commandBuffer;
    begin_quick_commands(commandBuffer);

    const VkImageLayout oldLayout = image.get_current_layout();
    image.set_current_layout(newLayout);
    VkImageMemoryBarrier barrier{};
    barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout                       = oldLayout;
    barrier.newLayout                       = newLayout;
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.image                           = image.get_image();

    if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) 
    {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (has_stencil_component(image.get_format()))
        {
            barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    } else {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = UInt32(image.get_mip_levels());
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) 
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) 
    {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage      = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) 
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    } else {
        throw std::invalid_argument("unsupported layout transition!");
    }

    vkCmdPipelineBarrier(commandBuffer,
                         sourceStage,
                         destinationStage,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &barrier);

    end_quick_commands(commandBuffer);
}

Void SRenderManager::copy_buffer(const Buffer& source, Buffer& destination)
{
    VkCommandBuffer commandBuffer;
    begin_quick_commands(commandBuffer);

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0; // Optional
    copyRegion.dstOffset = 0; // Optional
    copyRegion.size      = source.get_size();
    vkCmdCopyBuffer(commandBuffer, source.get_buffer(), destination.get_buffer(), 1, &copyRegion);

    end_quick_commands(commandBuffer);
}

Void SRenderManager::begin_quick_commands(VkCommandBuffer& commandBuffer)
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool        = commandPool;
    allocInfo.commandBufferCount = 1;
    
    vkAllocateCommandBuffers(logicalDevice.get_device(), &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
}

Void SRenderManager::end_quick_commands(VkCommandBuffer commandBuffer)
{
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &commandBuffer;

    vkQueueSubmit(logicalDevice.get_graphics_queue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(logicalDevice.get_graphics_queue());

    vkFreeCommandBuffers(logicalDevice.get_device(), commandPool, 1, &commandBuffer);
}

Bool SRenderManager::has_stencil_component(VkFormat format)
{
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

Void SRenderManager::shutdown()
{
    SPDLOG_INFO("Wait until frame end...");
    vkDeviceWaitIdle(logicalDevice.get_device());
    SPDLOG_INFO("Render Manager shutdown.");

    for(Image& image : images)
    {
        image.clear(logicalDevice, nullptr);
    }

    for (Buffer& buffer : uniformBuffers)
    {
        buffer.clear(logicalDevice, nullptr);
    }

    for (Buffer& buffer : buffers)
    {
        buffer.clear(logicalDevice, nullptr);
    }

    vkDestroyCommandPool(logicalDevice.get_device(), commandPool, nullptr);
    descriptorPool.clear(logicalDevice, nullptr);
    computePipeline.clear(logicalDevice, nullptr);
    graphicsPipeline.clear(logicalDevice, nullptr);
    renderPass.clear(logicalDevice, nullptr);
    swapchain.clear(logicalDevice, nullptr);


    for (UInt64 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        vkDestroySemaphore(logicalDevice.get_device(), renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(logicalDevice.get_device(), imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(logicalDevice.get_device(), inFlightFences[i], nullptr);
    }

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
