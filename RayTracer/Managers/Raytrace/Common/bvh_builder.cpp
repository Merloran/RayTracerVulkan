#include "bvh_builder.hpp"

#include "vertex.hpp"
#include <cstdlib>
#include <fstream>

Void BVHBuilder::create_tree(const DynamicArray<Vertex>& vertexes, const DynamicArray<UInt32>& indexes)
{
    if (indexes.size() % 3 != 0)
    {
        SPDLOG_ERROR("Invalid count of indexes: {}", indexes.size());
        return;
    }
    const UInt64 triangleCount = indexes.size() / 3;

    SPDLOG_INFO("Build tree of {} triangles", triangleCount);
    if (load_tree("scene.bvh"))
    {
        SPDLOG_INFO("BVH loaded from file");
        rootId = Int32(triangleCount);
        return;
    }

    const UInt64 hierarchySize = triangleCount * 2 - 1;
    DynamicArray<Int32> objects;
    objects.reserve(triangleCount);
    hierarchy.reserve(hierarchySize);
    for (UInt64 triangleId = 0; triangleId < indexes.size(); triangleId += 3)
    {
        BVHNode node;
        const FVector3& a = vertexes[indexes[triangleId]].position;
        const FVector3& b = vertexes[indexes[triangleId + 1]].position;
        const FVector3& c = vertexes[indexes[triangleId + 2]].position;

        min(a, b, c, node.min);
        max(a, b, c, node.max);

        node.rightId = UInt32(triangleId);
        node.leftId = UInt32(triangleId);
        pad(node);

        hierarchy.emplace_back(node);
        Int32 nodeId = Int32(triangleId) / 3;
        objects.emplace_back(nodeId);
    }
    SPDLOG_INFO("Leaf nodes created...");
    spdlog::set_pattern("%v");
    rootId = create_hierarchy(objects, 0, Int32(objects.size()));
    fill_stackless_data(rootId, -1);
    save_tree("scene.bvh");
    spdlog::set_pattern("%+");
    SPDLOG_INFO("Build tree complete.");
}

Void BVHBuilder::fill_stackless_data(Int32 nodeId, Int32 parentId)
{
    BVHNode& node = hierarchy[nodeId];
    node.parentId = parentId;

    if (node.leftId != node.rightId)
    {
        node.nextId = node.leftId;
        node.primitiveId = -1;
        if (parentId != -1)
        {
            const BVHNode& parentNode = hierarchy[parentId];
            if (nodeId == parentNode.leftId)
            {
                node.skipId = parentNode.rightId;
            }
            else {
                node.skipId = parentNode.skipId;
            }
        }
        else {
            node.skipId = -1;
        }
        fill_stackless_data(node.leftId, nodeId);
        fill_stackless_data(node.rightId, nodeId);
        return;
    }

    // Node is leaf
    node.primitiveId = node.leftId;
    if (parentId == -1) // Tree is built from one node
    {
        node.nextId = -1;
        node.skipId = -1;
        return;
    }

    const BVHNode& parentNode = hierarchy[parentId];
    if (nodeId == parentNode.leftId)
    {
        node.nextId = parentNode.rightId;
        node.skipId = parentNode.rightId;
    }
    else {
        node.nextId = parentNode.skipId;
        node.skipId = parentNode.skipId;
    }
}

Int32 BVHBuilder::create_hierarchy(const DynamicArray<Int32>& srcObjects, Int32 begin, Int32 end)
{
    const Int32 objectSpan = end - begin;
    DynamicArray<Int32> objects = srcObjects;
    const UInt64 nodeId = hierarchy.size();
    BVHNode& node = hierarchy.emplace_back();
    const Int32 currentAxis = rand_int(0, 2);
    auto comparator = [&](const Int32 first, const Int32 second)
        {
            return hierarchy[first].min[currentAxis] < hierarchy[second].min[currentAxis];
        };

    if (objectSpan == 1)
    {
        hierarchy.pop_back();
        return begin;
    }
    else if (objectSpan == 2)
    {
        if (comparator(begin, begin + 1))
        {
            node.leftId = begin;
            node.rightId = begin + 1;
        }
        else {
            node.leftId = begin + 1;
            node.rightId = begin;
        }
    }
    else {
        std::sort(objects.begin() + begin, objects.begin() + end, comparator);

        const Int32 mid = begin + objectSpan / 2;
        node.leftId = create_hierarchy(objects, begin, mid);
        node.rightId = create_hierarchy(objects, mid, end);
    }

    const BVHNode& left  = hierarchy[node.leftId];
    const BVHNode& right = hierarchy[node.rightId];
    node.min = glm::min(left.min, right.min);
    node.max = glm::max(left.max, right.max);

    SPDLOG_INFO("Progress: {0:.2f}%", Float32(hierarchy.size()) / Float32(hierarchy.capacity()) * 100.0f);
    return nodeId;
}

Void BVHBuilder::save_tree(const String& path)
{
    std::ofstream file;
    file.open(path, std::ios::binary);
    file.write(reinterpret_cast<const Char*>(hierarchy.data()), hierarchy.size() * sizeof(BVHNode));
    file.close();
}

Bool BVHBuilder::load_tree(const String& path)
{
    std::ifstream file;
    file.open(path, std::ios::binary | std::ios::ate);
    if (!file)
    {
        SPDLOG_WARN("Failed to open {}", path);
        return false;
    }

    const UInt64 size = file.tellg();

    file.seekg(0, std::ios::beg);
    hierarchy.resize(size / sizeof(BVHNode));

    file.read(reinterpret_cast<Char*>(hierarchy.data()), size);
    file.close();
    return true;
}

Void BVHBuilder::min(const FVector3& a, const FVector3& b, const FVector3& c, FVector3& result)
{
    result = glm::min(a, b);
    result = glm::min(result, c);
}

Void BVHBuilder::max(const FVector3& a, const FVector3& b, const FVector3& c, FVector3& result)
{
    result = glm::max(a, b);
    result = glm::max(result, c);
}

Void BVHBuilder::pad(BVHNode& node)
{
    const Float32 delta = 0.001953125f; // DO NOT ASK XD
    FVector3 size = node.max - node.min;
    for (Int32 i = 0; i < 3; ++i)
    {
        if (size[i] < delta)
        {
            node.min[i] -= delta * 0.5f;
            node.max[i] += delta * 0.5f;
        }
    }

}

Int32 BVHBuilder::rand_int(Int32 min, Int32 max)
{
    return rand() % (max - min) + min;
}
