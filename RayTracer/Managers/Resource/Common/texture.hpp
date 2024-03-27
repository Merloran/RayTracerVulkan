#pragma once

class Image;

enum class ETextureType : Int16
{
	None = 0,

	Albedo,
	Normal,
	Roughness,
	Metalness,
	AmbientOcclusion,
	Emission,
	Height,
	Opacity,
	HDR,

	RM,	
	RMAO,

	Count,
};

struct Texture 
{
	IVector2 size;
	Int32 channels;
	ETextureType type = ETextureType::None;
	Handle<Image> image;
	String name;
	UInt8* data;
};