#pragma once

struct Vertex
{
	FVector3 position; alignas(16)
	FVector3 normal; alignas(16)
	FVector2 uv;
	Int32 materialId;
};