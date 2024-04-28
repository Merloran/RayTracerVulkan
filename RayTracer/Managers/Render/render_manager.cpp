#include "render_manager.hpp"


#include "../Display/display_manager.hpp"
#include "../Resource/resource_manager.hpp"
#include "../Resource/Common/handle.hpp"
#include "../Resource/Common/material.hpp"
#include "../Resource/Common/texture.hpp"
#include "../Resource/Common/mesh.hpp"
#include "../Resource/Common/model.hpp"
#include "../Raytrace/raytrace_manager.hpp"
#include "Camera/camera.hpp"
#include "Common/command_buffer.hpp"
#include "Common/shader.hpp"
#include "Common/image.hpp"

#include <filesystem>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <magic_enum.hpp>


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
    
    create_dynamic_buffer<UniformBufferObject>(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    
    create_graphics_descriptors();
    graphicsPool = create_command_pool(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    create_command_buffers(get_command_pool_by_handle(graphicsPool),
                           VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                           { "Graphics" });
    create_synchronization_objects();

    //Shaders should be created after logical device
    Handle<Shader> vert = load_shader(SHADERS_PATH + "Shader.vert", EShaderType::Vertex);
    Handle<Shader> frag = load_shader(SHADERS_PATH + "Shader.frag", EShaderType::Fragment);
    DynamicArray<Shader> shaders;
    shaders.push_back(get_shader_by_handle(vert));
    shaders.push_back(get_shader_by_handle(frag));
    swapchain.create(logicalDevice, physicalDevice, surface, nullptr);
    renderPass.create(physicalDevice, logicalDevice, swapchain, nullptr, physicalDevice.get_max_samples());

    graphicsPipeline.create_graphics_pipeline(descriptorPool, renderPass, shaders, logicalDevice, nullptr);
}

Void SRenderManager::setup_imgui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (Void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls


    imguiPool = create_command_pool(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    create_command_buffers(get_command_pool_by_handle(imguiPool),
                           VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                           { "ImGui" });

    Handle<Shader> vert = load_shader(SHADERS_PATH + "Shader.vert", EShaderType::Vertex);
    Handle<Shader> frag = load_shader(SHADERS_PATH + "Shader.frag", EShaderType::Fragment);
    DynamicArray<Shader> shaders;
    shaders.push_back(get_shader_by_handle(vert));
    shaders.push_back(get_shader_by_handle(frag));
    imguiPass.create(physicalDevice, logicalDevice, swapchain, nullptr, VK_SAMPLE_COUNT_1_BIT, true, VK_ATTACHMENT_LOAD_OP_DONT_CARE);
    imguiPipeline.create_graphics_pipeline(descriptorPool, imguiPass, shaders, logicalDevice, nullptr);

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    const Array<VkDescriptorPoolSize, 1> poolSizes =
    {
        {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}},
    };
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets       = 1;
    poolInfo.poolSizeCount = UInt32(poolSizes.size());
    poolInfo.pPoolSizes    = poolSizes.data();
    VkResult error = vkCreateDescriptorPool(logicalDevice.get_device(), &poolInfo, nullptr, &imguiDescriptorPool);
    s_check_vk_result(error);
    
    const VkSurfaceCapabilitiesKHR capabilities = physicalDevice.get_capabilities(surface);
    ImGui_ImplGlfw_InitForVulkan(&SDisplayManager::get().get_window(), true);
    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance        = instance;
    initInfo.PhysicalDevice  = physicalDevice.get_device();
    initInfo.Device          = logicalDevice.get_device();
    initInfo.QueueFamily     = physicalDevice.get_graphics_family_index();
    initInfo.Queue           = logicalDevice.get_graphics_queue();
    initInfo.PipelineCache   = imguiPipeline.get_cache();
    initInfo.DescriptorPool  = imguiDescriptorPool;
    initInfo.Subpass         = 0;
    initInfo.MinImageCount   = capabilities.minImageCount;
    initInfo.ImageCount      = capabilities.minImageCount + 1;
    initInfo.MSAASamples     = imguiPass.get_samples();
    initInfo.Allocator       = nullptr;
    initInfo.CheckVkResultFn = s_check_vk_result;
    ImGui_ImplVulkan_Init(&initInfo, imguiPass.get_render_pass());
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
        SPDLOG_ERROR("Command buffer handle {} not found, returned None.", name);
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
        SPDLOG_ERROR("Command buffer {} not found, returned default.", name);
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
        SPDLOG_ERROR("Image {} not found, returned default.", handle.id);
        return images[0];
    }
    return images[handle.id];
}

