#pragma once

class Buffer;

struct Mesh 
{
	DynamicArray<FVector3> positions;
	DynamicArray<FVector3> normals;
	DynamicArray<FVector2> uvs;
	DynamicArray<UInt32> indexes;
	Handle<Buffer> positionsHandle;
	Handle<Buffer> normalsHandle;
	Handle<Buffer> uvsHandle;
	Handle<Buffer> indexesHandle;
	String name;
};
