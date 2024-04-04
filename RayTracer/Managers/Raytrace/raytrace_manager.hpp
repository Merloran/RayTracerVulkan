#pragma once
#include "../Resource/Common/handle.hpp"
#include "../Resource/Common/texture.hpp"
#include "../Render/Common/shader.hpp"
#include "../Render/Common/descriptor_pool.hpp"
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

	Handle<Shader> rayGeneration, rayTrace, screenV, screenF;
	Handle<Buffer> vertexesHandle, indexesHandle, materialsHandle, emissionTrianglesHandle;
	BVHBuilder bvh;
	DescriptorPool descriptorPool;
	Texture screenTexture, directionTexture;

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
	Void create_descriptors();
};