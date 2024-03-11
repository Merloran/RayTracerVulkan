#define __STDC_LIB_EXT1__ 1
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_NOEXCEPTION
#include "resource_manager.hpp"

#include "Common/handle.hpp"
#include "Common/model.hpp"
#include "Common/material.hpp"
#include "Common/mesh.hpp"
#include "Common/texture.hpp"

#include <filesystem>

Void SResourceManager::startup()
{
	SPDLOG_INFO("Resource Manager startup.");
	Material defaultMaterial;
	defaultMaterial.name = "DefaultMaterial";

	defaultMaterial.textures.reserve(5);
	defaultMaterial.textures.emplace_back(load_texture(TEXTURES_PATH + "Default/Albedo.png", 
									  				   "DefaultBaseColor", 
													   ETextureType::Albedo));

	defaultMaterial.textures.emplace_back(load_texture(TEXTURES_PATH + "Default/Normal.png", 
													   "DefaultNormal", 
													   ETextureType::Normal));

	defaultMaterial.textures.emplace_back(load_texture(TEXTURES_PATH + "Default/Roughness.png", 
													   "DefaultRoughness", 
													   ETextureType::Roughness));

	defaultMaterial.textures.emplace_back(load_texture(TEXTURES_PATH + "Default/Metalness.png", 
													   "DefaultMetalness", 
													   ETextureType::Metalness));

	defaultMaterial.textures.emplace_back(load_texture(TEXTURES_PATH + "Default/AmbientOcclusion.png", 
													   "DefaultAmbientOcclusion",
													   ETextureType::AmbientOcclusion));

	create_material(defaultMaterial, defaultMaterial.name);
}

SResourceManager& SResourceManager::get()
{
	static SResourceManager instance;
	return instance;
}

Void SResourceManager::load_gltf_asset(const String& filePath)
{
	load_gltf_asset(std::filesystem::path(filePath));
}

Void SResourceManager::load_gltf_asset(const std::filesystem::path & filePath)
{
	tinygltf::Model gltfModel;
	String error;
	String warning;

	tinygltf::TinyGLTF loader;

	if (!loader.LoadASCIIFromFile(&gltfModel, &error, &warning, filePath.string()) || !warning.empty() || !error.empty())
	{
		SPDLOG_ERROR("Failed to load gltf file: {} - {} - {}", filePath.string(), error, warning);
		return;
	}

	for (tinygltf::Node& gltfNode : gltfModel.nodes)
	{
		//TODO: change it later
		if (gltfNode.mesh == -1) //Skip nodes without meshes
		{
			continue;
		}
		load_model(filePath, gltfNode, gltfModel);
	}
}

Handle<Model> SResourceManager::load_model(const std::filesystem::path &filePath, const tinygltf::Node &gltfNode, tinygltf::Model &gltfModel)
{
	const String modelName = filePath.stem().string() + gltfNode.name;
	if (nameToIdModels.find(modelName) != nameToIdModels.end())
	{
		SPDLOG_WARN("Model with name {} already exist!", modelName);
		//TODO: think of it
		return get_model_handle_by_name(modelName);
	}
	tinygltf::Mesh& gltfMesh = gltfModel.meshes[gltfNode.mesh];

	const UInt64 modelId = models.size();
	Model& model		 = models.emplace_back();

	model.meshes.reserve(gltfMesh.primitives.size());
	model.directory = filePath.string();

	for (UInt64 i = 0; i < gltfMesh.primitives.size(); i++)
	{
		tinygltf::Primitive& primitive = gltfMesh.primitives[i];
		String meshName = modelName + std::to_string(i);
		Handle<Mesh> mesh = load_mesh(meshName, primitive, gltfModel);
		model.meshes.push_back(mesh);
		Handle<Material> material = get_material_handle_by_name("DefaultMaterial");
		if (primitive.material >= 0)
		{
			material = load_material(filePath.parent_path(),
									 gltfModel.materials[primitive.material],
									 gltfModel);
		}
		model.materials.push_back(material);
	}

	const Handle<Model> modelHandle{ Int32(modelId) };
	nameToIdModels[modelName] = modelHandle;
	model.name = modelName;

	return modelHandle;
}

