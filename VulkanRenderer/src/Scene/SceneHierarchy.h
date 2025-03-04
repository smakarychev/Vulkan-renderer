#pragma once

#include "types.h"

#include <vector>
#include <glm/glm.hpp>

#include "SceneInstance.h"

struct Transform3d;
class ResourceUploader;

namespace assetLib
{
    struct SceneInfo;
}

struct SceneHierarchyHandle
{
    static constexpr u32 INVALID = ~0lu;
    u32 Handle{INVALID};

    auto operator<=>(const SceneHierarchyHandle&) const = default;
    operator u32() const { return Handle; }
};

enum class SceneHierarchyNodeType : u16
{
    Dummy, Mesh, Light
};

struct SceneHierarchyNode
{
    SceneHierarchyNodeType Type{SceneHierarchyNodeType::Dummy};
    u16 Depth{0};
    SceneHierarchyHandle Parent{};
    glm::mat4 LocalTransform{1.0f};
    u32 PayloadIndex{0};
};

struct SceneHierarchyInfo
{
    static SceneHierarchyInfo FromAsset(assetLib::SceneInfo& sceneInfo);
    
    std::vector<SceneHierarchyNode> Nodes;
    u16 MaxDepth{0};
};

class SceneHierarchy
{
    friend class Scene;
public:
    void Add(SceneInstance instance, const Transform3d& baseTransform);
    
    void OnUpdate(SceneGeometry2& geometry, ResourceUploader& uploader);
private:
    struct InstanceData
    {
        u32 FirstNode{0};
        u32 NodeCount{0};
        u32 FirstRenderObject{0};
        u32 RenderObjectCount{0};
    };
private:
    SceneHierarchyInfo m_Info{};
    std::vector<InstanceData> m_InstancesData;
};
