#include "rendererpch.h"

#include "SceneHierarchy.h"

#include "ResourceUploader.h"
#include "Scene.h"
#include "FrameContext.h"

SceneHierarchyInfo SceneHierarchyInfo::FromAsset(const lux::assetlib::SceneAsset& scene)
{
    SceneHierarchyInfo sceneHierarchy = {};

    const u32 sceneIndex = scene.Header.DefaultSubscene;
    auto& subscene = scene.Header.Subscenes[sceneIndex];
    
    auto& nodes = scene.Header.Nodes;
    sceneHierarchy.Nodes.reserve(nodes.size());

    struct NodeInfo
    {
        u32 ParentIndex{SceneHierarchyHandle::INVALID};
        u32 NodeIndex{SceneHierarchyHandle::INVALID};
        u16 Depth{0};
    };
    std::queue<NodeInfo> nodesToProcess;
    for (auto& node : subscene.Nodes)
        nodesToProcess.push({.NodeIndex = (u32)node});

    while (!nodesToProcess.empty())
    {
        auto [parent, nodeIndex, depth] = nodesToProcess.front();
        nodesToProcess.pop();

        sceneHierarchy.MaxDepth = std::max(sceneHierarchy.MaxDepth, depth);

        auto& node = nodes[nodeIndex];
        SceneHierarchyNodeType type = SceneHierarchyNodeType::Dummy;
        if (node.Mesh != lux::assetlib::SCENE_UNSET_INDEX)
            type = SceneHierarchyNodeType::Mesh;
        else if (node.Light != lux::assetlib::SCENE_UNSET_INDEX)
            type = SceneHierarchyNodeType::Light;

        const u32 thisNodeNewIndex = (u32)sceneHierarchy.Nodes.size();
        u32 payloadIndex = ~0lu;
        switch (type)
        {
        case SceneHierarchyNodeType::Mesh:
            payloadIndex = node.Mesh;
            break;
        case SceneHierarchyNodeType::Light:
            payloadIndex = node.Light;
            break;
        case SceneHierarchyNodeType::Dummy:
        default:
            break;
        }
        
        sceneHierarchy.Nodes.push_back({
            .Type = type,
            .Depth = depth,
            .Parent = parent,
            .LocalTransform = node.Transform,
            .PayloadIndex = payloadIndex
        });

        for (u32 childNode : nodes[nodeIndex].Children)
            nodesToProcess.push({
                .ParentIndex = thisNodeNewIndex,
                .NodeIndex = childNode,
                .Depth = (u16)(depth + 1)
            });
    }
    
    return sceneHierarchy;
}

void SceneHierarchy::Add(SceneInstance instance, const Transform3d& baseTransform)
{
    const SceneHierarchyInfo& instanceHierarchy = instance.m_SceneInfo->m_Hierarchy; 

    const u32 firstNode = (u32)m_Info.Nodes.size();
    const u32 firstRenderObject = m_LastRenderObject;
    const u32 firstLight = m_LastLight;

    m_LastLight += (u32)instance.m_SceneInfo->m_Lights.Lights.size();
    m_LastRenderObject += (u32)instance.m_SceneInfo->m_Geometry.RenderObjects.size();

    for (auto& node : instanceHierarchy.Nodes)
    {
        const bool isTopLevel = node.Parent == SceneHierarchyHandle::INVALID;
        u32 payloadIndex = node.PayloadIndex;
        switch (node.Type)
        {
        case SceneHierarchyNodeType::Mesh:
            payloadIndex += firstRenderObject;
            break;
        case SceneHierarchyNodeType::Light:
            payloadIndex += firstLight;
            break;
        case SceneHierarchyNodeType::Dummy:
        default:
            break;
        }
        m_Info.Nodes.push_back({
            .Type = node.Type,
            .Depth = node.Depth,
            .Parent = isTopLevel ? SceneHierarchyHandle::INVALID : node.Parent + firstNode,
            .LocalTransform = isTopLevel ?
                baseTransform.Combine(node.LocalTransform) :
                node.LocalTransform,
            .PayloadIndex = payloadIndex
        });
    }
    
    m_Info.MaxDepth = std::max(m_Info.MaxDepth, instanceHierarchy.MaxDepth);
}

namespace
{
    void updateRenderObject(Buffer renderObjects, u32 renderObjectIndex, const glm::mat4& transform,
        ResourceUploader& uploader)
    {
        uploader.UpdateBuffer(
            renderObjects,
            transform,
            renderObjectIndex * sizeof(RenderObjectGPU) + offsetof(RenderObjectGPU, Transform));
    }

    void updateLight(CommonLight& light, const glm::mat4& transform)
    {
        switch (light.Type)
        {
        case LightType::Directional:
            light.PositionDirection = glm::normalize(transform * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f));
            break;
        case LightType::Point:
            light.PositionDirection = transform * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            break;
        case LightType::Spot:
            ASSERT(false, "Spot light is not implemented")
            break;
        }
    }
}

void SceneHierarchy::OnUpdate(Scene& scene, FrameContext& ctx)
{
    auto& nodes = m_Info.Nodes;
    std::vector<glm::mat4> transforms(nodes.size());

    for (u32 i = 0; i < nodes.size(); i++)
        transforms[i] = nodes[i].Parent == SceneHierarchyHandle::INVALID ?
            nodes[i].LocalTransform.ToMatrix() :
            transforms[nodes[i].Parent.Handle] * nodes[i].LocalTransform.ToMatrix();

    for (auto&& [i, node] : std::views::enumerate(nodes))
    {
        switch (node.Type)
        {
        case SceneHierarchyNodeType::Mesh:
            updateRenderObject(scene.Geometry().RenderObjects.Buffer, node.PayloadIndex, transforms[i],
                *ctx.ResourceUploader);
            break;
        case SceneHierarchyNodeType::Light:
            updateLight(scene.Lights().Get(node.PayloadIndex), transforms[i]);
            break;
        case SceneHierarchyNodeType::Dummy:
        default:
            break;
        }
    }
}