Handle<Mesh> SResourceManager::load_mesh(const String& meshName, tinygltf::Primitive& primitive, tinygltf::Model& gltfModel)
{
	if (nameToIdMeshes.find(meshName) != nameToIdMeshes.end())
	{
		SPDLOG_WARN("Mesh with name {} already exist!", meshName);
		//TODO: think of it
		return get_mesh_handle_by_name(meshName);
	}

	const UInt64 meshId = meshes.size();
	Mesh& mesh			= meshes.emplace_back();

	const tinygltf::Accessor& indexesAccessor = gltfModel.accessors[primitive.indices];
	Int32 indexesType = indexesAccessor.componentType;
	Int32 indexesTypeCount = indexesAccessor.type;

	// Load indexes
	if (indexesTypeCount != TINYGLTF_TYPE_SCALAR)
	{
		SPDLOG_ERROR("Mesh indexes not loaded, not supported type: GLTF_TYPE {}; Name {}", indexesTypeCount, meshName);
		meshes.pop_back();
		return Handle<Mesh>::sNone;
	}
	
	switch (indexesType)
	{
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
		{
			process_accessor<UInt16>(gltfModel, indexesAccessor, mesh.indexes);
			break;
		}
		case TINYGLTF_COMPONENT_TYPE_SHORT:
		{
			process_accessor<Int16>(gltfModel, indexesAccessor, mesh.indexes);
			break;
		}
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
		{
			process_accessor<UInt32>(gltfModel, indexesAccessor, mesh.indexes);
			break;
		}
		default:
		{
			SPDLOG_ERROR("Mesh indexes not loaded, not supported type: GLTF_COMPONENT_TYPE {}; Name {}", indexesType, meshName);
			meshes.pop_back();
			return Handle<Mesh>::sNone;
		}
	}

	// Load positions
	const tinygltf::Accessor& positionsAccessor = gltfModel.accessors[primitive.attributes["POSITION"]];
	Int32 positionsType = positionsAccessor.componentType;
	Int32 positionsTypeCount = positionsAccessor.type;

	if (positionsTypeCount != TINYGLTF_TYPE_VEC3)
	{
		SPDLOG_ERROR("Mesh positions not loaded, not supported type: GLTF_TYPE {}; Name {}", positionsTypeCount, meshName);
		meshes.pop_back();
		return Handle<Mesh>::sNone;
	}

	if (positionsType != TINYGLTF_COMPONENT_TYPE_FLOAT)
	{
		SPDLOG_ERROR("Mesh positions not loaded, not supported type: GLTF_COMPONENT_TYPE {}; Name {}", positionsType, meshName);
		meshes.pop_back();
		return Handle<Mesh>::sNone;
	}

	process_accessor<FVector3>(gltfModel, positionsAccessor, mesh.positions);


	// Load normals
	const tinygltf::Accessor& normalsAccessor = gltfModel.accessors[primitive.attributes["NORMAL"]];
	Int32 normalsType = normalsAccessor.componentType;
	Int32 normalsTypeCount = normalsAccessor.type;

	if (normalsTypeCount != TINYGLTF_TYPE_VEC3)
	{
		SPDLOG_ERROR("Mesh normals not loaded, not supported type: GLTF_TYPE {}; Name {}", normalsTypeCount, meshName);
		meshes.pop_back();
		return Handle<Mesh>::sNone;
	}

	if (normalsType != TINYGLTF_COMPONENT_TYPE_FLOAT)
	{
		SPDLOG_ERROR("Mesh normals not loaded, not supported type: GLTF_COMPONENT_TYPE {}; Name {}", normalsType, meshName);
		meshes.pop_back();
		return Handle<Mesh>::sNone;
	}

	process_accessor<glm::vec3>(gltfModel, normalsAccessor, mesh.normals);


	// Load uvs
	const tinygltf::Accessor& uvsAccessor = gltfModel.accessors[primitive.attributes["TEXCOORD_0"]];
	Int32 uvsType = uvsAccessor.componentType;
	Int32 uvsTypeCount = uvsAccessor.type;

	if (uvsTypeCount != TINYGLTF_TYPE_VEC2)
	{
		SPDLOG_ERROR("Mesh uvs not loaded, not supported type: GLTF_TYPE {}; Name {}", uvsTypeCount, meshName);
		meshes.pop_back();
		return Handle<Mesh>::sNone;
	}

	if (uvsType != TINYGLTF_COMPONENT_TYPE_FLOAT)
	{
		SPDLOG_ERROR("Mesh uvs not loaded, not supported type: GLTF_COMPONENT_TYPE {}; Name {}", uvsType, meshName);
		meshes.pop_back();
		return Handle<Mesh>::sNone;
	}

	process_accessor<FVector2>(gltfModel, uvsAccessor, mesh.uvs);

	const Handle<Mesh> meshHandle{ Int32(meshId) };
	nameToIdMeshes[meshName] = meshHandle;
	mesh.name = meshName;

	return meshHandle;
}

