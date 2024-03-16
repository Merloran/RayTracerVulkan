#include "shader.hpp"

#include "logical_device.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>


Void Shader::create(const String& filePath, const String& destinationPath, const String& compilerPath, const EShaderType shaderType, const LogicalDevice& logicalDevice, const VkAllocationCallbacks* allocator)
{
    const std::filesystem::path path(filePath);
    this->filePath = filePath;
    this->destinationPath = destinationPath;
    name = path.stem().string();
    type = shaderType;

	const Bool isCompiled = compile(filePath, destinationPath, compilerPath);

    if (!isCompiled)
    {
        return;
    }
    const Bool isLoaded = load(filePath, destinationPath);

    if (!isLoaded)
    {
        return;
    }

    create_module(logicalDevice, allocator);
}

Void Shader::recreate(const String& compilerPath, const LogicalDevice& logicalDevice, const VkAllocationCallbacks* allocator)
{
    clear(logicalDevice, allocator);

    const Bool isCompiled = compile(filePath, destinationPath, compilerPath);

    if (!isCompiled)
    {
        return;
    }

    const Bool isLoaded = load(filePath, destinationPath);
    if (!isLoaded)
    {
        return;
    }

    create_module(logicalDevice, allocator);
}

const String& Shader::get_name() const
{
    return name;
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

Void Shader::create_module(const LogicalDevice& logicalDevice, const VkAllocationCallbacks* allocator)
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const UInt32*>(code.data());
    
    if (vkCreateShaderModule(logicalDevice.get_device(), &createInfo, allocator, &module) != VK_SUCCESS)
    {
        SPDLOG_ERROR("Failed to create shader module {}", name);
    }
}

Void Shader::clear(const LogicalDevice& logicalDevice, const VkAllocationCallbacks* allocator)
{
    code.clear();
    vkDestroyShaderModule(logicalDevice.get_device(), module, allocator);
    module = nullptr;
}