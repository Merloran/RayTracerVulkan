#include "raytrace_manager.hpp"


#include "../Render/Camera/camera.hpp"
#include "../Render/Common/image.hpp"
#include "../Render/render_manager.hpp"
#include "../Display/display_manager.hpp"
#include "../Render/Common/command_buffer.hpp"
#include "../Resource/resource_manager.hpp"
#include "../Resource/Common/model.hpp"
#include "../Resource/Common/handle.hpp"
#include "../Resource/Common/material.hpp"
#include "../Resource/Common/mesh.hpp"
#include "Common/vertex.hpp"

#include <imgui.h>
#include <magic_enum.hpp>
#include <GLFW/glfw3.h>


SRaytraceManager& SRaytraceManager::get()
{
	static SRaytraceManager instance;
	return instance;
}

Void SRaytraceManager::startup()
{
	SPDLOG_INFO("Raytrace Manager startup.");
	SResourceManager& resourceManager = SResourceManager::get();
	SDisplayManager& displayManager = SDisplayManager::get();
	SRenderManager& renderManager = SRenderManager::get();
	areRaysRegenerated = false;
	isEnabled = false;
	currentImageIndex = 0;
	for (Texture& texture : screenTextures)
	{
		texture.name	 = "Result.png";
		texture.channels = 4;
	}
	rayGeneration = renderManager.load_shader(renderManager.SHADERS_PATH + "RayGeneration.comp", EShaderType::Compute);
	raytrace	  = renderManager.load_shader(renderManager.SHADERS_PATH + "RayTrace.comp", EShaderType::Compute);
	screenV		  = renderManager.load_shader(renderManager.SHADERS_PATH + "Screen.vert", EShaderType::Vertex);
	screenF		  = renderManager.load_shader(renderManager.SHADERS_PATH + "Screen.frag", EShaderType::Fragment);

	renderTime = 0.0f;
	maxBouncesCount = 6;
	frameLimit = 0;
	frameCount = 0;
	backgroundColor = { 0.0f, 0.0f, 0.0f };

	materials.reserve(resourceManager.get_materials().size());
	for (const Material& material : resourceManager.get_materials())
	{
		GPUMaterial& gpuMaterial = materials.emplace_back();
		gpuMaterial.albedo	  = material.textures[UInt64(ETextureType::Albedo)].id;
		gpuMaterial.normal	  = material.textures[UInt64(ETextureType::Normal)].id;

		if (material.textures[UInt64(ETextureType::RM)].id == Handle<Texture>::sNone.id)
		{
			gpuMaterial.roughness = material.textures[UInt64(ETextureType::Roughness)].id;
			gpuMaterial.metalness = material.textures[UInt64(ETextureType::Metalness)].id;
		} else {
			gpuMaterial.roughness = material.textures[UInt64(ETextureType::RM)].id;
			gpuMaterial.metalness = material.textures[UInt64(ETextureType::RM)].id;
		}

		gpuMaterial.emission  = material.textures[UInt64(ETextureType::Emission)].id;
		gpuMaterial.indexOfRefraction  = material.indexOfRefraction;
	}

	UInt64 vertexesSize = 0;
	UInt64 indexesSize = 0;

	const DynamicArray<Model>& models = resourceManager.get_models();
	for (const Model& model : models)
	{
		for (Handle<Mesh> handle : model.meshes)
		{
			const Mesh& mesh = resourceManager.get_mesh_by_handle(handle);
			vertexesSize += mesh.positions.size();
			indexesSize  += mesh.indexes.size();
		}
	}
	vertexes.reserve(vertexesSize);
	indexes.reserve(indexesSize);
	trianglesCount = Int32(indexesSize / 3);
	// Predicting emission triangles count
	emissionTriangles.reserve(glm::max(Int32(trianglesCount * 0.01f), 1));

	for (const Model& model : models)
	{
		for (UInt64 i = 0; i < model.meshes.size(); ++i)
		{
			const Mesh& mesh = resourceManager.get_mesh_by_handle(model.meshes[i]);
			for (UInt64 j = 0; j < mesh.positions.size(); ++j)
			{
				Vertex& vertex = vertexes.emplace_back();
				vertex.position   = mesh.positions[j];
				vertex.normal	  = mesh.normals[j];
				vertex.uv		  = mesh.uvs[j];
				vertex.materialId = model.materials[i].id;
			}
		}
	}

	UInt32 indexesOffset = 0;
	for (const Model& model : models)
	{
		for (const Handle<Mesh> handle : model.meshes)
		{
			const Mesh& mesh = resourceManager.get_mesh_by_handle(handle);
			for (UInt64 j = 0; j < mesh.indexes.size(); ++j)
			{
				UInt32 index = mesh.indexes[j] + indexesOffset;
				const GPUMaterial& gpuMaterial = materials[vertexes[index].materialId];
				if (j % 3 == 0 && gpuMaterial.emission != -1)
				{
					emissionTriangles.emplace_back(UInt32(indexes.size()));
				}
				indexes.emplace_back(index);
			}
			indexesOffset += UInt32(mesh.positions.size());
		}
	}

	bvh.create_tree(vertexes, indexes);

	vertexesHandle			= renderManager.create_static_buffer(vertexes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	indexesHandle			= renderManager.create_static_buffer(indexes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	materialsHandle			= renderManager.create_static_buffer(materials, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	bvhHandle				= renderManager.create_static_buffer(bvh.hierarchy, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	emissionTrianglesHandle = renderManager.create_static_buffer(emissionTriangles, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);


	directionTexture.image = renderManager.create_image(displayManager.get_framebuffer_size(),
														VK_FORMAT_R32G32B32A32_SFLOAT,
														VK_IMAGE_USAGE_STORAGE_BIT,
														VK_IMAGE_TILING_OPTIMAL);
	renderManager.transition_image_layout(renderManager.get_image_by_handle(directionTexture.image),
										  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
										  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
										  VK_IMAGE_LAYOUT_GENERAL);

	accumulationTexture.image = renderManager.create_image(displayManager.get_framebuffer_size(),
														   VK_FORMAT_R32G32B32A32_SFLOAT,
														   VK_IMAGE_USAGE_STORAGE_BIT,
														   VK_IMAGE_TILING_OPTIMAL);
	renderManager.transition_image_layout(renderManager.get_image_by_handle(accumulationTexture.image),
										  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
										  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
										  VK_IMAGE_LAYOUT_GENERAL);
	for (Texture& screenTexture : screenTextures)
	{
		screenTexture.image = renderManager.create_image(displayManager.get_framebuffer_size(),
														 VK_FORMAT_R32G32B32A32_SFLOAT,
														 VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
														 VK_IMAGE_TILING_OPTIMAL);
		renderManager.transition_image_layout(renderManager.get_image_by_handle(screenTexture.image),
											  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
											  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
											  VK_IMAGE_LAYOUT_GENERAL);
	}
	

	imageAvailable		  = renderManager.create_semaphore("raytraceImageAvailable");
	renderFinished		  = renderManager.create_semaphore("raytraceRenderFinished");
	rayGenerationFinished = renderManager.create_semaphore("rayGenerationFinished");
	raytraceFinished	  = renderManager.create_semaphore("raytraceFinished");
	renderInFlight		  = renderManager.create_fence("raytraceRenderInFlight");
	raytraceInFlight	  = renderManager.create_fence("raytraceInFlight", VK_FENCE_CREATE_SIGNALED_BIT);
	create_quad_buffers();
	create_descriptors();
	create_pipelines();
	setup_descriptors();
	raytraceCommandPool = renderManager.create_command_pool(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
	renderManager.create_command_buffers(raytraceCommandPool,
										 VK_COMMAND_BUFFER_LEVEL_PRIMARY, 
										 { "RayGenerationBuffer" ,"RaytraceBuffer", "RenderBuffer" });
	rayGenerationBuffer = renderManager.get_command_buffer_handle_by_name("RayGenerationBuffer");
	raytraceBuffer		= renderManager.get_command_buffer_handle_by_name("RaytraceBuffer");
	renderBuffer		= renderManager.get_command_buffer_handle_by_name("RenderBuffer");
}

Void SRaytraceManager::update(Camera &camera, Float32 &deltaTime, Float32 &currentFrame, Float32 &lastFrame)
{
	SDisplayManager &displayManager = SDisplayManager::get();
	SRenderManager &renderManager = SRenderManager::get();

	const IVector2& size = displayManager.get_framebuffer_size();

	renderTime += deltaTime;
	const Bool hasWindowResized = accumulationTexture.size != size || shouldRefresh;
	const Bool hasCameraChanged = camera.has_changed();

	if (hasWindowResized)
	{
		shouldRefresh = false;
		resize_images(size);
		if (IVector2(renderManager.get_swapchain().get_extent()) != size)
		{
			renderManager.recreate_swapchain();
		}
	}

	VkResult result = renderManager.get_logical_device().get_fence_status(renderManager.get_fence_by_handle(raytraceInFlight));
	if (result == VK_NOT_READY)
	{
		// SPDLOG_INFO("HELLO :D");
		render();
		return;
	}
	// SPDLOG_WARN("NOT HELLO :(");

	currentFrame = Float32(glfwGetTime());
	deltaTime = currentFrame - lastFrame;
	lastFrame = currentFrame;

	if (hasWindowResized || hasCameraChanged)
	{
		camera.set_camera_changed(false);
		generate_rays(camera);
		frameCount = 0;
		renderTime = 0.0f;
		areRaysRegenerated = true;
	}

	if (frameLimit == 0 || frameCount < frameLimit)
	{
		ray_trace(camera);
		frameCount++;
		currentImageIndex = (currentImageIndex + 1) % screenTextures.size();
		areRaysRegenerated = false;
	}

	render();
}

Void SRaytraceManager::resize_images(const UVector2& size)
{
	SRenderManager& renderManager = SRenderManager::get();
	const LogicalDevice& logicalDevice = renderManager.get_logical_device();
	logicalDevice.wait_compute_queue_idle();
	accumulationTexture.size = size;
	directionTexture.size    = size;
	renderManager.resize_image(size, accumulationTexture.image);
	renderManager.transition_image_layout(renderManager.get_image_by_handle(accumulationTexture.image),
										  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
										  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
										  VK_IMAGE_LAYOUT_GENERAL);

	renderManager.resize_image(size, directionTexture.image);
	renderManager.transition_image_layout(renderManager.get_image_by_handle(directionTexture.image),
										  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
										  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
										  VK_IMAGE_LAYOUT_GENERAL);

	for (const Texture& screenTexture : screenTextures)
	{
		renderManager.resize_image(size, screenTexture.image);
		renderManager.transition_image_layout(renderManager.get_image_by_handle(screenTexture.image),
											  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
											  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
											  VK_IMAGE_LAYOUT_GENERAL);
	}
	

	DescriptorResourceInfo accumulationResource;
	VkDescriptorImageInfo& accumulationInfo = accumulationResource.imageInfos.emplace_back();
	const Image& accumulation = renderManager.get_image_by_handle(accumulationTexture.image);
	accumulationInfo.imageLayout  = accumulation.get_current_layout();
	accumulationInfo.imageView	  = accumulation.get_view();
	accumulationInfo.sampler      = accumulation.get_sampler();

	raytracePool.update_set(renderManager.get_logical_device(), accumulationResource, accumulationImage, 0, 0);

	for (UInt64 i = 0; i < screenTextures.size(); ++i)
	{
		screenTextures[i].size  = size;
		DescriptorResourceInfo screenResource;
		VkDescriptorImageInfo& screenInfo = screenResource.imageInfos.emplace_back();
		const Image& screen = renderManager.get_image_by_handle(screenTextures[i].image);
		screenInfo.imageLayout  = screen.get_current_layout();
		screenInfo.imageView	= screen.get_view();
		screenInfo.sampler      = screen.get_sampler();
		raytracePool.update_set(renderManager.get_logical_device(), screenResource, screenImages[i], 0, 0);
		postprocessPool.update_set(renderManager.get_logical_device(), screenResource, fragmentImages[i], 0, 0);
	}

	DescriptorResourceInfo directionResource;
	VkDescriptorImageInfo& directionInfo = directionResource.imageInfos.emplace_back();
	const Image& direction = renderManager.get_image_by_handle(directionTexture.image);
	directionInfo.imageLayout = direction.get_current_layout();
	directionInfo.imageView	  = direction.get_view();
	directionInfo.sampler     = direction.get_sampler();

	raytracePool.update_set(renderManager.get_logical_device(), directionResource, directionImage, 0, 0);
}

Void SRaytraceManager::generate_rays(Camera& camera)
{
	const SDisplayManager& displayManager = SDisplayManager::get();
	SRenderManager& renderManager = SRenderManager::get();
	const LogicalDevice& logicalDevice = renderManager.get_logical_device();
	const CommandBuffer& commandBuffer = renderManager.get_command_buffer_by_handle(rayGenerationBuffer);
	const UVector2 workGroupsCount = glm::ceil(FVector2(directionTexture.size) / FVector2(WORKGROUP_SIZE));

	const Float32 theta = glm::radians(camera.get_fov());
	const Float32 h = glm::tan(theta * 0.5f);
	FVector2 viewportSize;
	viewportSize.y = 2.0f * h;
	viewportSize.x = viewportSize.y * displayManager.get_aspect_ratio();

	const FVector3 viewportU = Float32(viewportSize.x) * camera.get_right();
	const FVector3 viewportV = Float32(viewportSize.y) * camera.get_up();

	pixelDeltaU = viewportU / Float32(directionTexture.size.x);
	pixelDeltaV = viewportV / Float32(directionTexture.size.y);
	originPixel = camera.get_position() + camera.get_forward() + (pixelDeltaU - viewportU + pixelDeltaV - viewportV) * 0.5f;

	VkFence raytraceFence = renderManager.get_fence_by_handle(raytraceInFlight);
	logicalDevice.wait_for_fence(raytraceFence, true);
	logicalDevice.reset_fence(raytraceFence);
	commandBuffer.reset(0);

	commandBuffer.begin();
	commandBuffer.bind_pipeline(rayGenerationPipeline);

	DescriptorSetData& accumulation = raytracePool.get_set_data_by_handle(accumulationImage);
	DescriptorSetData& direction    = raytracePool.get_set_data_by_handle(directionImage);
	
	commandBuffer.bind_descriptor_set(rayGenerationPipeline, accumulation.set, 3);
	commandBuffer.bind_descriptor_set(rayGenerationPipeline, direction.set, 2);

	RayGenerationConstants constants{};
	constants.cameraPosition = camera.get_position();
	constants.originPixel	 = originPixel;
	constants.pixelDeltaU	 = pixelDeltaU;
	constants.pixelDeltaV	 = pixelDeltaV;
	constants.imageSize		 = directionTexture.size;

	commandBuffer.set_constants(rayGenerationPipeline,
								VK_SHADER_STAGE_COMPUTE_BIT,
								0,
								sizeof(constants),
								&constants);

	commandBuffer.dispatch({ workGroupsCount, 1 });
	
	commandBuffer.pipeline_image_barrier(renderManager.get_image_by_handle(accumulationTexture.image),
										 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
										 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
										 VK_IMAGE_LAYOUT_GENERAL);

	commandBuffer.end();
	
	logicalDevice.submit_compute_queue(VK_NULL_HANDLE,
									   0,
									   commandBuffer.get_buffer(),
									   renderManager.get_semaphore_by_handle(rayGenerationFinished),
									   VK_NULL_HANDLE);
}

Void SRaytraceManager::ray_trace(Camera& camera)
{
	SResourceManager& resourceManager = SResourceManager::get();
	SRenderManager& renderManager = SRenderManager::get();
	const LogicalDevice& logicalDevice = renderManager.get_logical_device();
	const CommandBuffer& commandBuffer = renderManager.get_command_buffer_by_handle(raytraceBuffer);
	const UVector2 workGroupsCount = glm::ceil(FVector2(directionTexture.size) / FVector2(WORKGROUP_SIZE));

	VkFence raytraceFence = renderManager.get_fence_by_handle(raytraceInFlight);
	if (!areRaysRegenerated)
	{
		logicalDevice.wait_for_fence(raytraceFence, true);
		logicalDevice.reset_fence(raytraceFence);
	}
	commandBuffer.reset(0);

	commandBuffer.begin();
	commandBuffer.bind_pipeline(raytracePipeline);

	commandBuffer.pipeline_image_barrier(renderManager.get_image_by_handle(screenTextures[currentImageIndex].image),
										 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
										 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
										 VK_IMAGE_LAYOUT_GENERAL);
	
	DescriptorSetData& screen		= raytracePool.get_set_data_by_handle(screenImages[currentImageIndex]);
	DescriptorSetData& accumulation = raytracePool.get_set_data_by_handle(accumulationImage);
	DescriptorSetData& direction	= raytracePool.get_set_data_by_handle(directionImage);
	DescriptorSetData& textures		= renderManager.get_pool().get_set_data_by_handle(bindlessTextures);
	DescriptorSetData& scene		= raytracePool.get_set_data_by_handle(sceneData);

	commandBuffer.bind_descriptor_set(raytracePipeline, screen.set, screen.setNumber);
	commandBuffer.bind_descriptor_set(raytracePipeline, accumulation.set, accumulation.setNumber);
	commandBuffer.bind_descriptor_set(raytracePipeline, direction.set, direction.setNumber);
	commandBuffer.bind_descriptor_set(raytracePipeline, textures.set, textures.setNumber);
	commandBuffer.bind_descriptor_set(raytracePipeline, scene.set, scene.setNumber);

	RaytraceConstants constants{};
	constants.backgroundColor		 = backgroundColor;
	constants.cameraPosition		 = camera.get_position();
	constants.pixelDeltaU			 = pixelDeltaU;
	constants.pixelDeltaV			 = pixelDeltaV;
	constants.imageSize				 = accumulationTexture.size;
	constants.viewBounds			 = camera.get_view_bounds();
	constants.invFrameCount			 = 1.0f / Float32(frameCount + 1);
	constants.time					 = renderTime;
	constants.frameCount			 = frameCount;
	constants.trianglesCount		 = trianglesCount;
	constants.emissionTrianglesCount = Int32(emissionTriangles.size());
	constants.maxBouncesCount		 = maxBouncesCount;
	constants.rootId				 = bvh.rootId;
	constants.environmentMapId		 = Int32(resourceManager.get_textures().size() - 1ULL);

	commandBuffer.set_constants(raytracePipeline,
								VK_SHADER_STAGE_COMPUTE_BIT,
								0,
								sizeof(constants),
								&constants);

	commandBuffer.dispatch({ workGroupsCount, 1 });

	commandBuffer.pipeline_image_barrier(renderManager.get_image_by_handle(screenTextures[currentImageIndex].image),
										 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
										 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 
										 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	commandBuffer.end();
	VkResult result;
	if (areRaysRegenerated)
	{

		result = logicalDevice.submit_compute_queue(renderManager.get_semaphore_by_handle(rayGenerationFinished),
													VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
													commandBuffer.get_buffer(),
													VK_NULL_HANDLE,
													raytraceFence);
	} else {
		result = logicalDevice.submit_compute_queue(VK_NULL_HANDLE,
													0,
													commandBuffer.get_buffer(),
													VK_NULL_HANDLE,
													raytraceFence);
	}

	if (result != VK_SUCCESS)
	{
		SPDLOG_ERROR("Submit compute queue failed with: {}", magic_enum::enum_name(result));
	}
}

Void SRaytraceManager::render()
{
	SRenderManager& renderManager = SRenderManager::get();
	const LogicalDevice& logicalDevice = renderManager.get_logical_device();
	const CommandBuffer& commandBuffer = renderManager.get_command_buffer_by_handle(renderBuffer);
	Swapchain& swapchain = renderManager.get_swapchain();
	const UVector2& extent = swapchain.get_extent();

	const VkFence imguiFence = renderManager.get_fence_by_name("imguiInFlight");
	logicalDevice.wait_for_fence(imguiFence, true);
	logicalDevice.reset_fence(imguiFence);

	VkSemaphore imageSemaphore = renderManager.get_semaphore_by_handle(imageAvailable);
	VkResult result = logicalDevice.acquire_next_image(swapchain, imageSemaphore);
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        return;
    }

	commandBuffer.reset(0);
	commandBuffer.begin();
	commandBuffer.begin_render_pass(renderManager.get_render_pass_by_handle(postprocessPass), 
									swapchain, 
									swapchain.get_image_index(), 
									VK_SUBPASS_CONTENTS_INLINE);
	commandBuffer.bind_pipeline(postprocessPipeline);

	commandBuffer.set_viewport(0, { 0.0f, 0.0f }, extent, { 0.0f, 1.0f });
	commandBuffer.set_scissor(0, { 0, 0 }, extent);


	const DescriptorSetData& screen = postprocessPool.get_set_data_by_handle(fragmentImages[currentImageIndex]);
	commandBuffer.bind_descriptor_set(postprocessPipeline, screen.set, screen.setNumber);

	const VkBuffer positionsBuffer = renderManager.get_buffer_by_handle(quadPositions).get_buffer();
	const VkBuffer normalsBuffer   = renderManager.get_buffer_by_handle(quadNormals).get_buffer();
	const VkBuffer uvsBuffer	   = renderManager.get_buffer_by_handle(quadUvs).get_buffer();
	const VkBuffer indexBuffer	   = renderManager.get_buffer_by_handle(quadIndexes).get_buffer();

	const Array<VkBuffer, 3> vertexBuffers = { positionsBuffer, normalsBuffer, uvsBuffer };
	const Array<VkDeviceSize, 3> offsets = { 0, 0, 0 };
	commandBuffer.bind_vertex_buffers(0, vertexBuffers, offsets);
	commandBuffer.bind_index_buffer(indexBuffer, 0, VK_INDEX_TYPE_UINT32);

	commandBuffer.draw_indexed(6, 1, 0, 0, 0);

	commandBuffer.end_render_pass();
	commandBuffer.end();


	VkFence renderFence = renderManager.get_fence_by_handle(renderInFlight);
	VkSemaphore renderSemaphore = renderManager.get_semaphore_by_handle(renderFinished);
	logicalDevice.submit_graphics_queue(imageSemaphore,
										VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
										commandBuffer.get_buffer(),
										renderSemaphore,
										renderFence);
}

Void SRaytraceManager::create_pipelines()
{
	SRenderManager& renderManager = SRenderManager::get();
	
	rayGenerationPipeline.create_compute_pipeline(rayGenerationPool, 
												  renderManager.get_shader_by_handle(rayGeneration), 
												  renderManager.get_logical_device(), 
												  nullptr);
	
	raytracePipeline.create_compute_pipeline(raytracePool,
											 renderManager.get_shader_by_handle(raytrace),
											 renderManager.get_logical_device(),
											 nullptr);

	DynamicArray<Shader> shaders;
	shaders.push_back(renderManager.get_shader_by_handle(screenV));
	shaders.push_back(renderManager.get_shader_by_handle(screenF));
	
	postprocessPass = renderManager.create_render_pass(VK_SAMPLE_COUNT_1_BIT, false);
	
	postprocessPipeline.create_graphics_pipeline(postprocessPool, 
												 renderManager.get_render_pass_by_handle(postprocessPass),
											     shaders, 
											     renderManager.get_logical_device(), 
											     nullptr);
}

Void SRaytraceManager::create_descriptors()
{
	SResourceManager& resourceManager = SResourceManager::get();
	const SRenderManager& renderManager = SRenderManager::get();


	rayGenerationPool.add_binding("AccumulationLayout",
								  3,
								  0,
								  VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
								  1,
								  VK_SHADER_STAGE_COMPUTE_BIT,
								  VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
								  VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
								  VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);

	rayGenerationPool.add_binding("DirectionLayout",
								  2,
								  0,
								  VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
								  1,
								  VK_SHADER_STAGE_COMPUTE_BIT,
								  VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
								  VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
								  VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);

	rayGenerationPool.create_layouts(renderManager.get_logical_device(), nullptr);

	DynamicArray<VkPushConstantRange> rayGenerationConstants;
	VkPushConstantRange& rayGeneration = rayGenerationConstants.emplace_back();
	rayGeneration.size = sizeof(RayGenerationConstants);
	rayGeneration.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	rayGenerationPool.set_push_constants(rayGenerationConstants);


	raytracePool.add_binding("ScreenImage",
							 4,
							 0,
							 VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
							 1,
							 VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
							 VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
							 VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
							 VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);

	raytracePool.add_binding("AccumulationLayout",
							 3,
							 0,
							 VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
							 1,
							 VK_SHADER_STAGE_COMPUTE_BIT,
							 VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
							 VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
							 VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);

	raytracePool.add_binding("DirectionLayout",
							 2,
							 0,
							 VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
							 1,
							 VK_SHADER_STAGE_COMPUTE_BIT,
							 VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
							 VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
							 VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);

	raytracePool.add_binding("TexturesLayout",
							 1,
							 0,
							 VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
							 UInt32(resourceManager.get_textures().size()),
							 VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
							 VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
							 VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
							 VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);

	raytracePool.add_binding("SceneDataLayout",
							 0,
							 0,
							 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
							 1,
							 VK_SHADER_STAGE_COMPUTE_BIT,
							 VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
							 VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
							 VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);

	raytracePool.add_binding("SceneDataLayout",
							 0,
							 1,
							 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
							 1,
							 VK_SHADER_STAGE_COMPUTE_BIT,
							 VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
							 VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
							 VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);

	raytracePool.add_binding("SceneDataLayout",
							 0,
							 2,
							 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
							 1,
							 VK_SHADER_STAGE_COMPUTE_BIT,
							 VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
							 VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
							 VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);

	raytracePool.add_binding("SceneDataLayout",
							 0,
							 3,
							 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
							 1,
							 VK_SHADER_STAGE_COMPUTE_BIT,
							 VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
							 VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
							 VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);

	raytracePool.add_binding("SceneDataLayout",
							 0,
							 4,
							 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
							 1,
							 VK_SHADER_STAGE_COMPUTE_BIT,
							 VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
							 VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
							 VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);

	raytracePool.create_layouts(renderManager.get_logical_device(), nullptr);

	DynamicArray<VkPushConstantRange> raytraceConstants;
	VkPushConstantRange& raytrace = raytraceConstants.emplace_back();
	raytrace.size		= sizeof(RaytraceConstants);
	raytrace.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	raytracePool.set_push_constants(raytraceConstants);


	postprocessPool.add_binding("ScreenImage",
								4,
								0,
								VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
								1,
								VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
								VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
								VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
								VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);

	postprocessPool.create_layouts(renderManager.get_logical_device(), nullptr);
}

Void SRaytraceManager::setup_descriptors()
{
	SRenderManager& renderManager = SRenderManager::get();

	const Handle<DescriptorLayoutData> sceneLayout = raytracePool.get_layout_data_handle_by_name("SceneDataLayout");
	DynamicArray<DescriptorResourceInfo> sceneResources;

	VkDescriptorBufferInfo& vertexesInfo = sceneResources.emplace_back().bufferInfos.emplace_back();
	vertexesInfo.buffer = renderManager.get_buffer_by_handle(vertexesHandle).get_buffer();
	vertexesInfo.offset = 0;
	vertexesInfo.range  = sizeof(vertexes[0]) * vertexes.size();
	
	VkDescriptorBufferInfo& indexesInfo = sceneResources.emplace_back().bufferInfos.emplace_back();
	indexesInfo.buffer = renderManager.get_buffer_by_handle(indexesHandle).get_buffer();
	indexesInfo.offset = 0;
	indexesInfo.range  = sizeof(indexes[0]) * indexes.size();

	VkDescriptorBufferInfo& materialsInfo = sceneResources.emplace_back().bufferInfos.emplace_back();
	materialsInfo.buffer = renderManager.get_buffer_by_handle(materialsHandle).get_buffer();
	materialsInfo.offset = 0;
	materialsInfo.range  = sizeof(materials[0]) * materials.size();

	VkDescriptorBufferInfo& bvhInfo = sceneResources.emplace_back().bufferInfos.emplace_back();
	bvhInfo.buffer = renderManager.get_buffer_by_handle(bvhHandle).get_buffer();
	bvhInfo.offset = 0;
	bvhInfo.range = sizeof(bvh.hierarchy[0]) * bvh.hierarchy.size();

	VkDescriptorBufferInfo& emissionTrianglesInfo = sceneResources.emplace_back().bufferInfos.emplace_back();
	emissionTrianglesInfo.buffer = renderManager.get_buffer_by_handle(emissionTrianglesHandle).get_buffer();
	emissionTrianglesInfo.offset = 0;
	emissionTrianglesInfo.range  = sizeof(emissionTriangles[0]) * emissionTriangles.size();

	sceneData = raytracePool.add_set(sceneLayout, sceneResources, "SceneData");


	bindlessTextures = renderManager.get_pool().get_set_data_handle_by_name("Textures");

	for (UInt64 i = 0; i < screenTextures.size(); ++i)
	{
		const Handle<DescriptorLayoutData> screenLayout = raytracePool.get_layout_data_handle_by_name("ScreenImage");
		const Image& screen = renderManager.get_image_by_handle(screenTextures[i].image);
		DynamicArray<DescriptorResourceInfo> screenResources;
		VkDescriptorImageInfo& screenInfo = screenResources.emplace_back().imageInfos.emplace_back();
		screenInfo.imageLayout = screen.get_current_layout();
		screenInfo.imageView = screen.get_view();
		screenInfo.sampler = screen.get_sampler();

		screenImages[i] = raytracePool.add_set(screenLayout, screenResources, "ScreenImage" + std::to_string(i));
	}


	const Handle<DescriptorLayoutData> accumulationLayout = raytracePool.get_layout_data_handle_by_name("AccumulationLayout");
	const Image& accumulation = renderManager.get_image_by_handle(accumulationTexture.image);
	DynamicArray<DescriptorResourceInfo> accumulationResources;
	VkDescriptorImageInfo& accumulationInfo = accumulationResources.emplace_back().imageInfos.emplace_back();
	accumulationInfo.imageLayout = accumulation.get_current_layout();
	accumulationInfo.imageView	 = accumulation.get_view();
	accumulationInfo.sampler	 = accumulation.get_sampler();

	accumulationImage = raytracePool.add_set(accumulationLayout, accumulationResources, "AccumulationTexture");


	const Handle<DescriptorLayoutData> directionLayout = raytracePool.get_layout_data_handle_by_name("DirectionLayout");
	const Image& direction = renderManager.get_image_by_handle(directionTexture.image);
	DynamicArray<DescriptorResourceInfo> directionResources;
	VkDescriptorImageInfo& directionInfo = directionResources.emplace_back().imageInfos.emplace_back();
	directionInfo.imageLayout = direction.get_current_layout();
	directionInfo.imageView	  = direction.get_view();
	directionInfo.sampler	  = direction.get_sampler();

	directionImage = raytracePool.add_set(directionLayout, directionResources, "DirectionTexture");

	raytracePool.create_sets(renderManager.get_logical_device(), nullptr);


	for (UInt64 i = 0; i < screenTextures.size(); ++i)
	{
		const Handle<DescriptorLayoutData> fragmentLayout = postprocessPool.get_layout_data_handle_by_name("ScreenImage");
		const Image& screen = renderManager.get_image_by_handle(screenTextures[i].image);
		DynamicArray<DescriptorResourceInfo> fragmentResources;
		VkDescriptorImageInfo& fragmentInfo = fragmentResources.emplace_back().imageInfos.emplace_back();
		fragmentInfo.imageLayout = screen.get_current_layout();
		fragmentInfo.imageView = screen.get_view();
		fragmentInfo.sampler = screen.get_sampler();

		fragmentImages[i] = postprocessPool.add_set(fragmentLayout, fragmentResources, "ScreenImage" + std::to_string(i));
	}

	postprocessPool.create_sets(renderManager.get_logical_device(), nullptr);
}

Void SRaytraceManager::create_quad_buffers()
{
	SRenderManager& renderManager = SRenderManager::get();
	const DynamicArray<FVector3> positions =
	{
		FVector3( 1.0f, -1.0f, 1.0f),
		FVector3(-1.0f, -1.0f, 1.0f),
		FVector3( 1.0f,  1.0f, 1.0f),
		FVector3(-1.0f,  1.0f, 1.0f),
	};

	const DynamicArray<FVector3> normals =
	{
		FVector3(0.0f, 0.0f, 1.0f),
		FVector3(0.0f, 0.0f, 1.0f),
		FVector3(0.0f, 0.0f, 1.0f),
		FVector3(0.0f, 0.0f, 1.0f),
	};

	const DynamicArray<FVector2> uvs =
	{
		FVector2(1.0f, 1.0f),
		FVector2(0.0f, 1.0f),
		FVector2(1.0f, 0.0f),
		FVector2(0.0f, 0.0f),
	};

	const DynamicArray<UInt32> indexes =
	{
		0, 1, 2, //Top left triangle
		2, 1, 3, //Bottom right triangle
	};

    quadIndexes	  = renderManager.create_static_buffer(indexes, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    quadPositions = renderManager.create_static_buffer(positions, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	quadNormals   = renderManager.create_static_buffer(normals, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    quadUvs		  = renderManager.create_static_buffer(uvs, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
}

Int32 SRaytraceManager::get_frame_count() const
{
	return frameCount;
}

FVector3 SRaytraceManager::get_background_color() const
{
	return backgroundColor;
}

Texture& SRaytraceManager::get_screen_texture()
{
	return screenTextures[currentImageIndex];
}

Void SRaytraceManager::reload_shaders()
{
	SRenderManager& renderManager = SRenderManager::get();
	const LogicalDevice& logicalDevice = renderManager.get_logical_device();
    Bool result = true;

	Shader& generationShader = renderManager.get_shader_by_handle(rayGeneration);
	Shader& traceShader = renderManager.get_shader_by_handle(raytrace);
	Shader& fragment = renderManager.get_shader_by_handle(screenF);
	Shader& vertex = renderManager.get_shader_by_handle(screenV);

    result &= generationShader.recreate(logicalDevice, nullptr);
    result &= traceShader.recreate(logicalDevice, nullptr);
    result &= fragment.recreate(logicalDevice, nullptr);
    result &= vertex.recreate(logicalDevice, nullptr);

    if (!result)
    {
        SPDLOG_ERROR("Failed to reload shaders.");
        return;
    }

    DynamicArray<Shader> shaders;
    shaders.emplace_back(fragment);
    shaders.emplace_back(vertex);
	
    logicalDevice.wait_idle();
    postprocessPipeline.recreate_pipeline(postprocessPool,
										  renderManager.get_render_pass_by_handle(postprocessPass),
										  shaders,
										  logicalDevice,
										  nullptr);

    raytracePipeline.recreate_pipeline(raytracePool,
								       RenderPass(),
									   { traceShader },
								       logicalDevice,
								       nullptr);

    rayGenerationPipeline.recreate_pipeline(rayGenerationPool,
											RenderPass(),
											{ generationShader },
											logicalDevice,
											nullptr);
}

Void SRaytraceManager::refresh()
{
	shouldRefresh = true;
}

Void SRaytraceManager::shutdown()
{
	SPDLOG_INFO("Raytrace Manager shutdown.");
	const SRenderManager& renderManager = SRenderManager::get();
	const LogicalDevice& logicalDevice = renderManager.get_logical_device();
	logicalDevice.wait_idle();
	SPDLOG_INFO("Wait until frame end...");

	raytracePool.clear(logicalDevice, nullptr);
	rayGenerationPool.clear(logicalDevice, nullptr);
	postprocessPool.clear(logicalDevice, nullptr);
	rayGenerationPipeline.clear(logicalDevice, nullptr);
	raytracePipeline.clear(logicalDevice, nullptr);
	postprocessPipeline.clear(logicalDevice, nullptr);


	for (Texture& texture : screenTextures)
	{
		free(texture.data);
	}
}
