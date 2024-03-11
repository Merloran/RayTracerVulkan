#pragma once

template<typename Type>
struct Handle;
struct Mesh;
struct Material;

/** It's just set of meshes and materials */
struct Model
{
	DynamicArray<Handle<Mesh>> meshes;
	DynamicArray<Handle<Material>> materials;
	String directory;
	String name;
};