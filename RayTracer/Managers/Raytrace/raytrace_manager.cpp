#include "raytrace_manager.hpp"

#include <glad/glad.h>

#include "../Render/Camera/camera.hpp"
#include "../Render/Common/image.hpp"
#include "../Render/render_manager.hpp"
#include "../Display/display_manager.hpp"
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

	create_descriptors();
	create_pipelines();
	setup_descriptors();
}

Void SRaytraceManager::update(Camera &camera, Float32 deltaTime)
{
	SDisplayManager &displayManager = SDisplayManager::get();
	SRenderManager &renderManager = SRenderManager::get();

	const IVector2& size = displayManager.get_window_size();

	renderTime += deltaTime;
	const Bool hasWindowResized = accumulationTexture.size != size || shouldRefresh;
	const Bool hasCameraChanged = camera.has_changed();

	if (hasWindowResized)
	{
		shouldRefresh = false;
		accumulationTexture.size = size;
		directionTexture.size = size;
		renderManager.resize_image(size, accumulationTexture.image);
		renderManager.resize_image(size, directionTexture.image);
		// glBindImageTexture(0, directionTexture.gpuId, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
		// glBindImageTexture(1, screenTexture.gpuId, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
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
	
	// screen.use();
	// screen.set_int("accumulated", 0);
	// screen.set_float("invFrameCount", Float32(1.0f / (frameCount - 1)));
	// glActiveTexture(GL_TEXTURE0);
	// glBindTexture(GL_TEXTURE_2D, screenTexture.gpuId);
	// renderManager.draw_quad();
}

Void SRaytraceManager::ray_trace(Camera& camera)
{
	// rayTrace.use();
	// rayTrace.set_vec3("backgroundColor", backgroundColor);
	// rayTrace.set_vec3("cameraPosition", camera.get_position());
	// rayTrace.set_vec3("pixelDeltaU", pixelDeltaU);
	// rayTrace.set_vec3("pixelDeltaV", pixelDeltaV);
	// rayTrace.set_ivec2("imageSize", screenTexture.size);
	// rayTrace.set_vec2("viewBounds", camera.get_view_bounds());
	// rayTrace.set_float("time", renderTime);
	// rayTrace.set_int("frameCount", frameCount);
	// rayTrace.set_int("trianglesCount", trianglesCount);
	// rayTrace.set_int("emissionTrianglesCount", emissionTriangles.size());
	// rayTrace.set_int("maxBouncesCount", maxBouncesCount);
	// rayTrace.set_int("rootId", bvh.rootId);
	// rayTrace.set_int("environmentMapId", textures.size() - 1);

	const IVector2 workGroupsCount = glm::ceil(FVector2(directionTexture.size) / FVector2(WORKGROUP_SIZE));
	
	// glDispatchCompute(workGroupsCount.x, workGroupsCount.y, 1);
}

Void SRaytraceManager::generate_rays(Camera& camera)
{
	const SDisplayManager& displayManager = SDisplayManager::get();
	const IVector2 workGroupsCount = glm::ceil(FVector2(directionTexture.size) / FVector2(WORKGROUP_SIZE));

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
	
	// rayGeneration.set_vec3("cameraPosition", camera.get_position());
	// rayGeneration.set_vec3("originPixel", originPixel);
	// rayGeneration.set_vec3("pixelDeltaU", pixelDeltaU);
	// rayGeneration.set_vec3("pixelDeltaV", pixelDeltaV);
	// rayGeneration.set_ivec2("imageSize", directionTexture.size);
	// glDispatchCompute(workGroupsCount.x, workGroupsCount.y, 1);
	// glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
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

	descriptorPool.add_binding("RayGenerationLayout",
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
	rayGeneration.offset	 = 0;
	rayGeneration.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	VkPushConstantRange& raytrace = pushConstants.emplace_back();
	raytrace.size		= sizeof(RaytraceConstants);
	raytrace.offset		= 0;
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

	descriptorPool.add_set(sceneLayout,
						   sceneResources,
						   "SceneData");


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

	descriptorPool.add_set(descriptorPool.get_layout_data_handle_by_name("TexturesDataLayout"),
						   textureResources,
						   "Textures");


	descriptorPool.create_sets(renderManager.get_logical_device(), nullptr);
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
	SRenderManager& renderManager = SRenderManager::get();
	descriptorPool.clear(renderManager.get_logical_device(), nullptr);
	postprocessSwapchain.clear(renderManager.get_logical_device(), nullptr);
	postprocessPass.clear(renderManager.get_logical_device(), nullptr);
	postprocessPipeline.clear(renderManager.get_logical_device(), nullptr);
	rayGenerationPipeline.clear(renderManager.get_logical_device(), nullptr);
	raytracePipeline.clear(renderManager.get_logical_device(), nullptr);
}