Handle<Material> SResourceManager::load_material(const std::filesystem::path& assetPath, tinygltf::Material& gltfMaterial, const tinygltf::Model& gltfModel)
{
	if (nameToIdMaterials.find(gltfMaterial.name) != nameToIdMaterials.end())
	{
		SPDLOG_WARN("Material with name {} already exist!", gltfMaterial.name);
		//TODO: think of it
		return get_material_handle_by_name(gltfMaterial.name);
	}

	const UInt64 materialId = materials.size();
	Material& material		= materials.emplace_back();

	const Int32 albedoId			  = gltfMaterial.pbrMetallicRoughness.baseColorTexture.index;
	const Int32 metallicRoughnessId   = gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index;
	const Int32 normalId			  = gltfMaterial.normalTexture.index;
	const Int32 ambientOcclusionId    = gltfMaterial.occlusionTexture.index;
	const Int32 emissionId		      = gltfMaterial.emissiveTexture.index;

	material.textures.reserve(5);
	if (albedoId >= 0)
	{
		const tinygltf::Texture& texture = gltfModel.textures[albedoId];
		const tinygltf::Image& image = gltfModel.images[texture.source];
		const std::filesystem::path textureName = image.uri;
		material.textures.emplace_back(load_texture(assetPath / image.uri, 
													textureName.stem().string(), 
													ETextureType::Albedo));
	}

	if (metallicRoughnessId >= 0)
	{
		const tinygltf::Texture& texture = gltfModel.textures[metallicRoughnessId];
		const tinygltf::Image& image = gltfModel.images[texture.source];
		const std::filesystem::path textureName = image.uri;
		material.textures.emplace_back(load_texture(assetPath / image.uri, 
													textureName.stem().string(), 
													ETextureType::RM));
	}

	if (normalId >= 0)
	{
		const tinygltf::Texture& texture = gltfModel.textures[normalId];
		const tinygltf::Image& image = gltfModel.images[texture.source];
		const std::filesystem::path textureName = image.uri;
		material.textures.emplace_back(load_texture(assetPath / image.uri, 
													textureName.stem().string(), 
													ETextureType::Normal));
	}

	if (ambientOcclusionId >= 0)
	{
		const tinygltf::Texture& texture = gltfModel.textures[ambientOcclusionId];
		const tinygltf::Image& image = gltfModel.images[texture.source];
		const std::filesystem::path textureName = image.uri;
		material.textures.emplace_back(load_texture(assetPath / image.uri, 
													textureName.stem().string(), 
													ETextureType::AmbientOcclusion));
	}

	if (emissionId >= 0)
	{
		const tinygltf::Texture& texture = gltfModel.textures[emissionId];
		const tinygltf::Image& image = gltfModel.images[texture.source];
		const std::filesystem::path textureName = image.uri;
		material.textures.emplace_back(load_texture(assetPath / image.uri, 
													textureName.stem().string(), 
													ETextureType::Emission));
	}

	auto iterator = gltfMaterial.extensions.find("KHR_materials_ior");
	if (iterator != gltfMaterial.extensions.end())
	{
		const tinygltf::Value &indexOfRefractionValue = iterator->second.Get("ior");
		if (indexOfRefractionValue.IsNumber())
		{
			material.indexOfRefraction = Float32(indexOfRefractionValue.GetNumberAsDouble());
		}
	}

	const Handle<Material> materialHandle{ Int32(materialId) };
	nameToIdMaterials[gltfMaterial.name] = materialHandle;
	material.name = gltfMaterial.name;

	return materialHandle;
}

