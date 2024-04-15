#pragma once
#include "../Resource/Common/handle.hpp"
#include "../Resource/Common/texture.hpp"
#include "../Render/Common/shader.hpp"
#include "../Render/Common/descriptor_pool.hpp"
#include "../Render/Common/pipeline.hpp"
#include "../Render/Common/render_pass.hpp"
#include "../Render/Common/swapchain.hpp"
#include "Common/bvh_builder.hpp"


class CommandBuffer;

struct GPUMaterial
{
	Int32 albedo;
	Int32 normal;
	Int32 roughness;
	Int32 metalness;
	Int32 emission;
	Float32 indexOfRefraction;
};

struct RayGenerationConstants
{
	alignas(16) FVector3 cameraPosition;
	alignas(16) FVector3 originPixel;
	alignas(16) FVector3 pixelDeltaU;
	alignas(16) FVector3 pixelDeltaV;
	IVector2 imageSize;
};

struct RaytraceConstants
{
	alignas(16) FVector3 backgroundColor;
	alignas(16) FVector3 cameraPosition;
	alignas(16) FVector3 pixelDeltaU;
	alignas(16) FVector3 pixelDeltaV;
	FVector2 viewBounds;
	IVector2 imageSize;
	Float32  time;
	Int32	 frameCount;
	Int32	 maxBouncesCount;
	Int32	 trianglesCount;
	Int32	 emissionTrianglesCount;
	Int32	 rootId;
	Int32	 environmentMapId;
};

struct PostprocessConstants
{
	IVector2 imageSize;
	Float32 invFrameCount;
};

struct Vertex;
class Buffer;
class Camera;

class SRaytraceManager
{
public:
	SRaytraceManager(SRaytraceManager&) = delete;
	static SRaytraceManager& get();

	Void startup();

	Void update(Camera& camera, Float32 deltaTime);

	[[nodiscard]]
	Int32 get_frame_count() const;
	[[nodiscard]]
	FVector3 get_background_color() const;
	[[nodiscard]]
	const Texture& get_screen_texture() const;

	Void refresh();
	Void shutdown();

	Int32 maxBouncesCount;
	Int32 frameLimit;

private:
	SRaytraceManager() = default;
	~SRaytraceManager() = default;
	static constexpr IVector2 WORKGROUP_SIZE{ 16, 16 };
	DescriptorPool raytracePool, rayGenerationPool, postprocessPool;
	Pipeline rayGenerationPipeline, raytracePipeline, postprocessPipeline;
	RenderPass postprocessPass;

	Handle<Buffer> quadIndexes, quadPositions, quadUvs;
	Handle<VkCommandPool> raytraceCommandPool;
	Handle<CommandBuffer> raytraceBuffer, renderBuffer;
	Handle<Shader> rayGeneration, raytrace, screenV, screenF;
	Handle<Buffer> vertexesHandle, indexesHandle, materialsHandle, bvhHandle, emissionTrianglesHandle;
	Handle<DescriptorSetData> sceneData, accumulationImage, directionImage, bindlessTextures;
	BVHBuilder bvh;
	Texture accumulationTexture, directionTexture;

	DynamicArray<GPUMaterial> materials;
	DynamicArray<Vertex> vertexes;
	DynamicArray<UInt32> indexes;
	DynamicArray<UInt32> emissionTriangles;

	FVector3 originPixel, pixelDeltaU, pixelDeltaV, backgroundColor;
	Float32 renderTime;
	Int32 frameCount, trianglesCount;
	Bool shouldRefresh;
	VkFence raytraceInFlight, renderInFlight;
	VkSemaphore imageAvailable, renderFinished, rayGenerationFinished, raytraceFinished;
	Bool firstFrame, areRaysRegenerated;

	Void resize_images(const UVector2& size);
	Void generate_rays(Camera& camera);
	Void ray_trace(Camera& camera);
	Void render();
	Void create_pipelines();
	Void create_descriptors();
	Void setup_descriptors();
	Void create_quad_buffers();
	Void create_synchronization_objects();
};