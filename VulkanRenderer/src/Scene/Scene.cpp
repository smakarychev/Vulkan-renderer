#include "rendererpch.h"

#include "Scene.h"

#include "FrameContext.h"
#include "ResourceUploader.h"

Scene Scene::CreateEmpty(DeletionQueue& deletionQueue)
{
    Scene scene = {};

    scene.m_Geometry = SceneGeometry::CreateEmpty(deletionQueue);
    scene.m_Lights = SceneLight::CreateEmpty(deletionQueue);

    return scene;
}

SceneInstance Scene::Instantiate(const SceneInfo& sceneInfo, const SceneInstantiationData& instantiationData,
    FrameContext& ctx)
{
    const SceneInstance instance = RegisterSceneInstance(sceneInfo);
    if (m_SceneInstancesMap[&sceneInfo] == 1)
        m_Geometry.Add(instance, ctx);
    SceneGeometry::AddRenderObjectsResult addCommandsResult = m_Geometry.AddRenderObjects(instance, ctx);
    m_Lights.Add(instance);
    AddToHierarchy(instance, instantiationData.Transform);

    m_InstanceAddedSignal.Emit({
        .SceneInfo = &sceneInfo,
        .RenderObjectsOffset = addCommandsResult.FirstRenderObject,
    });
    
    return instance;
}

void Scene::Delete(SceneInstance instance)
{
    /*
     * - Find commands and meshlets referenced by the instance
     * - Delete these commands, shift alive commands and meshlets to the freed space
     * - If this was the last instance of `SceneInfo`:
     *  - Free geometry suballocations of this scene info 
     */
}

void Scene::OnUpdate(FrameContext& ctx)
{
    UpdateHierarchy(ctx);
    m_Lights.OnUpdate(ctx);
}

SceneInstance Scene::RegisterSceneInstance(const SceneInfo& sceneInfo)
{
    m_SceneInstancesMap[&sceneInfo] += 1;
    SceneInstance instance = {};
    instance.m_InstanceId = m_ActiveInstances;
    instance.m_SceneInfo = &sceneInfo;
    
    m_ActiveInstances++;
    
    return instance;
}

void Scene::AddToHierarchy(SceneInstance instance, const Transform3d& baseTransform)
{
    const SceneHierarchyInfo& instanceHierarchy = instance.m_SceneInfo->m_Hierarchy; 

    const u32 firstNode = (u32)m_HierarchyInfo.Nodes.size();
    const u32 firstRenderObject = m_LastRenderObject;
    const u32 firstLight = m_LastLight;

    m_LastLight += (u32)instance.m_SceneInfo->m_Lights.Lights.size();
    m_LastRenderObject += (u32)instance.m_SceneInfo->m_Geometry.RenderObjects.size();
    m_RenderObjectPreviousTransforms.resize(m_LastRenderObject);

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
        m_HierarchyInfo.Nodes.push_back({
            .Type = node.Type,
            .Depth = node.Depth,
            .Parent = isTopLevel ? SceneHierarchyHandle::INVALID : node.Parent + firstNode,
            .LocalTransform = isTopLevel ?
                baseTransform.Combine(node.LocalTransform) :
                node.LocalTransform,
            .PayloadIndex = payloadIndex
        });
    }
    
    m_HierarchyInfo.MaxDepth = std::max(m_HierarchyInfo.MaxDepth, instanceHierarchy.MaxDepth);
}

namespace
{
void updateRenderObject(Buffer renderObjects, u32 renderObjectIndex,
    const glm::mat4& previousTransform, const glm::mat4& transform, ResourceUploader& uploader)
{
    uploader.UpdateBuffer(
        renderObjects,
        Span<const glm::mat4>{transform, previousTransform},
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

void Scene::UpdateHierarchy(FrameContext& ctx)
{
    auto& nodes = m_HierarchyInfo.Nodes;
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
            {
                auto& previousTransform = m_RenderObjectPreviousTransforms[node.PayloadIndex];
                updateRenderObject(Geometry().RenderObjects.Buffer, node.PayloadIndex,
                    previousTransform, transforms[i], *ctx.ResourceUploader);
                m_RenderObjectPreviousTransforms[node.PayloadIndex] = transforms[i];
            }
            break;
        case SceneHierarchyNodeType::Light:
            updateLight(Lights().Get(node.PayloadIndex), transforms[i]);
            break;
        case SceneHierarchyNodeType::Dummy:
        default:
            break;
        }
    }
}
