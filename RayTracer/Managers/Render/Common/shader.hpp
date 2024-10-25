#pragma once
#include <vulkan/vulkan.hpp>

class LogicalDevice;

enum class EShaderType : UInt8
{
	None = 0U,
	Vertex,
	Geometry,
	Fragment,
	Compute,
	Count
};

class Shader
{
public:
    Void create(const String& filePath,
                const String& functionName,
                const EShaderType shaderType, 
                const LogicalDevice& logicalDevice, 
                const VkAllocationCallbacks* allocator);

    Bool recreate(const LogicalDevice& logicalDevice, 
                  const VkAllocationCallbacks* allocator);

    [[nodiscard]]
	const String& get_name() const;
    [[nodiscard]]
	const String& get_function_name() const;
    [[nodiscard]]
	const String& get_file_path() const;
    [[nodiscard]]
    const VkShaderModule& get_module() const;
    [[nodiscard]]
    EShaderType get_type() const;

    Void clear(const LogicalDevice& logicalDevice, const VkAllocationCallbacks* allocator);

private:
    VkShaderModule module;
    String code;
    DynamicArray<UInt32> compiledCode;
    String filePath;
    String name, functionName;
    EShaderType type;

    Void compose_name(const String& filePath, EShaderType type);
    Bool compile(const String& filePath);
    Bool load(const String& filePath);
    Bool create_module(const LogicalDevice& logicalDevice, const VkAllocationCallbacks* allocator);
};