#pragma once

struct Mesh 
{
	DynamicArray<FVector3> positions;
	DynamicArray<FVector3> normals;
	DynamicArray<FVector2> uvs;
	DynamicArray<UInt32> indexes;
	String name;
};