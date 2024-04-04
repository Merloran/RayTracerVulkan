#pragma once
#include "bvh_node.hpp"

struct Vertex;

class BVHBuilder
{
public:
	Void create_tree(const DynamicArray<Vertex>& vertexes, const DynamicArray<UInt32>& indexes);

	DynamicArray<BVHNode> hierarchy;
	Int32 rootId;
private:
	Int32 create_hierarchy(const DynamicArray<Int32>& srcObjects, Int32 begin, Int32 end);
	Void fill_stackless_data(Int32 nodeId, Int32 parentId);
	Void save_tree(const String& path);
	Bool load_tree(const String& path);
	Void min(const FVector3& a, const FVector3& b, const FVector3& c, FVector3& result);
	Void max(const FVector3& a, const FVector3& b, const FVector3& c, FVector3& result);
	Void pad(BVHNode& node);
	Int32 rand_int(Int32 min, Int32 max);
};

