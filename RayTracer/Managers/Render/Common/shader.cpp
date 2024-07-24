#include "shader.hpp"

#include "logical_device.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>


Void Shader::create(const String& filePath, const String& destinationPath, const String& compilerPath, const String& functionName, const EShaderType shaderType, const LogicalDevice& logicalDevice, const VkAllocationCallbacks* allocator)
{
    this->filePath = filePath;
    this->destinationPath = destinationPath;
    this->functionName = functionName;
    type = shaderType;
	compose_name(filePath, shaderType);
#ifndef NDEBUG
	const Bool isCompiled = compile(filePath, destinationPath, compilerPath);
    if (!isCompiled)
    {
        return;
    }
#endif
    const Bool isLoaded = load(filePath, destinationPath);
    if (!isLoaded)
    {
        return;
    }

    create_module(logicalDevice, allocator);
}

Bool Shader::recreate(const String& compilerPath, const LogicalDevice& logicalDevice, const VkAllocationCallbacks* allocator)
{
    clear(logicalDevice, allocator);

    const Bool isCompiled = compile(filePath, destinationPath, compilerPath);
    if (!isCompiled)
    {
        return false;
    }

    const Bool isLoaded = load(filePath, destinationPath);
    if (!isLoaded)
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

Bool Shader::compile(const String& filePath, const String& destinationPath, const String& compilerPath)
{
    const String compileParameters = "-o";
    std::stringstream command;
    command << compilerPath << " " << filePath << " " << compileParameters << " " << destinationPath;
    const Int32 result = system(command.str().c_str());

    if (result == 0)
    {
        SPDLOG_INFO("Successfully compiled {} shader", name);
    } else {
        SPDLOG_ERROR("Compiling {} ended with code: {}", name, result);
        return false;
    }
    return true;
}

Bool Shader::load(const String& filePath, const String& destinationPath)
{
    std::ifstream file(destinationPath, std::ios::ate | std::ios::binary);
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
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const UInt32*>(code.data());
    
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
    vkDestroyShaderModule(logicalDevice.get_device(), module, allocator);
    module = VK_NULL_HANDLE;
}