Buffer& SRenderManager::get_buffer_by_handle(const Handle<Buffer> handle)
{
    if (handle.id < 0 || handle.id >= Int32(buffers.size()))
    {
        SPDLOG_ERROR("Buffer {} not found, returned default.", handle.id);
        return buffers[0];
    }
    return buffers[handle.id];
}

VkCommandPool SRenderManager::get_command_pool_by_handle(const Handle<VkCommandPool> handle)
{
    if (handle.id < 0 || handle.id >= Int32(commandPools.size()))
    {
        SPDLOG_ERROR("Command pool {} not found, returned default.", handle.id);
        return commandPools[0];
    }
    return commandPools[handle.id];
}

Handle<Shader> SRenderManager::load_shader(const String& filePath, const EShaderType shaderType, const String& functionName)
{
    const UInt64 shaderId = shaders.size();
    Shader& shader        = shaders.emplace_back();

    const std::filesystem::path path(filePath);
    const String destinationPath = (SHADERS_PATH / path.filename()).string() + COMPILED_SHADER_EXTENSION;
    shader.create(filePath, destinationPath, GLSL_COMPILER_PATH, functionName, shaderType, logicalDevice, nullptr);

    const Handle<Shader> handle{Int32(shaderId)};
    auto iterator = nameToIdShaders.find(shader.get_name());
    if (iterator != nameToIdShaders.end())
    {
        SPDLOG_WARN("Shader {} already exists.", shader.get_name());
        shader.clear(logicalDevice, nullptr);
        return iterator->second;
    }

    nameToIdShaders[shader.get_name()] = handle;

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
    mesh.positionsHandle = create_static_buffer(mesh.positions, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    mesh.normalsHandle   = create_static_buffer(mesh.normals, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    mesh.uvsHandle       = create_static_buffer(mesh.uvs, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    mesh.indexesHandle   = create_static_buffer(mesh.indexes, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
}

Void SRenderManager::generate_texture_images(DynamicArray<Texture>& textures)
{
    for (Texture& texture : textures)
    {
        UInt32 mipLevel = UInt32(std::floor(std::log2(std::max(texture.size.x, texture.size.y)))) + 1;
        create_texture_image(texture, mipLevel);
    }
}

Void SRenderManager::create_texture_image(Texture& texture, UInt32 mipLevels)
{
    Buffer stagingBuffer{};
    UInt64 textureSize = UInt64(texture.size.x * texture.size.y * texture.channels);
    if (texture.type == ETextureType::HDR)
    {
        textureSize *= sizeof(Float32);
    }
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

    
	if (texture.channels != 4)
    {
        SPDLOG_ERROR("Not supported channels count: {} in texture: {}", texture.channels, texture.name);
        stagingBuffer.clear(logicalDevice, nullptr);
        return;
    }
    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
    if (texture.type == ETextureType::HDR)
    {
        format = VK_FORMAT_R32G32B32A32_SFLOAT;
    }


    texture.image.id = Int32(images.size());
    Image& textureImage = images.emplace_back();

    textureImage.create(physicalDevice,
                        logicalDevice,
                        texture.size,
                        mipLevels,
                        VK_SAMPLE_COUNT_1_BIT,
                        format,
                        VK_IMAGE_TILING_OPTIMAL,
                        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                        VK_IMAGE_ASPECT_COLOR_BIT,
                        nullptr);
    textureImage.create_sampler(physicalDevice, logicalDevice, nullptr);

    transition_image_layout(textureImage,
                            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
                            VK_PIPELINE_STAGE_TRANSFER_BIT,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copy_buffer_to_image(stagingBuffer, textureImage);
    if (texture.type != ETextureType::HDR)
    {
        generate_mipmaps(textureImage); // implicit transition to read optimal
    } else {
        transition_image_layout(textureImage,
                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    stagingBuffer.clear(logicalDevice, nullptr);
}

Handle<Image> SRenderManager::create_image(const UVector2& size, VkFormat format, VkImageUsageFlags usage, VkImageTiling tiling, UInt32 mipLevels)
{
	const Handle<Image> handle = { Int32(images.size()) };
    Image& image = images.emplace_back();

    image.create(physicalDevice,
                 logicalDevice,
                 size,
                 mipLevels,
                 VK_SAMPLE_COUNT_1_BIT,
                 format,
                 tiling,
                 usage,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 VK_IMAGE_ASPECT_COLOR_BIT,
                 nullptr);

    image.create_sampler(physicalDevice, logicalDevice, nullptr);

    return handle;
}

Void SRenderManager::resize_image(const UVector2& newSize, Handle<Image> image)
{
    get_image_by_handle(image).resize(physicalDevice, logicalDevice, newSize, nullptr);
}

Void SRenderManager::setup_graphics_descriptors(const DynamicArray<Texture>& textures)
{
    DynamicArray<DescriptorResourceInfo> uniformResources;
    VkDescriptorBufferInfo& uniformBufferInfo = uniformResources.emplace_back().bufferInfos.emplace_back();
    uniformBufferInfo.buffer = dynamicBuffers[0].get_buffer();
    uniformBufferInfo.offset = 0;
    uniformBufferInfo.range  = sizeof(UniformBufferObject);

    descriptorPool.add_set(descriptorPool.get_layout_data_handle_by_name("CameraDataLayout"),
                           uniformResources,
						   "GraphicsDescriptorSet");

    DynamicArray<DescriptorResourceInfo> resources;
    DynamicArray<VkDescriptorImageInfo>& imageInfos = resources.emplace_back().imageInfos;
    imageInfos.reserve(textures.size());
    for (const Texture& texture : textures)
    {
        Image& image = get_image_by_handle(texture.image);
        VkDescriptorImageInfo& imageInfo = imageInfos.emplace_back();
        imageInfo.imageLayout = image.get_current_layout();
        imageInfo.imageView   = image.get_view();
        imageInfo.sampler     = image.get_sampler();
    }

    descriptorPool.add_set(descriptorPool.get_layout_data_handle_by_name("TexturesDataLayout"),
                           resources,
                           "Textures");


    descriptorPool.create_sets(logicalDevice, nullptr);
}

Void SRenderManager::reload_shaders()
{
    Bool result = true;
    result &= get_shader_by_name("VShader").recreate(GLSL_COMPILER_PATH, logicalDevice, nullptr);
    result &= get_shader_by_name("FShader").recreate(GLSL_COMPILER_PATH, logicalDevice, nullptr);

    if (!result)
    {
        SPDLOG_ERROR("Failed to reload shaders.");
        return;
    }
    DynamicArray<Shader> shaders;
    shaders.emplace_back(get_shader_by_name("VShader"));
    shaders.emplace_back(get_shader_by_name("FShader"));

    logicalDevice.wait_idle();
    graphicsPipeline.recreate_pipeline(descriptorPool, renderPass, shaders, logicalDevice, nullptr);
}

Void SRenderManager::update_imgui()
{
    SRaytraceManager& raytraceManager = SRaytraceManager::get();
    // Start the Dear ImGui frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();


    ImGui::Begin("Config");

    ImGui::Text("This is some useful text.");

    ImGui::DragInt("Frame limit", &raytraceManager.frameLimit);
    ImGui::SliderInt("Max bounces", &raytraceManager.maxBouncesCount, 0, 32);

    if (ImGui::Button("Reload Shaders"))
    {
        reload_shaders();
    }

    ImGui::Checkbox("Raytrace enabled", &raytraceManager.isEnabled);

    ImGuiIO& io = ImGui::GetIO(); (Void)io;
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
    ImGui::End();

    ImGui::Render();
}

Void SRenderManager::render_imgui()
{
    SRaytraceManager& raytraceManager = SRaytraceManager::get();

    CommandBuffer commandBuffer = get_command_buffer_by_name("ImGui");
    commandBuffer.reset(0);
    
    const UVector2& extent = swapchain.get_extent();

    commandBuffer.begin();
    commandBuffer.begin_render_pass(imguiPass, swapchain, swapchain.get_image_index(), VK_SUBPASS_CONTENTS_INLINE);
    commandBuffer.bind_pipeline(imguiPipeline);

    commandBuffer.set_viewport(0, { 0.0f, 0.0f }, extent, { 0.0f, 1.0f });
    commandBuffer.set_scissor(0, { 0, 0 }, extent);

    //IMGUI
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer.get_buffer());

    commandBuffer.end_render_pass();
    commandBuffer.end();

    if (raytraceManager.isEnabled)
    {
        logicalDevice.submit_graphics_queue(raytraceManager.renderFinished,
                                            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                            commandBuffer.get_buffer(),
                                            imguiFinished,
                                            imguiInFlight);

        VkResult result = logicalDevice.submit_present_queue({ imguiFinished }, swapchain);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || SDisplayManager::get().was_resize_handled())
        {
            recreate_swapchain(swapchain, renderPass);
            return;
        }
    } else {
        logicalDevice.submit_graphics_queue(renderFinished,
                                            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                            commandBuffer.get_buffer(),
                                            imguiFinished,
                                            imguiInFlight);

        VkResult result = logicalDevice.submit_present_queue({ imguiFinished, imageAvailable }, swapchain);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || SDisplayManager::get().was_resize_handled())
        {
            recreate_swapchain(swapchain, renderPass);
            return;
        }
    }

    logicalDevice.wait_for_fence(imguiInFlight, true);
    logicalDevice.reset_fence(imguiInFlight);
}

Void SRenderManager::render(Camera& camera, const DynamicArray<Model>& models, Float32 time)
{
    CommandBuffer commandBuffer = get_command_buffer_by_name("Graphics");
    SResourceManager& resourceManager = SResourceManager::get();
    const UVector2& extent = swapchain.get_extent();
    VkResult result = logicalDevice.acquire_next_image(swapchain, imageAvailable);
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        recreate_swapchain(swapchain, renderPass);
        return;
    }

    {
        UniformBufferObject ubo{};
        ubo.time = time;
        const FMatrix4 view = camera.get_view();
        FMatrix4 proj = camera.get_projection(SDisplayManager::get().get_aspect_ratio());
        proj[1][1] *= -1.0f; // Invert Y axis
        ubo.viewProjection = proj * view;
        update_dynamic_buffer(ubo, dynamicBuffers[0]);
    }

    commandBuffer.reset(0);
    commandBuffer.begin();
    commandBuffer.begin_render_pass(renderPass, swapchain, swapchain.get_image_index(), VK_SUBPASS_CONTENTS_INLINE);
    commandBuffer.bind_pipeline(graphicsPipeline);
    
    commandBuffer.set_viewport(0, { 0.0f, 0.0f }, extent, { 0.0f, 1.0f });
    commandBuffer.set_scissor(0, { 0, 0 }, extent);

    const DescriptorSetData& uniformSet = descriptorPool.get_set_data_by_name("GraphicsDescriptorSet");
    commandBuffer.bind_descriptor_set(graphicsPipeline, uniformSet.set, uniformSet.setNumber);

    const DescriptorSetData& textureSet = descriptorPool.get_set_data_by_name("Textures");
    commandBuffer.bind_descriptor_set(graphicsPipeline, textureSet.set, textureSet.setNumber);

    for (const Model& model : models)
    {
        for (UInt64 i = 0; i < model.meshes.size(); ++i)
        {
            const Mesh& mesh = resourceManager.get_mesh_by_handle(model.meshes[i]);
            const Material& material = resourceManager.get_material_by_handle(model.materials[i]);

            const VkBuffer positionsBuffer = get_buffer_by_handle(mesh.positionsHandle).get_buffer();
            const VkBuffer normalsBuffer = get_buffer_by_handle(mesh.normalsHandle).get_buffer();
            const VkBuffer uvsBuffer = get_buffer_by_handle(mesh.uvsHandle).get_buffer();
            const VkBuffer indexBuffer = get_buffer_by_handle(mesh.indexesHandle).get_buffer();

            Array<VkBuffer, 3> vertexBuffers = { positionsBuffer, normalsBuffer, uvsBuffer };
            Array<VkDeviceSize, 3> offsets = { 0, 0, 0 };
            commandBuffer.bind_vertex_buffers(0, vertexBuffers, offsets);
            commandBuffer.bind_index_buffer(indexBuffer, 0, VK_INDEX_TYPE_UINT32);

            VertexConstants vertexConstants{};
            vertexConstants.model = FMatrix4(1.0f);
            FragmentConstants fragmentConstants{};
            fragmentConstants.albedoId = material.textures[UInt64(ETextureType::Albedo)].id;
            
            commandBuffer.set_constants(graphicsPipeline, 
                                        VK_SHADER_STAGE_VERTEX_BIT, 
                                        0, 
                                        sizeof(vertexConstants),
                                        &vertexConstants);

            commandBuffer.set_constants(graphicsPipeline, 
                                        VK_SHADER_STAGE_FRAGMENT_BIT, 
                                        sizeof(vertexConstants),
                                        sizeof(fragmentConstants),
                                        &fragmentConstants);

            commandBuffer.draw_indexed(UInt32(mesh.indexes.size()),
                                       1,
                                       0,
                                       0,
                                       0);
        }
    }

    commandBuffer.end_render_pass();
    commandBuffer.end();

    
    logicalDevice.submit_graphics_queue(VK_NULL_HANDLE,
										0,
                                        commandBuffer.get_buffer(),
                                        renderFinished,
                                        inFlightFence);

    isFrameEven = !isFrameEven;
    logicalDevice.wait_for_fence(inFlightFence, true);
    logicalDevice.reset_fence(inFlightFence);
}

VkSurfaceKHR SRenderManager::get_surface() const
{
    return surface;
}

const PhysicalDevice& SRenderManager::get_physical_device() const
{
    return physicalDevice;
}

const LogicalDevice& SRenderManager::get_logical_device() const
{
    return logicalDevice;
}

Swapchain& SRenderManager::get_swapchain()
{
    return swapchain;
}

DescriptorPool& SRenderManager::get_pool()
{
    return descriptorPool;
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

Handle<VkCommandPool> SRenderManager::create_command_pool(VkCommandPoolCreateFlagBits flags)
{
    Handle<VkCommandPool> handle = { Int32(commandPools.size()) };
    VkCommandPool pool;
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags            = flags;
    poolInfo.queueFamilyIndex = physicalDevice.get_graphics_family_index();

    const VkResult result = vkCreateCommandPool(logicalDevice.get_device(), &poolInfo, nullptr, &pool);
    if (result != VK_SUCCESS)
    {
        SPDLOG_ERROR("Creating command pool failed with: {}", magic_enum::enum_name(result));
        return Handle<VkCommandPool>::sNone;
    }

    commandPools.push_back(pool);
    return handle;
}

Void SRenderManager::create_command_buffers(Handle<VkCommandPool> handle, VkCommandBufferLevel level, const DynamicArray<String>& names)
{
    create_command_buffers(get_command_pool_by_handle(handle), level, names);
}

Void SRenderManager::create_command_buffers(VkCommandPool pool, VkCommandBufferLevel level, const DynamicArray<String>& names)
{
    for (const String& name : names)
    {
        const auto& iterator = nameToIdCommandBuffers.find(name);
        if (iterator != nameToIdCommandBuffers.end())
        {
            SPDLOG_ERROR("Failed to create command buffers, name: {} already exist.", name);
            return;
        }
    }
    commandBuffers.reserve(names.size());
    DynamicArray<VkCommandBuffer> buffers;
    buffers.resize(names.size());
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = pool;
    allocInfo.level              = level;
    allocInfo.commandBufferCount = UInt32(buffers.size());
    
    const VkResult result = vkAllocateCommandBuffers(logicalDevice.get_device(), &allocInfo, buffers.data());
    if (result != VK_SUCCESS)
    {
        SPDLOG_ERROR("Creating command buffers failed with: {}", magic_enum::enum_name(result));
        return;
    }

    for (UInt64 i = 0; i < names.size(); ++i)
    {
	    const Handle<CommandBuffer> handle = { Int32(commandBuffers.size()) };
        nameToIdCommandBuffers[names[i]] = handle;
        CommandBuffer& buffer = commandBuffers.emplace_back();
        buffer.set_buffer(buffers[i]);
        buffer.set_name(names[i]);
    }
}

Void SRenderManager::recreate_swapchain(Swapchain& swapchain, RenderPass& renderPass)
{
    SDisplayManager& displayManager = SDisplayManager::get();
    IVector2 windowSize = displayManager.get_framebuffer_size();
    while (windowSize.x < 1 || windowSize.y < 1)
    {
        windowSize = displayManager.get_framebuffer_size();
        glfwWaitEvents();
    }

    logicalDevice.wait_idle();

    swapchain.clear(logicalDevice, nullptr);
    swapchain.create(logicalDevice, physicalDevice, surface, nullptr);

    renderPass.clear_framebuffers(logicalDevice, nullptr);
    renderPass.clear_images(logicalDevice, nullptr);
    renderPass.create_attachments(physicalDevice, logicalDevice, swapchain, nullptr);
    renderPass.create_framebuffers(logicalDevice, swapchain, nullptr);
}

Void SRenderManager::create_graphics_descriptors()
{
    SResourceManager& resourceManager = SResourceManager::get();
    descriptorPool.add_binding("TexturesDataLayout",
                               1,
                               0,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               UInt32(resourceManager.get_textures().size()),
                               VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
                               VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
                               VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
                               VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);
    
    descriptorPool.add_binding("CameraDataLayout",
                               0,
                               0,
                               VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                               1,
                               VK_SHADER_STAGE_VERTEX_BIT,
                               VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
                               VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
                               VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);
    

    descriptorPool.create_layouts(logicalDevice, nullptr);

    DynamicArray<VkPushConstantRange> pushConstants;
    VkPushConstantRange& vertexConstant = pushConstants.emplace_back();
    vertexConstant.size       = sizeof(VertexConstants);
    vertexConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPushConstantRange& fragmentConstant = pushConstants.emplace_back();
    fragmentConstant.size       = sizeof(FragmentConstants);
    fragmentConstant.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    descriptorPool.set_push_constants(pushConstants);
}

Void SRenderManager::create_synchronization_objects()
{
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    
    if (vkCreateSemaphore(logicalDevice.get_device(), &semaphoreInfo, nullptr, &imageAvailable) != VK_SUCCESS ||
        vkCreateSemaphore(logicalDevice.get_device(), &semaphoreInfo, nullptr, &renderFinished) != VK_SUCCESS ||
        vkCreateSemaphore(logicalDevice.get_device(), &semaphoreInfo, nullptr, &imguiFinished) != VK_SUCCESS ||
        vkCreateFence(logicalDevice.get_device(), &fenceInfo, nullptr, &imguiInFlight) != VK_SUCCESS ||
        vkCreateFence(logicalDevice.get_device(), &fenceInfo, nullptr, &inFlightFence) != VK_SUCCESS)
    {

        throw std::runtime_error("failed to create synchronization objects for a frame!");
    }
}

Void SRenderManager::generate_mipmaps(Image& image)
{
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
    const UInt32 mipLevels = image.get_mip_level();

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

    image.set_current_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

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

Void SRenderManager::transition_image_layout(Image& image, VkPipelineStageFlags sourceStage, VkPipelineStageFlags destinationStage, VkImageLayout newLayout)
{
    CommandBuffer commandBuffer;
    VkCommandBuffer buffer;
    begin_quick_commands(buffer);
    commandBuffer.set_buffer(buffer);
    commandBuffer.pipeline_image_barrier(image, sourceStage, destinationStage, newLayout);
    end_quick_commands(buffer);
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

Void SRenderManager::create_quick_commands()
{
    quickPool = create_command_pool(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool        = get_command_pool_by_handle(quickPool);
    allocInfo.commandBufferCount = 1;

    quickCommandBuffer = { Int32(commandBuffers.size()) };
    CommandBuffer &quickBuffer = commandBuffers.emplace_back();
    VkCommandBuffer buffer = quickBuffer.get_buffer();

    const VkResult result = vkAllocateCommandBuffers(logicalDevice.get_device(), &allocInfo, &buffer);
    if (result != VK_SUCCESS)
    {
        SPDLOG_ERROR("Create command buffer failed with: {}", magic_enum::enum_name(result));
        return;
    }
    quickBuffer.set_buffer(buffer);
}

Void SRenderManager::begin_quick_commands(VkCommandBuffer& commandBuffer)
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool        = get_command_pool_by_handle(graphicsPool);
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

    logicalDevice.submit_graphics_queue({ submitInfo }, VK_NULL_HANDLE);
    logicalDevice.wait_graphics_queue_idle();

    vkFreeCommandBuffers(logicalDevice.get_device(), 
                         get_command_pool_by_handle(graphicsPool), 
                         1, &commandBuffer);
}

Void SRenderManager::s_check_vk_result(VkResult error)
{
    if (error != 0)
    {
        SPDLOG_ERROR("[vulkan] Error: VkResult = {}", magic_enum::enum_name(error));
    }
}

Void SRenderManager::shutdown_imgui()
{
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    vkDestroyDescriptorPool(logicalDevice.get_device(), imguiDescriptorPool, nullptr);
    vkDestroySemaphore(logicalDevice.get_device(), imguiFinished, nullptr);
    vkDestroyFence(logicalDevice.get_device(), imguiInFlight, nullptr);

    imguiPass.clear(logicalDevice, nullptr);
    imguiPipeline.clear(logicalDevice, nullptr);
}

Void SRenderManager::shutdown()
{
    SPDLOG_INFO("Render Manager shutdown.");
    logicalDevice.wait_idle();
    SPDLOG_INFO("Wait until frame end...");

    shutdown_imgui();

    for(Image& image : images)
    {
        image.clear(logicalDevice, nullptr);
    }

    for (Buffer& buffer : dynamicBuffers)
    {
        buffer.clear(logicalDevice, nullptr);
    }

    for (Buffer& buffer : buffers)
    {
        buffer.clear(logicalDevice, nullptr);
    }

    for (VkCommandPool pool : commandPools)
    {
        vkDestroyCommandPool(logicalDevice.get_device(), pool, nullptr);
    }

    descriptorPool.clear(logicalDevice, nullptr);
    graphicsPipeline.clear(logicalDevice, nullptr);
    renderPass.clear(logicalDevice, nullptr);
    swapchain.clear(logicalDevice, nullptr);
    
    vkDestroySemaphore(logicalDevice.get_device(), renderFinished, nullptr);
    vkDestroySemaphore(logicalDevice.get_device(), imageAvailable, nullptr);
    vkDestroyFence(logicalDevice.get_device(), inFlightFence, nullptr);

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
