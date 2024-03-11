#pragma once
struct Texture;

/** It's just set of textures */
struct Material
{
	DynamicArray<Handle<Texture>> textures;
	Float32	indexOfRefraction = 0.0f;
	String name;
};
