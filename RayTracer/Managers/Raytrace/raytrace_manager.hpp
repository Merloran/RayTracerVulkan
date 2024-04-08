#pragma once
#include "../Resource/Common/handle.hpp"
#include "../Resource/Common/texture.hpp"
#include "../Render/Common/shader.hpp"
#include "../Render/Common/descriptor_pool.hpp"
#include "../Render/Common/pipeline.hpp"
#include "../Render/Common/render_pass.hpp"
#include "../Render/Common/swapchain.hpp"
#include "Common/bvh_builder.hpp"


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
	FVector3 cameraPosition;
	FVector3 originPixel;
	FVector3 pixelDeltaU;
	FVector3 pixelDeltaV;
	IVector2 imageSize;
};

struct RaytraceConstants
{
	FVector3 backgroundColor;
	FVector3 cameraPosition;
	FVector3 pixelDeltaU;
	FVector3 pixelDeltaV;
	FVector2 viewBounds;
	Float32  time;
	IVector2 imageSize;
	Int32	 frameCount;
	Int32	 maxBouncesCount;
	Int32	 trianglesCount;
	Int32	 emissionTrianglesCount;
	Int32	 rootId;
	Int32	 environmentMapId;
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
	DescriptorPool pool;
	Pipeline rayGenerationPipeline, raytracePipeline, postprocessPipeline;
	RenderPass postprocessPass;
	Swapchain postprocessSwapchain;

	Handle<Shader> rayGeneration, raytrace, screenV, screenF;
	Handle<Buffer> vertexesHandle, indexesHandle, materialsHandle, bvhHandle, emissionTrianglesHandle;
	BVHBuilder bvh;
	DescriptorPool descriptorPool;
	Texture accumulationTexture, directionTexture;

	DynamicArray<GPUMaterial> materials;
	DynamicArray<Vertex> vertexes;
	DynamicArray<UInt32> indexes;
	DynamicArray<UInt32> emissionTriangles;

	FVector3 originPixel, pixelDeltaU, pixelDeltaV, backgroundColor;
	Float32 renderTime;
	Int32 frameCount, trianglesCount;
	Bool shouldRefresh;


	Void ray_trace(Camera& camera);
	Void generate_rays(Camera& camera);
	Void create_pipelines();
	Void create_descriptors();
	Void setup_descriptors();
};