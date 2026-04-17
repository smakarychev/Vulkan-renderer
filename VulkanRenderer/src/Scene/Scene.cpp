#include "rendererpch.h"

#include "Scene.h"

#include "FrameContext.h"
#include "ResourceUploader.h"
#include "Assets/Materials/MaterialAssetManager.h"

Scene::Scene(DeletionQueue& deletionQueue, lux::SceneAssetManager& sceneAssetManager)
    : m_SceneAssetManager(&sceneAssetManager)
{
    using SceneDeletedInfo = lux::SceneAssetManager::SceneDeletedInfo;
    using SceneReplacedInfo = lux::SceneAssetManager::SceneReplacedInfo;
    using MaterialUpdatedInfo = lux::SceneAssetManager::MaterialUpdatedInfo;
    m_Geometry = SceneGeometry::CreateEmpty(deletionQueue);
    m_Lights = SceneLight::CreateEmpty(deletionQueue);
    m_SceneDeletedHandler = SignalHandler<SceneDeletedInfo>([this](const SceneDeletedInfo& sceneDeletedInfo) {
        DeleteScene(sceneDeletedInfo.Scene);
    });
    m_SceneReplacedHandler = SignalHandler<SceneReplacedInfo>([this](const SceneReplacedInfo& sceneReplacedInfo) {
        ReplaceScene(sceneReplacedInfo.Scene, sceneReplacedInfo.Scene);
    });
    m_MaterialUpdatedHandler = SignalHandler<MaterialUpdatedInfo>([this](
        const MaterialUpdatedInfo& materialUpdatedInfo) {
        UpdateMaterial(materialUpdatedInfo.Scene);   
    });
    m_SceneDeletedHandler.Connect(sceneAssetManager.GetSceneDeletedSignal());
    m_SceneReplacedHandler.Connect(sceneAssetManager.GetSceneReplacedSignal());
    m_MaterialUpdatedHandler.Connect(sceneAssetManager.GetMaterialUpdatedSignal());
}


lux::SceneInstanceHandle Scene::Instantiate(lux::SceneHandle scene, const SceneInstantiationData& instantiationData)
{
    const lux::SceneInstanceHandle instance = RegisterSceneInstance(scene);
    InstantiateHandle(instance, instantiationData);
    
    return instance;
}

void Scene::InstantiateHandle(lux::SceneInstanceHandle handle, const SceneInstantiationData& instantiationData)
{
    m_NewInstances.push_back({
        .Instance = handle,
        .InstantiationData = instantiationData
    });
}

void Scene::Delete(lux::SceneInstanceHandle instance)
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

void Scene::OnUpdate(FrameContext& ctx)
{
    HandleSpawnAndSweep(ctx, /*reclaimHandles*/true);
    HandleReplacements(ctx);
    HandleMaterialUpdates(ctx);
    
    UpdateHierarchy(ctx);
    m_Lights.OnUpdate(ctx);
}

lux::SceneInstanceHandle Scene::RegisterSceneInstance(lux::SceneHandle scene)
{
    lux::SceneInstanceHandle handle = m_ActiveInstances.insert(SceneInstance{});
    MapSceneInstanceToSceneInfo(scene, handle);
    
    return handle;
}

void Scene::MapSceneInstanceToSceneInfo(lux::SceneHandle scene, lux::SceneInstanceHandle handle)
{
    m_ScenesMap[scene].Instances.insert(handle);
    m_ActiveInstances[handle].Scene = scene;
}

Scene::NewInstanceData Scene::AddToHierarchy(lux::SceneInstanceHandle instance, const Transform3d& baseTransform,
    FrameContext& ctx)
{
    const lux::SceneAsset& sceneAsset = *m_SceneAssetManager->Get(m_ActiveInstances[instance].Scene);
    const lux::SceneHierarchyInfo& instanceHierarchy = sceneAsset.Hierarchy; 
    const u32 firstNode = (u32)m_HierarchyInfo.Nodes.size();
    
    const SceneGeometry::AddRenderObjectsResult addResult = m_Geometry.AddRenderObjects(sceneAsset, instance, ctx);
    m_MaxRenderObjectIndex = std::max(
        m_MaxRenderObjectIndex, addResult.FirstRenderObject + (u32)sceneAsset.Geometry.RenderObjects.size());

    for (auto& node : instanceHierarchy.Nodes)
    {
        const bool isTopLevel = node.Parent == lux::SceneHierarchyHandle::INVALID;
        u32 payloadIndex = node.PayloadIndex;
        switch (node.Type)
        {
        case lux::SceneHierarchyNodeType::Mesh:
            payloadIndex += addResult.FirstRenderObject;
            break;
        case lux::SceneHierarchyNodeType::Light:
            payloadIndex = m_Lights.Add(sceneAsset.Lights.Lights[payloadIndex]);
            break;
        case lux::SceneHierarchyNodeType::Dummy:
        default:
            break;
        }
        m_HierarchyInfo.Nodes.push_back({
            .Type = node.Type,
            .Depth = node.Depth,
            .Parent = isTopLevel ? lux::SceneHierarchyHandle::INVALID : node.Parent + firstNode,
            .LocalTransform = isTopLevel ?
                baseTransform.Combine(node.LocalTransform) :
                node.LocalTransform,
            .PayloadIndex = payloadIndex,
            .Instance = instance
        });
    }
    
    m_HierarchyInfo.MaxDepth = std::max(m_HierarchyInfo.MaxDepth, instanceHierarchy.MaxDepth);

    return {
        .Scene = &sceneAsset,
        .Instance = instance,
        .RenderObjectsOffset = addResult.FirstRenderObject,
    };
}

