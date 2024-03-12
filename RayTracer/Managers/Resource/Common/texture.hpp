#pragma once

enum class ETextureType : Int16
{
	None			 = 0,

	Albedo			 = 0b0000000000000001,
	Normal			 = 0b0000000000000010,
	Roughness		 = 0b0000000000000100,
	Metalness		 = 0b0000000000001000,
	AmbientOcclusion = 0b0000000000010000,
	Emission		 = 0b0000000000100000,
	Height			 = 0b0000000001000000,
	Opacity			 = 0b0000000010000000,
	HDR				 = 0b1000000000000000,

	RM				 = Roughness | Metalness,
	RMAO			 = Roughness | Metalness | AmbientOcclusion,

	TypesCount		 = 12,
};

struct Texture 
{
	IVector2 size;
	Int32 channels;
	ETextureType type = ETextureType::None;
	String name;
	UInt8* data;
};