#pragma once
#include "texture.hpp"
struct Texture;

/** It's just set of textures */
struct Material
{
	Array<Handle<Texture>, UInt64(ETextureType::Count)> textures;
	Float32	indexOfRefraction = 0.0f;
	String name;
};