void Scene::DeleteScene(lux::SceneHandle scene)
{
    m_ReplacedScenes.push_back({.Original = scene, .Replacement = {}});
}

void Scene::ReplaceScene(lux::SceneHandle original, lux::SceneHandle replacement)
{
    m_ReplacedScenes.push_back({.Original = original, .Replacement = replacement});
}

void Scene::UpdateMaterial(lux::SceneHandle scene)
{
    m_UpdatedMaterials.push_back({.Scene = scene});
}

void Scene::HandleReplacements(FrameContext& ctx)
{
    auto getTopNodeLevelTransform = [&](const lux::SceneHierarchyNode& node) -> Transform3d {
        lux::SceneHierarchyHandle parent = node.Parent;
        if (parent == lux::SceneHierarchyHandle::INVALID)
            return node.LocalTransform;

        for (;;)
        {
            lux::SceneHierarchyHandle current = parent;
            parent = m_HierarchyInfo.Nodes[parent.Handle].Parent;
            if (parent == lux::SceneHierarchyHandle::INVALID)
                return m_HierarchyInfo.Nodes[current.Handle].LocalTransform;
        }
    };
    
    if (m_ReplacedScenes.empty())
        return;

    struct ReinstantiationData
    {
        lux::SceneHandle Replacement{};
        SceneInstantiationData InstantiationData{};
    };
    std::unordered_map<lux::SceneInstanceHandle, ReinstantiationData> reinstantiationData;
    for (auto&& [original, replacement] : m_ReplacedScenes)
    {
        auto& originalInstances = m_ScenesMap[original].Instances;
        for (auto& node : m_HierarchyInfo.Nodes)
        {
            if (originalInstances.contains(node.Instance) && !reinstantiationData.contains(node.Instance))
            {
                const lux::SceneAsset& originalScene = *m_SceneAssetManager->Get(original);
                const Transform3d topNodeOriginalTransform = originalScene.Hierarchy.Nodes.front().LocalTransform;
                
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

    for (auto&& [original, _] : m_ReplacedScenes)
    {
        m_Geometry.Delete(*m_SceneAssetManager->Get(original));
        m_ScenesMap[original].HasGeometry = false;
    }
    
    for (auto&& [instance, reinstantiationInfo] : reinstantiationData) {
        if (!reinstantiationInfo.Replacement.IsValid())
            continue;
        MapSceneInstanceToSceneInfo(reinstantiationInfo.Replacement, instance);
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
        if (!m_ScenesMap[instance.Scene].HasGeometry)
        {
            m_Geometry.Add(*m_SceneAssetManager->Get(instance.Scene), ctx);
            m_ScenesMap[instance.Scene].HasGeometry = true;
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
    
    auto reparentToValid = [&](lux::SceneHierarchyNode& node) {
        lux::SceneHierarchyHandle parent = node.Parent;
        while (
            parent != lux::SceneHierarchyHandle::INVALID &&
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
        lux::SceneHierarchyNode& node = m_HierarchyInfo.Nodes[i];
        
        if (m_InstanceIsAlive[node.Instance])
            continue;
        
        if (node.Type == lux::SceneHierarchyNodeType::Light)
            m_Lights.Delete(node.PayloadIndex);

        reorder[currentLast] = i;
        std::swap(m_HierarchyInfo.Nodes[i], m_HierarchyInfo.Nodes[currentLast]);
        currentLast--;
    }

    for (auto& node : m_HierarchyInfo.Nodes)
    {
        if (node.Parent != lux::SceneHierarchyHandle::INVALID)
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
        m_ScenesMap[instance.Scene].Instances.erase(instanceHandle);
        m_InstanceDeletedSignal.Emit({
            .Scene = m_SceneAssetManager->Get(instance.Scene),
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

void updateLight(lux::CommonLight& light, const glm::mat4& transform)
{
    switch (light.Type)
    {
    case lux::LightType::Directional:
        light.PositionDirection = glm::normalize(transform * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f));
        break;
    case lux::LightType::Point:
        light.PositionDirection = transform * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        break;
    case lux::LightType::Spot:
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
        transforms[i] = nodes[i].Parent == lux::SceneHierarchyHandle::INVALID ?
            nodes[i].LocalTransform.ToMatrix() :
            transforms[nodes[i].Parent.Handle] * nodes[i].LocalTransform.ToMatrix();

    for (auto&& [i, node] : std::views::enumerate(nodes))
    {
        switch (node.Type)
        {
        case lux::SceneHierarchyNodeType::Mesh:
            {
                auto& previousTransform = m_RenderObjectPreviousTransforms[node.PayloadIndex];
                updateRenderObject(Device::GetBufferArenaUnderlyingBuffer(Geometry().RenderObjects), node.PayloadIndex,
                    previousTransform, transforms[i], *ctx.ResourceUploader);
                m_RenderObjectPreviousTransforms[node.PayloadIndex] = transforms[i];
            }
            break;
        case lux::SceneHierarchyNodeType::Light:
            updateLight(Lights().Get(node.PayloadIndex), transforms[i]);
            break;
        case lux::SceneHierarchyNodeType::Dummy:
        default:
            break;
        }
    }
}

void Scene::HandleMaterialUpdates(FrameContext& ctx)
{
    if (m_UpdatedMaterials.empty())
        return;
    
    for (auto& updateInfo : m_UpdatedMaterials)
    {
        const lux::SceneHandle sceneHandle = updateInfo.Scene;
        if (!m_ScenesMap.contains(sceneHandle))
            continue;
        
        m_Geometry.UpdateMaterials(*m_SceneAssetManager->Get(sceneHandle), ctx);
    }
    
    m_UpdatedMaterials.clear();
}
