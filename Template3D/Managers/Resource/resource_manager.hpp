#pragma once
#include <tiny_gltf.h>

template<typename Type>
struct Handle;


namespace std
{
	namespace filesystem
	{
		class path;
	}
}

struct Texture;
struct Material;
struct Model;
struct Mesh;
enum class ETextureType : Int16;

class SResourceManager
{
public:
	const String TEXTURES_PATH = "Resources/Textures/";
	const String ASSETS_PATH   = "Resources/Assets/";

	SResourceManager(SResourceManager&) = delete;

	static SResourceManager& get();
	Void startup();

	Void load_gltf_asset(const String& filePath);
	Void load_gltf_asset(const std::filesystem::path &filePath);

	Handle<Model>    load_model(const std::filesystem::path &filePath, 
								const tinygltf::Node &gltfNode, 
								tinygltf::Model &gltfModel);
	Handle<Mesh>     load_mesh(const String &meshName, tinygltf::Primitive &primitive, tinygltf::Model &gltfModel);
	Handle<Material> load_material(const std::filesystem::path &assetPath, 
								   tinygltf::Material &gltfMaterial,
								   const tinygltf::Model &gltfModel);
	Handle<Texture>  load_texture(const std::filesystem::path &filePath, const String &textureName, const ETextureType type);
	Handle<Texture>  load_texture(const String &filePath, const String &textureName, const ETextureType type);

	Handle<Material> create_material(const Material &material, const String& name);
	
	Model    &get_model_by_name(const String &name);
	Model	 &get_model_by_handle(const Handle<Model> handle);
	Mesh     &get_mesh_by_name(const String &name);
	Mesh     &get_mesh_by_handle(const Handle<Mesh> handle);
	Material &get_material_by_name(const String &name);
	Material &get_material_by_handle(const Handle<Material> handle);
	Material &get_default_material();
	Texture  &get_texture_by_name(const String &name);
	Texture  &get_texture_by_handle(const Handle<Texture> handle);

	[[nodiscard]]
	const Handle<Model>		&get_model_handle_by_name(const String &name)	  const;
	[[nodiscard]]
	const Handle<Mesh>		&get_mesh_handle_by_name(const String &name)	  const;
	[[nodiscard]]
	const Handle<Material>	&get_material_handle_by_name(const String &name) const;
	[[nodiscard]]
	const Handle<Texture>	&get_texture_handle_by_name(const String &name)  const;

	[[nodiscard]]
	const DynamicArray<Model>    &get_models()    const;
	[[nodiscard]]
	const DynamicArray<Mesh>     &get_meshes()    const;
	[[nodiscard]]
	const DynamicArray<Material> &get_materials() const;
	[[nodiscard]]
	const DynamicArray<Texture>  &get_textures()  const;

	void shutdown();

protected:
	template<typename DataType, typename ArrayType>
	Void process_accessor(tinygltf::Model &gltfModel, const tinygltf::Accessor &accessor, DynamicArray<ArrayType> &outputData)
	{
		const Int32 bufferViewId = accessor.bufferView;

		const tinygltf::BufferView &bufferView = gltfModel.bufferViews[bufferViewId];
		tinygltf::Buffer &bufferData = gltfModel.buffers[bufferView.buffer];
		UInt8* dataBegin = bufferData.data.data() + accessor.byteOffset + bufferView.byteOffset;

		UInt64 stride = bufferView.byteStride;
		if (stride == 0)
		{
			stride = sizeof(DataType);
		}

		outputData.resize(accessor.count);

		for (UInt64 i = 0; i < accessor.count; i++)
		{
			outputData[i] = static_cast<ArrayType>(*reinterpret_cast<DataType*>(dataBegin + stride * i));
		}
	}

private:
	SResourceManager() = default;
	~SResourceManager() = default;

	std::unordered_map<String, Handle<Model>> nameToIdModels;
	DynamicArray<Model> models;

	std::unordered_map<String, Handle<Mesh>> nameToIdMeshes;
	DynamicArray<Mesh> meshes;

	std::unordered_map<String, Handle<Material>> nameToIdMaterials;
	DynamicArray<Material> materials;

	std::unordered_map<String, Handle<Texture>> nameToIdTextures;
	DynamicArray<Texture> textures;
};

