#pragma once

struct BVHNode
{
	alignas(16) FVector3 min; 
	alignas(16) FVector3 max; 
	Int32 leftId;
	Int32 rightId;
	Int32 parentId;
	Int32 nextId;
	Int32 skipId;
	Int32 primitiveId;
};