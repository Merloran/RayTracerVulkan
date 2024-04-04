#pragma once

struct BVHNode
{
	FVector3 min;
	Int32 leftId;
	FVector3 max;
	Int32 rightId;
	Int32 parentId;
	Int32 nextId;
	Int32 skipId;
	Int32 primitiveId;
};