Handle<Texture> SResourceManager::load_texture(const String& filePath, const String& textureName, const ETextureType type)
{
	return load_texture(std::filesystem::path(filePath), textureName, type);
}

Handle<Texture> SResourceManager::load_texture(const std::filesystem::path& filePath, const String& textureName, const ETextureType type)
{
	if (nameToIdTextures.find(textureName) != nameToIdTextures.end())
	{
		SPDLOG_WARN("Texture with name {} already exist!", textureName);
		//TODO: think of it
		return get_texture_handle_by_name(textureName);
	}

	
	const UInt64 textureId = textures.size();
	Texture& texture	   = textures.emplace_back();

	if (type == ETextureType::HDR)
	{
		texture.data = reinterpret_cast<UInt8*>(stbi_loadf(filePath.string().c_str(), 
														   &texture.size.x, 
														   &texture.size.y, 
														   &texture.channels, 
														   0));
	} else {
		texture.data = stbi_load(filePath.string().c_str(), 
								 &texture.size.x, 
								 &texture.size.y, 
								 &texture.channels, 
								 0);
	}

	texture.type = type;

	if (!texture.data)
	{
		SPDLOG_ERROR("Texture {} loading failed.", filePath.string());
		textures.pop_back();
		return Handle<Texture>::sNone;
	}

	const Handle<Texture> textureHandle{ Int32(textureId) };
	nameToIdTextures[textureName] = textureHandle;
	texture.name = textureName;

	return textureHandle;
}

Handle<Material> SResourceManager::create_material(const Material& material, const String& name)
{
	const UInt64 materialId = materials.size();
	materials.emplace_back(material);
	const Handle<Material> materialHandle{ Int32(materialId) };
	nameToIdMaterials[name] = materialHandle;
	return materialHandle;
}

Model& SResourceManager::get_model_by_name(const String& name)
{
	const auto& iterator = nameToIdModels.find(name);
	if (iterator == nameToIdModels.end() || iterator->second.id < 0 || iterator->second.id >= Int32(models.size()))
	{
		SPDLOG_WARN("Model {} not found, returned default.", name);
		return models[0];
	}

	return models[iterator->second.id];
}

Model& SResourceManager::get_model_by_handle(const Handle<Model> handle)
{
	if (handle.id < 0 || handle.id >= Int32(models.size()))
	{
		SPDLOG_WARN("Model {} not found, returned default.", handle.id);
		return models[0];
	}
	return models[handle.id];
}

Mesh& SResourceManager::get_mesh_by_name(const String& name)
{
	const auto& iterator = nameToIdMeshes.find(name);
	if (iterator == nameToIdMeshes.end() || iterator->second.id < 0 || iterator->second.id >= Int32(meshes.size()))
	{
		SPDLOG_WARN("Mesh {} not found, returned default.", name);
		return meshes[0];
	}

	return meshes[iterator->second.id];
}

