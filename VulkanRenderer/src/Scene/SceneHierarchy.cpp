#include "SceneHierarchy.h"

#include "ResourceUploader.h"
#include "Scene.h"
#include "SceneAsset.h"
#include "FrameContext.h"

#include <queue>
#include <ranges>

SceneHierarchyInfo SceneHierarchyInfo::FromAsset(assetLib::SceneInfo& sceneInfo)
{
    SceneHierarchyInfo sceneHierarchy = {};

    const u32 sceneIndex = sceneInfo.Scene.defaultScene > 0 ? (u32)sceneInfo.Scene.defaultScene : 0;
    auto& scene = sceneInfo.Scene.scenes[sceneIndex];
    
    auto& nodes = sceneInfo.Scene.nodes;
    sceneHierarchy.Nodes.reserve(nodes.size());

    struct NodeInfo
    {
        u32 ParentIndex{SceneHierarchyHandle::INVALID};
        u32 NodeIndex{SceneHierarchyHandle::INVALID};
        u16 Depth{0};
    };
    std::queue<NodeInfo> nodesToProcess;
    for (auto& node : scene.nodes)
        nodesToProcess.push({.NodeIndex = (u32)node});

    while (!nodesToProcess.empty())
    {
        auto [parent, nodeIndex, depth] = nodesToProcess.front();
        nodesToProcess.pop();

        sceneHierarchy.MaxDepth = std::max(sceneHierarchy.MaxDepth, depth);

        auto& node = nodes[nodeIndex];
        SceneHierarchyNodeType type = SceneHierarchyNodeType::Dummy;
        if (node.mesh >= 0)
            type = SceneHierarchyNodeType::Mesh;
        else if (node.light >= 0)
            type = SceneHierarchyNodeType::Light;

        const u32 thisNodeNewIndex = (u32)sceneHierarchy.Nodes.size();
        u32 payloadIndex = ~0lu;
        switch (type)
        {
        case SceneHierarchyNodeType::Mesh:
            payloadIndex = node.mesh;
            break;
        case SceneHierarchyNodeType::Light:
            payloadIndex = node.light;
            break;
        case SceneHierarchyNodeType::Dummy:
        default:
            break;
        }
        
        sceneHierarchy.Nodes.push_back({
            .Type = type,
            .Depth = depth,
            .Parent = parent,
            .LocalTransform = assetLib::getTransform(node),
            .PayloadIndex = payloadIndex});

        for (u32 childNode : nodes[nodeIndex].children)
            nodesToProcess.push({
                .ParentIndex = thisNodeNewIndex,
                .NodeIndex = childNode,
                .Depth = (u16)(depth + 1)});
    }
    
    return sceneHierarchy;
}

void SceneHierarchy::Add(SceneInstance instance, const Transform3d& baseTransform)
{
    ASSERT(instance.m_InstanceId == (u32)m_InstancesData.size(), "Every instance must have its hierarchy added")
    
    const SceneHierarchyInfo& instanceHierarchy = instance.m_SceneInfo->m_Hierarchy; 

    const InstanceData instanceData = {
        .FirstNode = (u32)m_Info.Nodes.size(),
        .NodeCount = (u32)instanceHierarchy.Nodes.size(),
        .FirstRenderObject = m_InstancesData.empty() ?
            0 : m_InstancesData.back().FirstRenderObject + m_InstancesData.back().RenderObjectCount,
        .RenderObjectCount = (u32)instance.m_SceneInfo->m_Geometry.RenderObjects.size(),
        .FirstLight = m_InstancesData.empty() ?
            0 : m_InstancesData.back().FirstLight + m_InstancesData.back().LightCount,
        .LightCount = (u32)instance.m_SceneInfo->m_Lights.Lights.size()};
    m_InstancesData.push_back(instanceData);

    for (auto& node : instanceHierarchy.Nodes)
    {
        const bool isTopLevel = node.Parent == SceneHierarchyHandle::INVALID;
        u32 payloadIndex = node.PayloadIndex;
        switch (node.Type)
        {
        case SceneHierarchyNodeType::Mesh:
            payloadIndex += instanceData.FirstRenderObject;
            break;
        case SceneHierarchyNodeType::Light:
            payloadIndex += instanceData.FirstLight;
            break;
        case SceneHierarchyNodeType::Dummy:
        default:
            break;
        }
        m_Info.Nodes.push_back({
            .Type = node.Type,
            .Depth = node.Depth,
            .Parent = isTopLevel ? SceneHierarchyHandle::INVALID : node.Parent + instanceData.FirstNode,
            .LocalTransform = isTopLevel ?
                baseTransform.ToMatrix() * node.LocalTransform :
                node.LocalTransform,
            .PayloadIndex = payloadIndex});
    }
    
    m_Info.MaxDepth = std::max(m_Info.MaxDepth, instanceHierarchy.MaxDepth);
}

namespace
{
    void updateMesh(Buffer renderObjects, u32 meshIndex, const glm::mat4& transform,
        ResourceUploader& uploader)
    {
        uploader.UpdateBuffer(
            renderObjects,
            transform,
            meshIndex * sizeof(RenderObjectGPU2) + offsetof(RenderObjectGPU2, Transform));
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
            nodes[i].LocalTransform :
            transforms[nodes[i].Parent.Handle] * nodes[i].LocalTransform;

    for (auto&& [i, node] : std::ranges::views::enumerate(nodes))
    {
        switch (node.Type)
        {
        case SceneHierarchyNodeType::Mesh:
            updateMesh(scene.Geometry().RenderObjects.Buffer, node.PayloadIndex, transforms[i], *ctx.ResourceUploader);
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
