#pragma once
#include "Common/debug_messenger.hpp"
#include "Common/physical_device.hpp"
#include "Common/logical_device.hpp"
#include "Common/swapchain.hpp"
#include "Common/render_pass.hpp"
#include "Common/descriptor_pool.hpp"
#include "Common/pipeline.hpp"

#include <vulkan/vulkan.hpp>

#include "Common/buffer.hpp"


class Camera;
struct Model;
struct Mesh;
struct Texture;
class Image;
struct CommandBuffer;
template<typename Type>
struct Handle;
class Shader;
enum class EShaderType : UInt8;


struct UniformBufferObject
{
	FMatrix4 viewProjection;
};

struct MeshPushConstant
{
	FMatrix4 model;
	UInt32 albedoId;
	UInt32 metallnessId;
	UInt32 roughnessId;
	UInt32 emissionId;
};

class SRenderManager
{
public:
	const String SHADERS_PATH = "Resources/Shaders/";
	const String COMPILED_SHADER_EXTENSION = ".spv";
	const String GLSL_COMPILER_PATH = "D:/VulkanSDK/Bin/glslc.exe";
	const UInt64 MAX_FRAMES_IN_FLIGHT = 2;
	SRenderManager(SRenderManager&) = delete;
	static SRenderManager& get();

	Void startup();

	Handle<Shader> load_shader(const String& filePath, const EShaderType shaderType);
	Void generate_mesh_buffers(DynamicArray<Mesh>& meshes);
	Void create_mesh_buffers(Mesh& mesh);

	Void draw_frame(Camera& camera, const DynamicArray<Model>& models);

	[[nodiscard]]
	const Handle<Shader>& get_shader_handle_by_name(const String& name)  const;
	Shader& get_shader_by_name(const String& name);
	Shader& get_shader_by_handle(const Handle<Shader> handle);

	[[nodiscard]]
	const Handle<CommandBuffer>& get_command_buffer_handle_by_name(const String& name)  const;
	CommandBuffer& get_command_buffer_by_name(const String& name);
	CommandBuffer& get_command_buffer_by_handle(const Handle<CommandBuffer> handle);

	Image& get_image_by_handle(const Handle<Image> handle);
	Buffer& get_buffer_by_handle(const Handle<Buffer> handle);

	Void shutdown();

private:
	SRenderManager()  = default;
	~SRenderManager() = default;

	VkInstance instance;
	DebugMessenger debugMessenger;
	VkSurfaceKHR surface; //TODO: Maybe move to display manager

	PhysicalDevice physicalDevice;
	LogicalDevice logicalDevice;
	Swapchain swapchain;
	RenderPass renderPass;
	DescriptorPool descriptorPool;
	Pipeline graphicsPipeline;
	Pipeline computePipeline;
	VkCommandPool commandPool;

	DynamicArray<Shader> shaders;
	HashMap<String, Handle<Shader>> nameToIdShaders;
	DynamicArray<CommandBuffer> commandBuffers;
	HashMap<String, Handle<CommandBuffer>> nameToIdCommandBuffers;
	DynamicArray<Buffer> buffers; // Contains index and vertex data buffers for meshes
	DynamicArray<Buffer> uniformBuffers;
	DynamicArray<Image> images;

	//DynamicArray<VkSemaphore> computeFinishedSemaphores;
	DynamicArray<VkSemaphore> imageAvailableSemaphores;
	DynamicArray<VkSemaphore> renderFinishedSemaphores;
	//DynamicArray<VkFence>     computeInFlightFences;
	DynamicArray<VkFence>	  inFlightFences;

	Bool isFrameEven;

	Void create_vulkan_instance();
	//TODO: Maybe move to display manager
	DynamicArray<const Char*> get_required_extensions();
	Void create_surface();
	Void create_command_pool(VkCommandPoolCreateFlagBits flags);
	Void create_command_buffers(VkCommandBufferLevel level, const DynamicArray<String>& names);
	Void create_vertex_buffer(Mesh& mesh);
	Void create_index_buffer(Mesh& mesh);
	Void create_uniform_buffer();
	Void create_texture_image(Texture& texture, UInt32 mipLevels);
	Void setup_graphics_descriptors();
	Void create_synchronization_objects();

	Void update_uniform_buffer(UInt32 currentImage, Camera& camera);
	Void record_command_buffer(VkCommandBuffer commandBuffer, UInt32 imageIndex, const DynamicArray<Model>& models);
	Void recreate_swapchain();

	Void generate_mipmaps(Image& image);
	Void copy_buffer_to_image(const Buffer& buffer, Image& image);
	Void transition_image_layout(Image& image, VkImageLayout newLayout);
	Void copy_buffer(const Buffer& source, Buffer& destination);
	Void begin_quick_commands(VkCommandBuffer &commandBuffer);
	Void end_quick_commands(VkCommandBuffer commandBuffer);
	Bool has_stencil_component(VkFormat format);
};

