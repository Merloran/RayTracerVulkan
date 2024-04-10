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
class CommandBuffer;
template<typename Type>
struct Handle;
class Shader;
enum class EShaderType : UInt8;


struct UniformBufferObject
{
	FMatrix4 viewProjection;
	Float32 time;
};

struct VertexConstants
{
	FMatrix4 model;
};

struct FragmentConstants
{
	UInt32 albedoId;
	UInt32 metalnessId;
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
	Void setup_imgui();

	Handle<Shader> load_shader(const String& filePath, const EShaderType shaderType, const String& functionName = "main");
	Void generate_mesh_buffers(DynamicArray<Mesh>& meshes);
	Void create_mesh_buffers(Mesh& mesh);
	Void generate_texture_images(DynamicArray<Texture>& textures);
	Void create_texture_image(Texture& texture, UInt32 mipLevels = 1);
	Handle<Image> create_image(const UVector2& size, 
							   VkFormat format, 
							   VkImageUsageFlagBits usage, 
							   VkImageTiling tiling,
							   UInt32 mipLevels = 1);
	Void transition_image_layout(Image& image,
								 VkPipelineStageFlags sourceStage,
								 VkPipelineStageFlags destinationStage,
								 VkImageLayout newLayout);
	Void resize_image(const UVector2& newSize, Handle<Image> image);
	Void setup_graphics_descriptors(const DynamicArray<Texture>& textures);
	Void reload_shaders();
	
	Void render_imgui();
	Void render(Camera& camera, const DynamicArray<Model>& models, Float32 time);

	[[nodiscard]]
	VkSurfaceKHR get_surface() const;
	[[nodiscard]]
	const PhysicalDevice& get_physical_device() const;
	[[nodiscard]]
	const LogicalDevice& get_logical_device() const;

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
	VkCommandPool get_command_pool_by_handle(const Handle<VkCommandPool> handle);

	template <typename Type>
	Handle<Buffer> create_dynamic_buffer(VkBufferUsageFlagBits usage, 
										VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT 
																		 | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
	{
		const VkDeviceSize bufferSize = sizeof(Type);
		const Handle<Buffer> handle = { Int32(uniformBuffers.size()) };
		Buffer& buffer = uniformBuffers.emplace_back();

		buffer.create(physicalDevice,
					  logicalDevice,
					  bufferSize,
					  usage,
					  properties,
					  nullptr);
		vkMapMemory(logicalDevice.get_device(), buffer.get_memory(), 0, bufferSize, 0, buffer.get_mapped_memory());

		return handle;
	}
	template <typename Type>
	Void update_dynamic_buffer(const Type& data, Buffer& buffer)
	{
		memcpy(*buffer.get_mapped_memory(), &data, sizeof(Type));
	}

	template<typename Type>
	Handle<Buffer> create_static_buffer(const DynamicArray<Type>& data, 
									   VkBufferUsageFlagBits usage, 
									   VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
	{
		const UInt64 bufferSize = sizeof(data[0]) * data.size();

		Buffer stagingBuffer;
		stagingBuffer.create(physicalDevice,
							 logicalDevice,
							 bufferSize,
							 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
							 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
							 nullptr);

		Void* addressToGPU;
		vkMapMemory(logicalDevice.get_device(), stagingBuffer.get_memory(), 0, bufferSize, 0, &addressToGPU);
		memcpy(addressToGPU, data.data(), bufferSize);
		vkUnmapMemory(logicalDevice.get_device(), stagingBuffer.get_memory());

		const Handle<Buffer> handle = { Int32(buffers.size()) };
		Buffer& buffer = buffers.emplace_back();

		buffer.create(physicalDevice,
						   logicalDevice,
						   bufferSize,
						   VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
						   properties,
						   nullptr);

		copy_buffer(stagingBuffer, buffer);

		stagingBuffer.clear(logicalDevice, nullptr);

		return handle;
	}

	Handle<VkCommandPool> create_command_pool(VkCommandPoolCreateFlagBits flags);
	Void create_command_buffers(Handle<VkCommandPool> handle, VkCommandBufferLevel level, const DynamicArray<String>& names);
	Void create_command_buffers(VkCommandPool pool, VkCommandBufferLevel level, const DynamicArray<String>& names);
	Void recreate_swapchain(Swapchain &swapchain, RenderPass &renderPass) const;

	Void shutdown_imgui();
	Void shutdown();

private:
	SRenderManager()  = default;
	~SRenderManager() = default;

	VkInstance instance;
	DebugMessenger debugMessenger;
	VkSurfaceKHR surface;

	PhysicalDevice physicalDevice;
	LogicalDevice logicalDevice;
	Swapchain swapchain;
	RenderPass renderPass;
	DescriptorPool descriptorPool;
	Pipeline graphicsPipeline;
	Pipeline computePipeline;
	DynamicArray<Shader> shaders;
	HashMap<String, Handle<Shader>> nameToIdShaders;
	
	Handle<VkCommandPool> graphicsPool;
	Handle<VkCommandPool> quickPool;
	DynamicArray<VkCommandPool> commandPools;

	Handle<CommandBuffer> quickCommandBuffer;
	DynamicArray<CommandBuffer> commandBuffers;
	HashMap<String, Handle<CommandBuffer>> nameToIdCommandBuffers;

	DynamicArray<Buffer> buffers; // Contains index and vertex data buffers for meshes
	DynamicArray<Buffer> uniformBuffers;
	DynamicArray<Image> images;
	
	DynamicArray<VkSemaphore> imageAvailableSemaphores;
	DynamicArray<VkSemaphore> renderFinishedSemaphores;
	DynamicArray<VkFence>	  inFlightFences;

	Bool isFrameEven;
	VkDescriptorPool imguiDescriptorPool;


	Void create_vulkan_instance();
	DynamicArray<const Char*> get_required_extensions();
	Void create_surface();
	Void create_graphics_descriptors();
	Void create_synchronization_objects();
	
	Void record_commands(const CommandBuffer &commandBuffer, const DynamicArray<Model>& models);

	Void generate_mipmaps(Image& image);
	Void copy_buffer_to_image(const Buffer& buffer, Image& image);
	Void copy_buffer(const Buffer& source, Buffer& destination);
	Void create_quick_commands();
	Void begin_quick_commands(VkCommandBuffer &commandBuffer);
	Void end_quick_commands(VkCommandBuffer commandBuffer);
	Bool has_stencil_component(VkFormat format);

	static Void s_check_vk_result(VkResult error);
};

