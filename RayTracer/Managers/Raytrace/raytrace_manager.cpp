#include "raytrace_manager.hpp"

#include <glad/glad.h>

#include "../Render/Camera/camera.hpp"
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
	SRenderManager& renderManager = SRenderManager::get();
	screenTexture.name = "Result.png";
	screenTexture.channels = 4;
	rayGeneration = renderManager.load_shader(renderManager.SHADERS_PATH + "RayGeneration.comp", EShaderType::Compute);
	rayTrace = renderManager.load_shader(renderManager.SHADERS_PATH + "RayTrace.comp", EShaderType::Compute);
	screenV  = renderManager.load_shader(renderManager.SHADERS_PATH + "Screen.vert", EShaderType::Vertex);
	screenF  = renderManager.load_shader(renderManager.SHADERS_PATH + "Screen.frag", EShaderType::Fragment);

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
		for (UInt64 i = 0; i < model.meshes.size(); ++i)
		{
			const Mesh& mesh = resourceManager.get_mesh_by_handle(model.meshes[i]);
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

	Int32 indexesOffset = 0;
	for (const Model& model : models)
	{
		for (UInt64 i = 0; i < model.meshes.size(); ++i)
		{
			const Mesh& mesh = resourceManager.get_mesh_by_handle(model.meshes[i]);
			for (UInt64 j = 0; j < mesh.indexes.size(); ++j)
			{
				indexes.emplace_back(mesh.indexes[j] + indexesOffset);
				const GPUMaterial& gpuMaterial = materials[vertexes[mesh.indexes[j] + indexesOffset].materialId];
				if (j % 3 == 0 && gpuMaterial.emission != -1)
				{
					emissionTriangles.emplace_back(mesh.indexes[j] + indexesOffset);
				}
			}
			indexesOffset += mesh.positions.size();
		}
	}

	vertexesHandle			= renderManager.create_static_buffer(vertexes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	indexesHandle			= renderManager.create_static_buffer(indexes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	materialsHandle			= renderManager.create_static_buffer(materials, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	emissionTrianglesHandle = renderManager.create_static_buffer(emissionTriangles, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

	bvh.create_tree(vertexes, indexes);
}

Void SRaytraceManager::update(Camera &camera, Float32 deltaTime)
{
	SDisplayManager &displayManager = SDisplayManager::get();
	SRenderManager &renderManager = SRenderManager::get();

	const IVector2& size = displayManager.get_window_size();

	renderTime += deltaTime;
	const Bool hasWindowResized = screenTexture.size != size || shouldRefresh;
	const Bool hasCameraChanged = camera.has_changed();

	if (hasWindowResized)
	{
		shouldRefresh = false;
		screenTexture.size = size;
		directionTexture.size = size;
		// resize_opengl_texture(screenTexture);
		// resize_opengl_texture(directionTexture);
		// glBindImageTexture(0, directionTexture.gpuId, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
		// glBindImageTexture(1, screenTexture.gpuId, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
	}

	if (hasWindowResized || hasCameraChanged)
	{
		generate_rays(camera);
		renderTime = 0.0f;
	}

	if (frameLimit == 0 || frameCount < frameLimit)
	{
		ray_trace(camera);
		frameCount++;
	}
	//
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

	const IVector2 workGroupsCount = screenTexture.size / WORKGROUP_SIZE;
	
	// glDispatchCompute(workGroupsCount.x, workGroupsCount.y, 1);
}

Void SRaytraceManager::generate_rays(Camera& camera)
{
	const SDisplayManager& displayManager = SDisplayManager::get();
	const glm::ivec2 workGroupsCount = directionTexture.size / WORKGROUP_SIZE;

	camera.set_camera_changed(false);
	frameCount = 1;
	const Float32 theta = glm::radians(camera.get_fov());
	const Float32 h = glm::tan(theta * 0.5f);
	glm::vec2 viewportSize;
	viewportSize.y = 2.0f * h;
	viewportSize.x = viewportSize.y * displayManager.get_aspect_ratio();

	const glm::vec3 viewportU = Float32(viewportSize.x) * camera.get_right();
	const glm::vec3 viewportV = Float32(viewportSize.y) * camera.get_up();

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

Void SRaytraceManager::create_descriptors()
{
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
	return screenTexture;
}

Void SRaytraceManager::refresh()
{
	shouldRefresh = true;
}

Void SRaytraceManager::shutdown()
{
}
