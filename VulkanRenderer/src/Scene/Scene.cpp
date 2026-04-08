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

SceneInstanceHandle Scene::Instantiate(const SceneInfo& sceneInfo, const SceneInstantiationData& instantiationData)
{
    const SceneInstanceHandle instance = RegisterSceneInstance(sceneInfo);
    InstantiateHandle(instance, instantiationData);
    
    return instance;
}

void Scene::InstantiateHandle(SceneInstanceHandle handle, const SceneInstantiationData& instantiationData)
{
    m_NewInstances.push_back({
        .Instance = handle,
        .InstantiationData = instantiationData
    });
}

void Scene::Delete(SceneInstanceHandle instance)
{
    auto it = std::ranges::find_if(m_NewInstances, [&instance](auto& newInstance) {
        return instance == newInstance.Instance;
    });
    const bool isJustAdded = it != m_NewInstances.end();
    if (isJustAdded)
    {
        m_NewInstances.erase(it);
        return;
    }

    m_InstanceIsAlive[instance] = false;
    m_DeletedInstances.push_back(instance);
}

void Scene::ReplaceScene(const SceneInfo& original, const SceneInfo& replacement)
{
    m_ReplacedScenes.push_back({.Original = &original, .Replacement = &replacement});
}

void Scene::OnUpdate(FrameContext& ctx)
{
    HandleSpawnAndSweep(ctx, /*reclaimHandles*/true);
    HandleReplacements(ctx);
    
    UpdateHierarchy(ctx);
    m_Lights.OnUpdate(ctx);
}

SceneInstanceHandle Scene::RegisterSceneInstance(const SceneInfo& sceneInfo)
{
    SceneInstanceHandle handle = m_ActiveInstances.insert(SceneInstance{});
    MapSceneInstanceToSceneInfo(sceneInfo, handle);
    
    return handle;
}

void Scene::MapSceneInstanceToSceneInfo(const SceneInfo& sceneInfo, SceneInstanceHandle handle)
{
    m_SceneInstancesMap[&sceneInfo].Instances.insert(handle);
    m_ActiveInstances[handle].m_SceneInfo = &sceneInfo;
}

Scene::NewInstanceData Scene::AddToHierarchy(SceneInstanceHandle instance, const Transform3d& baseTransform,
    FrameContext& ctx)
{
    const SceneInfo& sceneInfo = *m_ActiveInstances[instance].m_SceneInfo;
    const SceneHierarchyInfo& instanceHierarchy = sceneInfo.m_Hierarchy; 
    const u32 firstNode = (u32)m_HierarchyInfo.Nodes.size();
    
    const SceneGeometry::AddRenderObjectsResult addResult = m_Geometry.AddRenderObjects(sceneInfo, instance, ctx);
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
        .SceneInfo = &sceneInfo,
        .Instance = instance,
        .RenderObjectsOffset = addResult.FirstRenderObject,
    };
}

void Scene::HandleReplacements(FrameContext& ctx)
{
    auto getTopNodeLevelTransform = [&](const SceneHierarchyNode& node) -> Transform3d {
        SceneHierarchyHandle parent = node.Parent;
        if (parent == SceneHierarchyHandle::INVALID)
            return node.LocalTransform;

        for (;;)
        {
            SceneHierarchyHandle current = parent;
            parent = m_HierarchyInfo.Nodes[parent.Handle].Parent;
            if (parent == SceneHierarchyHandle::INVALID)
                return m_HierarchyInfo.Nodes[current.Handle].LocalTransform;
        }
    };
    
    if (m_ReplacedScenes.empty())
        return;

    struct ReinstantiationData
    {
        const SceneInfo* Replacement{nullptr};
        SceneInstantiationData InstantiationData{};
    };
    std::unordered_map<SceneInstanceHandle, ReinstantiationData> reinstantiationData;
    for (auto&& [original, replacement] : m_ReplacedScenes)
    {
        auto& originalInstances = m_SceneInstancesMap[original].Instances;
        for (auto& node : m_HierarchyInfo.Nodes)
        {
            if (originalInstances.contains(node.Instance) && !reinstantiationData.contains(node.Instance))
            {
                const Transform3d topNodeOriginalTransform = original->m_Hierarchy.Nodes.front().LocalTransform;
                
                reinstantiationData.emplace(
                    node.Instance, ReinstantiationData{
                        .Replacement = replacement,
                        .InstantiationData = {
                            .Transform = getTopNodeLevelTransform(node).Combine(topNodeOriginalTransform.Inverse())
                        }
                    });
            }
        }
    }
    auto nodesBackup = m_HierarchyInfo.Nodes;
    for (const auto& instance : reinstantiationData | std::views::keys)
        Delete(instance);
    Sweep(/*reclaimHandles*/false);
    
    for (auto&& [instance, reinstantiationInfo] : reinstantiationData) {
        MapSceneInstanceToSceneInfo(*reinstantiationInfo.Replacement, instance);
        InstantiateHandle(instance, reinstantiationInfo.InstantiationData);
    }
    Spawn(ctx);
    
    m_ReplacedScenes.clear();
}

