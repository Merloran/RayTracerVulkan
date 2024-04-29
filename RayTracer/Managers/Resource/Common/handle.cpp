#include "handle.hpp"
#include "texture.hpp"
#include "mesh.hpp"
#include "model.hpp"
#include "material.hpp"
#include "../../Render/Common/shader.hpp"
#include "../../Render/Common/descriptor_pool.hpp"
#include "../../Render/Common/command_buffer.hpp"
#include "../../Render/Common/image.hpp"

const Handle<Model> Handle<Model>::sNone = { -1 };
const Handle<Mesh> Handle<Mesh>::sNone = { -1 };
const Handle<Material> Handle<Material>::sNone = { -1 };
const Handle<Texture> Handle<Texture>::sNone = { -1 };
const Handle<Shader> Handle<Shader>::sNone = { -1 };
const Handle<DescriptorSetData> Handle<DescriptorSetData>::sNone = { -1 };
const Handle<DescriptorLayoutData> Handle<DescriptorLayoutData>::sNone = { -1 };
const Handle<CommandBuffer> Handle<CommandBuffer>::sNone = { -1 };
const Handle<VkCommandPool> Handle<VkCommandPool>::sNone = { -1 };
const Handle<VkSemaphore> Handle<VkSemaphore>::sNone = { -1 };
const Handle<VkFence> Handle<VkFence>::sNone = { -1 };
const Handle<Image> Handle<Image>::sNone = { -1 };