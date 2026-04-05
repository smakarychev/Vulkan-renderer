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

SceneInstance Scene::Instantiate(const SceneInfo& sceneInfo, const SceneInstantiationData& instantiationData)
{
    const SceneInstance instance = RegisterSceneInstance(sceneInfo);
    m_NewInstances.push_back({
        .Instance = instance,
        .InstantiationData = instantiationData
    });
    
    return instance;
}

void Scene::Delete(SceneInstance instance)
{
    auto it = std::ranges::find_if(m_NewInstances, [&instance](auto& newInstance) {
        return newInstance.Instance.m_InstanceId == instance.m_InstanceId;
    });
    const bool isJustAdded = it != m_NewInstances.end();
    if (isJustAdded)
    {
        m_NewInstances.erase(it);
        return;
    }

    m_InstanceIsAlive[instance.m_InstanceId] = false;
    m_DeletedInstances.push_back(instance);
}

void Scene::OnUpdate(FrameContext& ctx)
{
    Spawn(ctx);
    Sweep();
    
    UpdateHierarchy(ctx);
    m_Lights.OnUpdate(ctx);
}

SceneInstance Scene::RegisterSceneInstance(const SceneInfo& sceneInfo)
{
    m_SceneInstancesMap[&sceneInfo] += 1;
    SceneInstance instance = {};
    instance.m_InstanceId = m_ActiveInstancesIndices.insert(0);
    instance.m_SceneInfo = &sceneInfo;
    
    return instance;
}

Scene::NewInstanceData Scene::AddToHierarchy(SceneInstance instance, const Transform3d& baseTransform,
    FrameContext& ctx)
{
    const SceneInfo& sceneInfo = *instance.m_SceneInfo;
    const SceneHierarchyInfo& instanceHierarchy = sceneInfo.m_Hierarchy; 
    const u32 firstNode = (u32)m_HierarchyInfo.Nodes.size();
    
    const SceneGeometry::AddRenderObjectsResult addResult = m_Geometry.AddRenderObjects(instance, ctx);
    m_MaxRenderObjectIndex = std::max(
        m_MaxRenderObjectIndex, addResult.FirstRenderObject + (u32)sceneInfo.m_Geometry.RenderObjects.size());

    for (auto& node : instanceHierarchy.Nodes)
    {
        const bool isTopLevel = node.Parent == SceneHierarchyHandle::INVALID;
        u32 payloadIndex = node.PayloadIndex;
        switch (node.Type)
        {
        case SceneHierarchyNodeType::Mesh:
            payloadIndex += addResult.FirstRenderObject;
            break;
        case SceneHierarchyNodeType::Light:
            payloadIndex = m_Lights.Add(sceneInfo.m_Lights.Lights[payloadIndex]);
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
            .PayloadIndex = payloadIndex,
            .Instance = instance
        });
    }
    
    m_HierarchyInfo.MaxDepth = std::max(m_HierarchyInfo.MaxDepth, instanceHierarchy.MaxDepth);

    return {
        .Instance = instance,
        .RenderObjectsOffset = addResult.FirstRenderObject,
    };
}

void Scene::Spawn(FrameContext& ctx)
{
    for (auto&& [instance, instantiationData] : m_NewInstances)
    {
        if (m_SceneInstancesMap[instance.m_SceneInfo] == 1)
            m_Geometry.Add(instance, ctx);

        if (instance.m_InstanceId >= m_InstanceIsAlive.size())
            m_InstanceIsAlive.resize(instance.m_InstanceId + 1);
        m_InstanceIsAlive[instance.m_InstanceId] = true;
        m_InstanceAddedSignal.Emit(AddToHierarchy(instance, instantiationData.Transform, ctx));
    }
    m_NewInstances.clear();
}

void Scene::Sweep()
{
    if (m_DeletedInstances.empty())
        return;
    
    auto reparentToValid = [&](SceneHierarchyNode& node) {
        SceneHierarchyHandle parent = node.Parent;
        while (
            parent != SceneHierarchyHandle::INVALID &&
            !m_InstanceIsAlive[m_HierarchyInfo.Nodes[parent].Instance.m_InstanceId])
        {
            parent = m_HierarchyInfo.Nodes[parent].Parent;
        }
        node.Parent = parent;
    };

    std::vector<u32> reorder(m_HierarchyInfo.Nodes.size());
    std::ranges::iota(reorder, 0u);

    u32 currentLast = (u32)m_HierarchyInfo.Nodes.size() - 1;
    for (i32 i = (i32)currentLast; i >= 0; i--)
    {
        SceneHierarchyNode& node = m_HierarchyInfo.Nodes[i];
        
        const SceneInstance& instance = node.Instance;
        if (m_InstanceIsAlive[instance.m_InstanceId])
        {
            reparentToValid(node);
            continue;
        }
        
        if (node.Type == SceneHierarchyNodeType::Light)
            m_Lights.Delete(node.PayloadIndex);

        reorder[currentLast] = i;
        std::swap(m_HierarchyInfo.Nodes[i], m_HierarchyInfo.Nodes[currentLast]);
        currentLast--;
    }

    for (auto& node : m_HierarchyInfo.Nodes)
        if (node.Parent != SceneHierarchyHandle::INVALID)
            node.Parent.Handle = reorder[node.Parent.Handle];

    m_HierarchyInfo.Nodes.resize(currentLast + 1);
    for (auto&& [i, isAlive] : std::views::enumerate(m_InstanceIsAlive))
    {
        if (!isAlive)
            m_ActiveInstancesIndices.erase((u32)i);
        m_InstanceIsAlive[i] = true;
    }

    for (auto& instance : m_DeletedInstances)
    {
        m_Geometry.DeleteRenderObjects(instance);

        m_InstanceDeletedSignal.Emit({
            .Instance = instance,
        });
    }
    m_DeletedInstances.clear();
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
    m_RenderObjectPreviousTransforms.resize(m_MaxRenderObjectIndex);

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
                updateRenderObject(Device::GetBufferArenaUnderlyingBuffer(Geometry().RenderObjects), node.PayloadIndex,
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