void Scene::HandleSpawnAndSweep(FrameContext& ctx, bool reclaimHandles)
{
    Spawn(ctx);
    Sweep(reclaimHandles);
}

void Scene::Spawn(FrameContext& ctx)
{
    for (auto&& [instanceHandle, instantiationData] : m_NewInstances)
    {
        auto& instance = m_ActiveInstances[instanceHandle];
        if (!m_SceneInstancesMap[instance.m_SceneInfo].HasGeometry)
        {
            m_Geometry.Add(*instance.m_SceneInfo, ctx);
            m_SceneInstancesMap[instance.m_SceneInfo].HasGeometry = true;
        }

        if (instanceHandle >= m_InstanceIsAlive.size())
            m_InstanceIsAlive.resize(instanceHandle + 1);
        m_InstanceIsAlive[instanceHandle] = true;
        m_InstanceAddedSignal.Emit(AddToHierarchy(instanceHandle, instantiationData.Transform, ctx));
    }
    m_NewInstances.clear();
}

void Scene::Sweep(bool reclaimHandles)
{
    if (m_DeletedInstances.empty())
        return;
    
    auto reparentToValid = [&](SceneHierarchyNode& node) {
        SceneHierarchyHandle parent = node.Parent;
        while (
            parent != SceneHierarchyHandle::INVALID &&
            !m_InstanceIsAlive[m_HierarchyInfo.Nodes[parent].Instance])
        {
            parent = m_HierarchyInfo.Nodes[parent].Parent;
        }
        node.Parent = parent;
    };

    std::vector<u32> reorder(m_HierarchyInfo.Nodes.size());
    std::ranges::iota(reorder, 0u);

    for (auto& node : m_HierarchyInfo.Nodes)
    {
        if (m_InstanceIsAlive[node.Instance])
            reparentToValid(node);
    }
    
    u32 currentLast = (u32)m_HierarchyInfo.Nodes.size() - 1;
    for (i32 i = (i32)currentLast; i >= 0; i--)
    {
        SceneHierarchyNode& node = m_HierarchyInfo.Nodes[i];
        
        if (m_InstanceIsAlive[node.Instance])
            continue;
        
        if (node.Type == SceneHierarchyNodeType::Light)
            m_Lights.Delete(node.PayloadIndex);

        reorder[currentLast] = i;
        std::swap(m_HierarchyInfo.Nodes[i], m_HierarchyInfo.Nodes[currentLast]);
        currentLast--;
    }

    for (auto& node : m_HierarchyInfo.Nodes)
    {
        if (node.Parent != SceneHierarchyHandle::INVALID)
        {
            u32 order = node.Parent.Handle;
            u32 reordered = reorder[order];
            while (reordered != order)
            {
                order = reordered;
                reordered = reorder[order];
            }
            node.Parent.Handle = reordered;
        }
    }

    m_HierarchyInfo.Nodes.resize(currentLast + 1);

    for (auto& instanceHandle : m_DeletedInstances)
    {
        auto& instance = m_ActiveInstances[instanceHandle];
        m_Geometry.DeleteRenderObjects(instanceHandle);
        m_SceneInstancesMap[instance.m_SceneInfo].Instances.erase(instanceHandle);
        
        m_InstanceDeletedSignal.Emit({
            .SceneInfo = instance.m_SceneInfo,
            .Instance = instanceHandle,
        });
    }

    for (auto&& [i, isAlive] : std::views::enumerate(m_InstanceIsAlive))
    {
        if (!isAlive && reclaimHandles)
            m_ActiveInstances.erase((u32)i);
        m_InstanceIsAlive[i] = true;
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
