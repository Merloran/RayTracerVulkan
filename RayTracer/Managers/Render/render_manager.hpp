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


struct Texture;
class Image;
struct CommandBuffer;
template<typename Type>
struct Handle;
class Shader;
enum class EShaderType : UInt8;

class SRenderManager
{
public:
	const String SHADERS_PATH = "Resources/Shaders/";
	const String COMPILED_SHADER_EXTENSION = ".spv";
	const String GLSL_COMPILER_PATH = "D:/VulkanSDK/Bin/glslc.exe";
	SRenderManager(SRenderManager&) = delete;
	static SRenderManager& get();

	Void startup();

	Handle<Shader> load_shader(const String& filePath, const EShaderType shaderType);

	[[nodiscard]]
	const Handle<Shader>& get_shader_handle_by_name(const String& name)  const;
	Shader& get_shader_by_name(const String& name);
	Shader& get_shader_by_handle(const Handle<Shader> handle);

	[[nodiscard]]
	const Handle<CommandBuffer>& get_command_buffer_handle_by_name(const String& name)  const;
	CommandBuffer& get_command_buffer_by_name(const String& name);
	CommandBuffer& get_command_buffer_by_handle(const Handle<CommandBuffer> handle);

	Image& get_image_by_handle(const Handle<Image> handle);

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
	DynamicArray<Image> images;
	Handle<Image> colorImageHandle = Handle<Image>::sNone;
	Handle<Image> depthImageHandle = Handle<Image>::sNone;
	DynamicArray<VkFramebuffer> framebuffers;

	Void create_vulkan_instance();
	//TODO: Maybe move to display manager
	DynamicArray<const Char*> get_required_extensions();
	Void create_surface();
	Void create_command_pool(VkCommandPoolCreateFlagBits flags);
	Void create_command_buffers(VkCommandBufferLevel level, const DynamicArray<String>& names);
	Void create_color_image();
	Void create_depth_image();
	Void create_framebuffers();
	Void create_texture_image(Texture& texture, UInt32 mipLevels);
	Void setup_graphics_descriptors();

	Void generate_mipmaps(Image& image);
	Void copy_buffer_to_image(const Buffer& buffer, Image& image);
	Void transition_image_layout(Image& image, VkImageLayout newLayout);
	Void copy_buffer(const Buffer& source, Buffer& destination);
	Void begin_quick_commands(VkCommandBuffer &commandBuffer);
	Void end_quick_commands(VkCommandBuffer commandBuffer);
	Bool has_stencil_component(VkFormat format);
};