Mesh& SResourceManager::get_mesh_by_handle(const Handle<Mesh> handle)
{
	if (handle.id < 0 || handle.id >= Int32(meshes.size()))
	{
		SPDLOG_WARN("Mesh {} not found, returned default.", handle.id);
		return meshes[0];
	}
	return meshes[handle.id];
}

Material& SResourceManager::get_material_by_name(const String& name)
{
	const auto& iterator = nameToIdMaterials.find(name);
	if (iterator == nameToIdMaterials.end() || iterator->second.id < 0 || iterator->second.id >= Int32(materials.size()))
	{
		SPDLOG_WARN("Material {} not found, returned default.", name);
		return materials[0];
	}

	return materials[iterator->second.id];
}

Material& SResourceManager::get_material_by_handle(const Handle<Material> handle)
{
	if (handle.id < 0 || handle.id >= Int32(materials.size()))
	{
		SPDLOG_WARN("Material {} not found, returned default.", handle.id);
		return get_default_material();
	}
	return materials[handle.id];
}

Material& SResourceManager::get_default_material()
{
	return get_material_by_name("DefaultMaterial");
}

Texture& SResourceManager::get_texture_by_name(const String& name)
{
	const auto& iterator = nameToIdTextures.find(name);
	if (iterator == nameToIdTextures.end() || iterator->second.id < 0 || iterator->second.id >= Int32(textures.size()))
	{
		SPDLOG_WARN("Texture {} not found, returned default.", name);
		return textures[0];
	}

	return textures[iterator->second.id];
}

Texture& SResourceManager::get_texture_by_handle(const Handle<Texture> handle)
{
	if (handle.id < 0 || handle.id >= Int32(textures.size()))
	{
		SPDLOG_WARN("Texture {} not found, returned default.", handle.id);
		return textures[0];
	}
	return textures[handle.id];
}

const Handle<Model>& SResourceManager::get_model_handle_by_name(const String& name) const
{
	const auto& iterator = nameToIdModels.find(name);
	if (iterator == nameToIdModels.end())
	{
		SPDLOG_WARN("Model handle {} not found, returned none.", name);
		return Handle<Model>::sNone;
	}
	return iterator->second;
}

const Handle<Mesh>& SResourceManager::get_mesh_handle_by_name(const String& name) const
{
	const auto& iterator = nameToIdMeshes.find(name);
	if (iterator == nameToIdMeshes.end())
	{
		SPDLOG_WARN("Mesh handle {} not found, returned none.", name);
		return Handle<Mesh>::sNone;
	}
	return iterator->second;
}

const Handle<Material>& SResourceManager::get_material_handle_by_name(const String& name) const
{
	const auto& iterator = nameToIdMaterials.find(name);
	if (iterator == nameToIdMaterials.end())
	{
		SPDLOG_WARN("Material handle {} not found, returned none.", name);
		return get_material_handle_by_name("DefaultMaterial");
	}
	return iterator->second;
}

const Handle<Texture>& SResourceManager::get_texture_handle_by_name(const String& name) const
{
	const auto& iterator = nameToIdTextures.find(name);
	if (iterator == nameToIdTextures.end())
	{
		SPDLOG_WARN("Texture handle {} not found, returned none.", name);
		return Handle<Texture>::sNone;
	}
	return iterator->second;
}

const DynamicArray<Model>& SResourceManager::get_models() const
{
	return models;
}

const DynamicArray<Mesh>& SResourceManager::get_meshes() const
{
	return meshes;
}

const DynamicArray<Material>& SResourceManager::get_materials() const
{
	return materials;
}

const DynamicArray<Texture>& SResourceManager::get_textures() const
{
	return textures;
}

void SResourceManager::shutdown()
{
	SPDLOG_INFO("Resource Manager shutdown.");
	nameToIdTextures.clear();
	for (Texture& texture : textures)
	{
		stbi_image_free(texture.data);
	}
	textures.clear();

	nameToIdMaterials.clear();
	materials.clear();

	nameToIdMeshes.clear();
	meshes.clear();

	nameToIdModels.clear();
	models.clear();
}