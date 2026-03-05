#pragma once

#include "SceneInstance.h"

#include <CoreLib/types.h>
#include <CoreLib/Math/Transform.h>

#include <vector>

namespace lux::assetlib
{
struct SceneAsset;
}

struct FrameContext;
struct Transform3d;

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
    Transform3d LocalTransform{};
    u32 PayloadIndex{0};
};

struct SceneHierarchyInfo
{
    static SceneHierarchyInfo FromAsset(const lux::assetlib::SceneAsset& scene);
    
    std::vector<SceneHierarchyNode> Nodes;
    u16 MaxDepth{0};
};

class SceneHierarchy
{
    friend class Scene;
public:
    void Add(SceneInstance instance, const Transform3d& baseTransform);
    void OnUpdate(Scene& scene, FrameContext& ctx);
private:
    u32 m_LastRenderObject{0};
    u32 m_LastLight{0};
    SceneHierarchyInfo m_Info{};
};
