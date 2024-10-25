#include "shader.hpp"

#include "logical_device.hpp"
#include <filesystem>
#include <fstream>
#include <magic_enum.hpp>
#include <glslang/SPIRV/GlslangToSpv.h>
#include <glslang/Public/ShaderLang.h>
#include <glslang/Public/ResourceLimits.h>


Void Shader::create(const String& filePath, const String& functionName, const EShaderType shaderType, const LogicalDevice& logicalDevice, const VkAllocationCallbacks* allocator)
{
    this->filePath = filePath;
    this->functionName = functionName;
    type = shaderType;
	compose_name(filePath, shaderType);

    const Bool isLoaded = load(filePath);
    if (!isLoaded)
    {
        return;
    }

	const Bool isCompiled = compile(filePath);
    if (!isCompiled)
    {
        return;
    }


    create_module(logicalDevice, allocator);
}

Bool Shader::recreate(const LogicalDevice& logicalDevice, const VkAllocationCallbacks* allocator)
{
    clear(logicalDevice, allocator);

    const Bool isLoaded = load(filePath);
    if (!isLoaded)
    {
        return false;
    }

    const Bool isCompiled = compile(filePath);
    if (!isCompiled)
    {
        return false;
    }


    return create_module(logicalDevice, allocator);
}

const String& Shader::get_name() const
{
    return name;
}

const String& Shader::get_function_name() const
{
    return functionName;
}

const String& Shader::get_file_path() const
{
    return filePath;
}

const VkShaderModule& Shader::get_module() const
{
    return module;
}

EShaderType Shader::get_type() const
{
    return type;
}

Void Shader::compose_name(const String& filePath, EShaderType type)
{
    String prefix;
	switch (type)
	{
		case EShaderType::Vertex:
		{
            prefix = "V";
			break;
		}
		case EShaderType::Geometry:
        {
            prefix = "G";
            break;
        }
		case EShaderType::Fragment:
        {
            prefix = "F";
            break;
        }
		case EShaderType::Compute:
        {
            prefix = "C";
            break;
        }
        case EShaderType::None:
		default:
        {
            prefix = "N";
            break;
        }
	}

    const std::filesystem::path path(filePath);
	name = prefix + path.stem().string();
}

Bool Shader::compile(const String& filePath)
{
    const std::filesystem::path path(filePath);
    const String filename = path.filename().string();

    EShLanguage stage;
    switch (type)
    {
        case EShaderType::Vertex:
        {
            stage = EShLangVertex;
            break;
        }
        case EShaderType::Geometry:
        {
            stage = EShLangGeometry;
            break;
        }
        case EShaderType::Fragment:
        {
            stage = EShLangFragment;
            break;
        }
        case EShaderType::Compute:
        {
            stage = EShLangCompute;
            break;
        }
        case EShaderType::Count:
        case EShaderType::None:
        default:
        {
            SPDLOG_ERROR("Not supported shader type: {}", magic_enum::enum_name(type));
            return false;
        }
    }

    glslang::TShader shader(stage);

    const Char* shaderStrings = code.c_str();
    shader.setStrings(&shaderStrings, 1);

    shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientVulkan, 460);
    shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_3);
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_6);
    
    if (!shader.parse(GetDefaultResources(), 100, false, EShMsgDefault))
    {
        SPDLOG_ERROR("GLSL parsing failed: {}", shader.getInfoLog());
        return false;
    }

    glslang::TProgram program;
    program.addShader(&shader);

    if (!program.link(EShMsgDefault)) 
    {
        SPDLOG_ERROR("GLSL linking failed: {}", program.getInfoLog());
        return false;
    }

    glslang::GlslangToSpv(*program.getIntermediate(stage), compiledCode);

    return true;
}

Bool Shader::load(const String& filePath)
{
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        SPDLOG_ERROR("Failed to open shader {}", filePath);
        return false;
    }

    code.resize(file.tellg());
    file.seekg(0);
    file.read(code.data(), Int64(code.size()));
    file.close();

    return true;
}

Bool Shader::create_module(const LogicalDevice& logicalDevice, const VkAllocationCallbacks* allocator)
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = compiledCode.size() * sizeof(UInt32);
    createInfo.pCode = compiledCode.data();
    
    if (vkCreateShaderModule(logicalDevice.get_device(), &createInfo, allocator, &module) != VK_SUCCESS)
    {
        SPDLOG_ERROR("Failed to create shader module {}", name);
        return false;
    }

    return true;
}

Void Shader::clear(const LogicalDevice& logicalDevice, const VkAllocationCallbacks* allocator)
{
    code.clear();
    compiledCode.clear();
    vkDestroyShaderModule(logicalDevice.get_device(), module, allocator);
    module = VK_NULL_HANDLE;
}