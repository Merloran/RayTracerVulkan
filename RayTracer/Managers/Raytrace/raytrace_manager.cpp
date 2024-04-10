#include "raytrace_manager.hpp"

#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <magic_enum.hpp>

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
#include "Common/bvh_node.hpp"
#include "Common/vertex.hpp"

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
	accumulationTexture.name = "Result.png";
	accumulationTexture.channels = 4;
	rayGeneration = renderManager.load_shader(renderManager.SHADERS_PATH + "RayGeneration.comp", EShaderType::Compute);
	raytrace	  = renderManager.load_shader(renderManager.SHADERS_PATH + "RayTrace.comp", EShaderType::Compute);
	screenV		  = renderManager.load_shader(renderManager.SHADERS_PATH + "Screen.vert", EShaderType::Vertex);
	screenF		  = renderManager.load_shader(renderManager.SHADERS_PATH + "Screen.frag", EShaderType::Fragment);

	renderTime = 0.0f;
	maxBouncesCount = 0;
	backgroundColor = { 0.0f, 0.0f, 0.0f };

	materials.reserve(resourceManager.get_materials().size());
	for (const Material& material : resourceManager.get_materials())
	{
		GPUMaterial& gpuMaterial = materials.emplace_back();
		gpuMaterial.albedo	  = material.textures[UInt64(ETextureType::Albedo)].id;
		gpuMaterial.normal	  = material.textures[UInt64(ETextureType::Normal)].id;

		if (material.textures[UInt64(ETextureType::RM)].id != Handle<Texture>::sNone.id)
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
				vertex.position   = mesh.positions[i];
				vertex.normal	  = mesh.normals[i];
				vertex.uv		  = mesh.uvs[i];
				vertex.materialId = UInt64(model.materials[i].id);
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
				indexes.emplace_back(mesh.indexes[j] + indexesOffset);
				const GPUMaterial& gpuMaterial = materials[vertexes[mesh.indexes[j] + indexesOffset].materialId];
				if (j % 3 == 0 && gpuMaterial.emission != -1)
				{
					emissionTriangles.emplace_back(mesh.indexes[j] + indexesOffset);
				}
			}
			indexesOffset += UInt32(mesh.positions.size());
		}
	}

	bvh.create_tree(vertexes, indexes);

	vertexesHandle			= renderManager.create_static_buffer(vertexes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	indexesHandle			= renderManager.create_static_buffer(indexes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	materialsHandle			= renderManager.create_static_buffer(materials, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	emissionTrianglesHandle = renderManager.create_static_buffer(emissionTriangles, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	bvhHandle				= renderManager.create_static_buffer(bvh.hierarchy, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);


	accumulationTexture.image = renderManager.create_image(displayManager.get_framebuffer_size(),
														   VK_FORMAT_R32G32B32A32_SFLOAT,
														   VK_IMAGE_USAGE_SAMPLED_BIT,
														   VK_IMAGE_TILING_OPTIMAL);
	renderManager.transition_image_layout(renderManager.get_image_by_handle(accumulationTexture.image),
										  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
										  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
										  VK_IMAGE_LAYOUT_GENERAL);
	renderManager.transition_image_layout(renderManager.get_image_by_handle(accumulationTexture.image),
										  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
										  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
										  VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL);


	directionTexture.image = renderManager.create_image(displayManager.get_framebuffer_size(),
														VK_FORMAT_R32G32B32A32_SFLOAT,
														VK_IMAGE_USAGE_SAMPLED_BIT,
														VK_IMAGE_TILING_OPTIMAL);
	renderManager.transition_image_layout(renderManager.get_image_by_handle(directionTexture.image),
										  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
										  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
										  VK_IMAGE_LAYOUT_GENERAL);
	renderManager.transition_image_layout(renderManager.get_image_by_handle(directionTexture.image),
										  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
										  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
										  VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL);

	create_quad_buffers();
	create_descriptors();
	create_pipelines();
	setup_descriptors();
	raytracePool = renderManager.create_command_pool(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
	renderManager.create_command_buffers(raytracePool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, { "RaytraceBuffer" });
	raytraceBuffer = renderManager.get_command_buffer_handle_by_name("RaytraceBuffer");
}

Void SRaytraceManager::update(Camera &camera, Float32 deltaTime)
{
	SDisplayManager &displayManager = SDisplayManager::get();
	const SRenderManager &renderManager = SRenderManager::get();

	const IVector2& size = displayManager.get_window_size();

	renderTime += deltaTime;
	const Bool hasWindowResized = accumulationTexture.size != size || shouldRefresh;
	const Bool hasCameraChanged = camera.has_changed();

	if (hasWindowResized)
	{
		shouldRefresh = false;
		resize_images(size);
		renderManager.recreate_swapchain(postprocessSwapchain, postprocessPass);
	}

	if (hasWindowResized || hasCameraChanged)
	{
		camera.set_camera_changed(false);
		generate_rays(camera);
		renderTime = 0.0f;
	}

	if (frameLimit == 0 || frameCount < frameLimit)
	{
		ray_trace(camera);
		frameCount++;
	}

	render();
}

Void SRaytraceManager::render()
{
	SRenderManager& renderManager = SRenderManager::get();
	
    // vkWaitForFences(logicalDevice.get_device(), 1, &inFlightFences[currentFrame], VK_TRUE, Limits<UInt64>::max());
    //
    UInt32 imageIndex = 0;
    // VkResult result = vkAcquireNextImageKHR(logicalDevice.get_device(),
    //                                         swapchain.get_swapchain(),
    //                                         Limits<UInt64>::max(),
    //                                         imageAvailableSemaphores[currentFrame],
    //                                         VK_NULL_HANDLE,
    //                                         &imageIndex);
    //
    // if (result == VK_ERROR_OUT_OF_DATE_KHR)
    // {
    //     return;
    // }
    // else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    // {
    //     SPDLOG_ERROR("Acquire image failed with: {}", magic_enum::enum_name(result));
    //     return;
    // }
    //
    // vkResetFences(logicalDevice.get_device(), 1, &inFlightFences[currentFrame]);

	const CommandBuffer& commandBuffer = renderManager.get_command_buffer_by_handle(raytraceBuffer);
	commandBuffer.reset(0);

	
	const UVector2& extent = postprocessSwapchain.get_extent();

	commandBuffer.begin();
	commandBuffer.begin_render_pass(postprocessPass, postprocessSwapchain, imageIndex, VK_SUBPASS_CONTENTS_INLINE);
	commandBuffer.bind_pipeline(postprocessPipeline);

	commandBuffer.set_viewport(0, { 0.0f, 0.0f }, extent, { 0.0f, 1.0f });
	commandBuffer.set_scissor(0, { 0, 0 }, extent);


	const DescriptorSetData& accumulation = descriptorPool.get_set_data_by_handle(accumulationImage);
	commandBuffer.bind_descriptor_set(raytracePipeline, accumulation.set, 3);
	
	PostprocessConstants constants;
	constants.invFrameCount = 1.0f / Float32(frameCount - 1);

	commandBuffer.set_constants(postprocessPipeline,
								VK_SHADER_STAGE_FRAGMENT_BIT,
								sizeof(RayGenerationConstants) + sizeof(RaytraceConstants),
								sizeof(constants),
								&constants);

	const VkBuffer positionsBuffer = renderManager.get_buffer_by_handle(quadPositions).get_buffer();
	const VkBuffer uvsBuffer	   = renderManager.get_buffer_by_handle(quadUvs).get_buffer();
	const VkBuffer indexBuffer	   = renderManager.get_buffer_by_handle(quadIndexes).get_buffer();

	const Array<VkBuffer, 2> vertexBuffers = { positionsBuffer, uvsBuffer };
	const Array<VkDeviceSize, 2> offsets = { 0, 0 };
	commandBuffer.bind_vertex_buffers(0, vertexBuffers, offsets);
	commandBuffer.bind_index_buffer(indexBuffer, 0, VK_INDEX_TYPE_UINT32);

	commandBuffer.draw_indexed(6, 1, 0, 0, 0);

	//IMGUI
	//ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer.get_buffer());

	commandBuffer.end_render_pass();
	commandBuffer.end();



	// VkSubmitInfo submitInfo{};
	// Array<VkSemaphore, 1> waitSemaphores = { imageAvailableSemaphores[currentFrame] };
	// Array<VkSemaphore, 1> signalSemaphores = { renderFinishedSemaphores[currentFrame] };
	// Array<VkPipelineStageFlags, 1> waitStages = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	// submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	// submitInfo.waitSemaphoreCount = UInt32(waitSemaphores.size());
	// submitInfo.pWaitSemaphores = waitSemaphores.data();
	// submitInfo.pWaitDstStageMask = waitStages.data();
	// submitInfo.commandBufferCount = 1;
	// submitInfo.pCommandBuffers = &currentBuffer.get_buffer();
	// submitInfo.signalSemaphoreCount = UInt32(signalSemaphores.size());
	// submitInfo.pSignalSemaphores = signalSemaphores.data();
	//
	// result = vkQueueSubmit(logicalDevice.get_graphics_queue(), 1, &submitInfo, inFlightFences[currentFrame]);
	// if (result != VK_SUCCESS)
	// {
	// 	SPDLOG_ERROR("Submit draw commands failed with: {}", magic_enum::enum_name(result));
	// 	return;
	// }
	//
	// VkPresentInfoKHR presentInfo{};
	// presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	// presentInfo.waitSemaphoreCount = UInt32(signalSemaphores.size());
	// presentInfo.pWaitSemaphores = signalSemaphores.data();
	// presentInfo.swapchainCount = 1;
	// presentInfo.pSwapchains = &swapchain.get_swapchain();
	// presentInfo.pImageIndices = &imageIndex;
	// presentInfo.pResults = nullptr; // Optional
	//
	// result = vkQueuePresentKHR(logicalDevice.get_present_queue(), &presentInfo);
	//
	// if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || SDisplayManager::get().was_resize_handled())
	// {
	// }
	// else if (result != VK_SUCCESS)
	// {
	// 	SPDLOG_ERROR("Present swapchain image failed with: {}", magic_enum::enum_name(result));
	// 	return;
	// }
}

Void SRaytraceManager::resize_images(const UVector2& size)
{
	SRenderManager& renderManager = SRenderManager::get();
	accumulationTexture.size = size;
	directionTexture.size    = size;
	renderManager.resize_image(size, accumulationTexture.image);
	renderManager.resize_image(size, directionTexture.image);
	

	DescriptorResourceInfo accumulationResource;
	VkDescriptorImageInfo& accumulationInfo = accumulationResource.imageInfos.emplace_back();
	const Image& accumulationImage = renderManager.get_image_by_handle(accumulationTexture.image);
	accumulationInfo.imageLayout  = accumulationImage.get_current_layout();
	accumulationInfo.imageView	  = accumulationImage.get_view();
	accumulationInfo.sampler      = accumulationImage.get_sampler();

	const Handle<DescriptorSetData> accumulationHandle = descriptorPool.get_set_data_handle_by_name("AccumulationTexture");
	descriptorPool.update_set(renderManager.get_logical_device(), accumulationResource, accumulationHandle, 0, 0);


	DescriptorResourceInfo directionResource;
	VkDescriptorImageInfo& directionInfo = directionResource.imageInfos.emplace_back();
	const Image& directionImage = renderManager.get_image_by_handle(directionTexture.image);
	directionInfo.imageLayout = directionImage.get_current_layout();
	directionInfo.imageView	  = directionImage.get_view();
	directionInfo.sampler     = directionImage.get_sampler();

	const Handle<DescriptorSetData> directionHandle = descriptorPool.get_set_data_handle_by_name("DirectionTexture");
	descriptorPool.update_set(renderManager.get_logical_device(), directionResource, directionHandle, 0, 0);
}

Void SRaytraceManager::ray_trace(Camera& camera)
{
	SResourceManager& resourceManager = SResourceManager::get();
	SRenderManager& renderManager = SRenderManager::get();

	const UVector2 workGroupsCount = glm::ceil(FVector2(directionTexture.size) / FVector2(WORKGROUP_SIZE));
	const CommandBuffer& commandBuffer = renderManager.get_command_buffer_by_handle(raytraceBuffer);

	commandBuffer.begin();
	commandBuffer.bind_pipeline(raytracePipeline);

	DescriptorSetData& accumulation = descriptorPool.get_set_data_by_handle(accumulationImage);
	DescriptorSetData& direction	= descriptorPool.get_set_data_by_handle(directionImage);
	DescriptorSetData& textures		= descriptorPool.get_set_data_by_handle(bindlessTextures);
	DescriptorSetData& scene		= descriptorPool.get_set_data_by_handle(sceneData);

	commandBuffer.bind_descriptor_set(raytracePipeline, accumulation.set, 3);
	commandBuffer.bind_descriptor_set(raytracePipeline, direction.set, 2);
	commandBuffer.bind_descriptor_set(raytracePipeline, textures.set, 1);
	commandBuffer.bind_descriptor_set(raytracePipeline, scene.set, 0);

	RaytraceConstants constants{};
	constants.backgroundColor		 = backgroundColor;
	constants.cameraPosition		 = camera.get_position();
	constants.pixelDeltaU			 = pixelDeltaU;
	constants.pixelDeltaV			 = pixelDeltaV;
	constants.imageSize				 = accumulationTexture.size;
	constants.viewBounds			 = camera.get_view_bounds();
	constants.time					 = renderTime;
	constants.frameCount			 = frameCount;
	constants.trianglesCount		 = trianglesCount;
	constants.emissionTrianglesCount = Int32(emissionTriangles.size());
	constants.maxBouncesCount		 = maxBouncesCount;
	constants.rootId				 = bvh.rootId;
	constants.environmentMapId		 = Int32(resourceManager.get_textures().size() - 1ULL);

	commandBuffer.set_constants(raytracePipeline,
								VK_SHADER_STAGE_COMPUTE_BIT,
								sizeof(RayGenerationConstants),
								sizeof(constants),
								&constants);

	commandBuffer.dispatch({ workGroupsCount, 1 });

	commandBuffer.end();
}

Void SRaytraceManager::generate_rays(Camera& camera)
{
	const SDisplayManager& displayManager = SDisplayManager::get();
	SRenderManager& renderManager = SRenderManager::get();
	const CommandBuffer& commandBuffer = renderManager.get_command_buffer_by_handle(raytraceBuffer);
	const UVector2 workGroupsCount = glm::ceil(FVector2(directionTexture.size) / FVector2(WORKGROUP_SIZE));

	frameCount = 1;
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
	

	commandBuffer.begin();
	commandBuffer.bind_pipeline(raytracePipeline);

	DescriptorSetData& accumulation = descriptorPool.get_set_data_by_handle(accumulationImage);
	DescriptorSetData& direction    = descriptorPool.get_set_data_by_handle(directionImage);
	
	commandBuffer.bind_descriptor_set(raytracePipeline, accumulation.set, 3);
	commandBuffer.bind_descriptor_set(raytracePipeline, direction.set, 2);

	RayGenerationConstants constants{};
	constants.cameraPosition = camera.get_position();
	constants.originPixel	 = originPixel;
	constants.pixelDeltaU	 = pixelDeltaU;
	constants.pixelDeltaV	 = pixelDeltaV;
	constants.imageSize		 = directionTexture.size;

	commandBuffer.set_constants(raytracePipeline,
								VK_SHADER_STAGE_COMPUTE_BIT,
								0,
								sizeof(constants),
								&constants);

	commandBuffer.dispatch({ workGroupsCount, 1 });

	commandBuffer.end();
	// glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT); ???
}

Void SRaytraceManager::create_pipelines()
{
	SRenderManager& renderManager = SRenderManager::get();
	raytracePipeline.create_compute_pipeline(descriptorPool, 
											 renderManager.get_shader_by_handle(raytrace), 
											 renderManager.get_logical_device(), 
											 nullptr);

	rayGenerationPipeline.create_compute_pipeline(descriptorPool, 
												  renderManager.get_shader_by_handle(rayGeneration), 
												  renderManager.get_logical_device(), 
												  nullptr);
	DynamicArray<Shader> shaders;
	shaders.push_back(renderManager.get_shader_by_handle(screenV));
	shaders.push_back(renderManager.get_shader_by_handle(screenF));

	postprocessSwapchain.create(renderManager.get_logical_device(),
								renderManager.get_physical_device(),
								renderManager.get_surface(),
								nullptr);

	postprocessPass.create(renderManager.get_physical_device(), 
						   renderManager.get_logical_device(), 
						   postprocessSwapchain, 
						   nullptr,
						   VK_SAMPLE_COUNT_1_BIT,
						   false);

	postprocessSwapchain.create_framebuffers(renderManager.get_logical_device(), postprocessPass, nullptr);

	postprocessPipeline.create_graphics_pipeline(descriptorPool, 
											     postprocessPass, 
											     shaders, 
											     renderManager.get_logical_device(), 
											     nullptr);
}

Void SRaytraceManager::create_descriptors()
{
	SResourceManager& resourceManager = SResourceManager::get();
	const SRenderManager& renderManager = SRenderManager::get();

	descriptorPool.add_binding("AccumulationLayout",
							   3,
							   0,
							   VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
							   1,
							   VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
							   VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
							   VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
							   VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);

	descriptorPool.add_binding("DirectionLayout",
							   2,
							   0,
							   VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
							   1,
							   VK_SHADER_STAGE_COMPUTE_BIT,
							   VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
							   VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
							   VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);

	descriptorPool.add_binding("TexturesLayout",
							   1,
							   0,
							   VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
							   UInt32(resourceManager.get_textures().size()),
							   VK_SHADER_STAGE_COMPUTE_BIT,
							   VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
							   VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
							   VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);

	descriptorPool.add_binding("SceneDataLayout",
							   0,
							   0,
							   VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
							   UInt32(vertexes.size()),
							   VK_SHADER_STAGE_COMPUTE_BIT,
							   VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
							   VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
							   VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);

	descriptorPool.add_binding("SceneDataLayout",
							   0,
							   1,
							   VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
							   UInt32(indexes.size()),
							   VK_SHADER_STAGE_COMPUTE_BIT,
							   VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
							   VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
							   VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);

	descriptorPool.add_binding("SceneDataLayout",
							   0,
							   2,
							   VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
							   UInt32(materials.size()),
							   VK_SHADER_STAGE_COMPUTE_BIT,
							   VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
							   VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
							   VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);

	descriptorPool.add_binding("SceneDataLayout",
							   0,
							   3,
							   VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
							   UInt32(bvh.hierarchy.size()),
							   VK_SHADER_STAGE_COMPUTE_BIT,
							   VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
							   VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
							   VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);

	descriptorPool.add_binding("SceneDataLayout",
							   0,
							   4,
							   VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
							   UInt32(emissionTriangles.size()),
							   VK_SHADER_STAGE_COMPUTE_BIT,
							   VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
							   VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
							   VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);
	

	descriptorPool.create_layouts(renderManager.get_logical_device(), nullptr);

	DynamicArray<VkPushConstantRange> pushConstants;
	VkPushConstantRange& rayGeneration = pushConstants.emplace_back();
	rayGeneration.size		 = sizeof(RayGenerationConstants);
	rayGeneration.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	VkPushConstantRange& raytrace = pushConstants.emplace_back();
	raytrace.size		= sizeof(RaytraceConstants);
	raytrace.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	descriptorPool.set_push_constants(pushConstants);
}

Void SRaytraceManager::setup_descriptors()
{
	SResourceManager& resourceManager = SResourceManager::get();
	SRenderManager& renderManager = SRenderManager::get();

	const Handle<DescriptorLayoutData> sceneLayout = descriptorPool.get_layout_data_handle_by_name("SceneDataLayout");
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

	VkDescriptorBufferInfo& emissionTrianglesInfo = sceneResources.emplace_back().bufferInfos.emplace_back();
	emissionTrianglesInfo.buffer = renderManager.get_buffer_by_handle(emissionTrianglesHandle).get_buffer();
	emissionTrianglesInfo.offset = 0;
	emissionTrianglesInfo.range  = sizeof(emissionTriangles[0]) * emissionTriangles.size();

	VkDescriptorBufferInfo& bvhInfo = sceneResources.emplace_back().bufferInfos.emplace_back();
	bvhInfo.buffer = renderManager.get_buffer_by_handle(bvhHandle).get_buffer();
	bvhInfo.offset = 0;
	bvhInfo.range  = sizeof(bvh.hierarchy[0]) * bvh.hierarchy.size();

	descriptorPool.add_set(sceneLayout, sceneResources, "SceneData");


	DynamicArray<DescriptorResourceInfo> textureResources;
	DynamicArray<VkDescriptorImageInfo>& imageInfos = textureResources.emplace_back().imageInfos;
	const DynamicArray<Texture>& textures = resourceManager.get_textures();
	imageInfos.reserve(textures.size());
	for (const Texture& texture : textures)
	{
		Image& image = renderManager.get_image_by_handle(texture.image);
		VkDescriptorImageInfo& imageInfo = imageInfos.emplace_back();
		imageInfo.imageLayout = image.get_current_layout();
		imageInfo.imageView	  = image.get_view();
		imageInfo.sampler	  = image.get_sampler();
	}

	this->bindlessTextures = descriptorPool.add_set(descriptorPool.get_layout_data_handle_by_name("TexturesDataLayout"), 
											textureResources, 
											"Textures");


	const Handle<DescriptorLayoutData> accumulationLayout = descriptorPool.get_layout_data_handle_by_name("AccumulationLayout");
	DynamicArray<DescriptorResourceInfo> accumulationResources;
	VkDescriptorImageInfo& accumulationInfo = accumulationResources.emplace_back().imageInfos.emplace_back();
	const Image& accumulationImage = renderManager.get_image_by_handle(accumulationTexture.image);
	accumulationInfo.imageLayout = accumulationImage.get_current_layout();
	accumulationInfo.imageView	 = accumulationImage.get_view();
	accumulationInfo.sampler	 = accumulationImage.get_sampler();

	descriptorPool.add_set(accumulationLayout, accumulationResources, "AccumulationTexture");


	const Handle<DescriptorLayoutData> directionLayout = descriptorPool.get_layout_data_handle_by_name("DirectionLayout");
	DynamicArray<DescriptorResourceInfo> directionResources;
	VkDescriptorImageInfo& directionInfo = directionResources.emplace_back().imageInfos.emplace_back();
	const Image& directionImage = renderManager.get_image_by_handle(directionTexture.image);
	directionInfo.imageLayout = directionImage.get_current_layout();
	directionInfo.imageView	  = directionImage.get_view();
	directionInfo.sampler	  = directionImage.get_sampler();

	descriptorPool.add_set(directionLayout, directionResources, "DirectionTexture");


	descriptorPool.create_sets(renderManager.get_logical_device(), nullptr);
}

Void SRaytraceManager::create_quad_buffers()
{
	SRenderManager& renderManager = SRenderManager::get();
	const DynamicArray<UVector3> positions =
	{
		UVector3( 1.0f, -1.0f, 1.0f),
		UVector3(-1.0f, -1.0f, 1.0f),
		UVector3( 1.0f,  1.0f, 1.0f),
		UVector3(-1.0f,  1.0f, 1.0f),
	};

	const DynamicArray<UVector2> uvs =
	{
		UVector2(1.0f, 0.0f),
		UVector2(0.0f, 0.0f),
		UVector2(1.0f, 1.0f),
		UVector2(0.0f, 1.0f),
	};

	const DynamicArray<UInt32> indexes =
	{
		0, 1, 2, //Top left triangle
		1, 2, 3, //Bottom right triangle
	};

    quadIndexes	  = renderManager.create_static_buffer(indexes, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    quadPositions = renderManager.create_static_buffer(positions, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
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

const Texture& SRaytraceManager::get_screen_texture() const
{
	return accumulationTexture;
}

Void SRaytraceManager::refresh()
{
	shouldRefresh = true;
}

Void SRaytraceManager::shutdown()
{
	SPDLOG_INFO("Raytrace Manager shutdown.");
	const SRenderManager& renderManager = SRenderManager::get();
	vkDeviceWaitIdle(renderManager.get_logical_device().get_device());
	SPDLOG_INFO("Wait until frame end...");

	descriptorPool.clear(renderManager.get_logical_device(), nullptr);
	postprocessSwapchain.clear(renderManager.get_logical_device(), nullptr);
	postprocessPass.clear(renderManager.get_logical_device(), nullptr);
	postprocessPipeline.clear(renderManager.get_logical_device(), nullptr);
	rayGenerationPipeline.clear(renderManager.get_logical_device(), nullptr);
	raytracePipeline.clear(renderManager.get_logical_device(), nullptr);
}
