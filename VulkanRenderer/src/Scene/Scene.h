#pragma once

#include "Assets/Scenes/SceneAssetManager.h"
#include "SceneGeometry.h"
#include "SceneLight.h"
#include "CoreLib/Containers/FreeList.h"

#include <CoreLib/Signals/Signal.h>

class BindlessTextureDescriptorsRingBuffer;

struct SceneInstantiationData
{
    Transform3d Transform{};
};

class Scene
{
public:
    struct NewInstanceData
    {
        const lux::SceneAsset* Scene{nullptr};
        lux::SceneInstanceHandle Instance{};
        u32 RenderObjectsOffset{0};
    };
    struct DeletedInstanceData
    {
        const lux::SceneAsset* Scene{nullptr};
        lux::SceneInstanceHandle Instance{};
    };
public:
    Scene(DeletionQueue& deletionQueue, lux::SceneAssetManager& sceneAssetManager);
    const SceneGeometry& Geometry() const { return m_Geometry; }
    SceneGeometry& Geometry() { return m_Geometry; }

    const SceneLight& Lights() const { return m_Lights; }
    SceneLight& Lights() { return m_Lights; }

    Signal<NewInstanceData>& GetInstanceAddedSignal() { return m_InstanceAddedSignal; }
    Signal<DeletedInstanceData>& GetInstanceDeletedSignal() { return m_InstanceDeletedSignal; }
    
    lux::SceneInstanceHandle Instantiate(lux::SceneHandle scene, const SceneInstantiationData& instantiationData);
    void Delete(lux::SceneInstanceHandle instance);

    void OnUpdate(FrameContext& ctx);
    
    template <typename Fn>
    requires requires (Fn fn, lux::CommonLight& light, Transform3d& localTransform)
    {
        { fn(light, localTransform) } -> std::same_as<bool>;
    }
    void IterateLights(lux::LightType lightType, Fn&& callback);
private:
    lux::SceneInstanceHandle RegisterSceneInstance(lux::SceneHandle scene);
    void MapSceneInstanceToSceneInfo(lux::SceneHandle scene, lux::SceneInstanceHandle handle);
    void InstantiateHandle(lux::SceneInstanceHandle handle, const SceneInstantiationData& instantiationData);
    NewInstanceData AddToHierarchy(lux::SceneInstanceHandle instance, const Transform3d& baseTransform,
        FrameContext& ctx);
    void DeleteScene(lux::SceneHandle scene);
    void ReplaceScene(lux::SceneHandle original, lux::SceneHandle replacement);
    void UpdateMaterial(lux::SceneHandle scene);
    void HandleReplacements(FrameContext& ctx);
    void HandleSpawnAndSweep(FrameContext& ctx, bool reclaimHandles);
    void Spawn(FrameContext& ctx);
    void Sweep(bool reclaimHandles);
    void UpdateHierarchy(FrameContext& ctx);
    void HandleMaterialUpdates(FrameContext& ctx);
private:
    lux::SceneAssetManager* m_SceneAssetManager{};
    SceneGeometry m_Geometry{};
    SceneLight m_Lights{};
    
    lux::SceneHierarchyInfo m_HierarchyInfo{};
    std::vector<glm::mat4> m_RenderObjectPreviousTransforms;
    
    SignalHandler<lux::SceneAssetManager::SceneDeletedInfo> m_SceneDeletedHandler;
    SignalHandler<lux::SceneAssetManager::SceneReplacedInfo> m_SceneReplacedHandler;
    SignalHandler<lux::SceneAssetManager::MaterialUpdatedInfo> m_MaterialUpdatedHandler;

    struct RegisteredSceneInfo
    {
        bool HasGeometry{false};
        std::unordered_set<lux::SceneInstanceHandle> Instances;        
    };
    std::unordered_map<lux::SceneHandle, RegisteredSceneInfo> m_ScenesMap{};
    lux::FreeList<SceneInstance> m_ActiveInstances;
    u32 m_MaxRenderObjectIndex{0};

    Signal<NewInstanceData> m_InstanceAddedSignal{};
    Signal<DeletedInstanceData> m_InstanceDeletedSignal{};

    std::vector<bool> m_InstanceIsAlive;
    struct InstantiationInfo
    {
        lux::SceneInstanceHandle Instance{};
        SceneInstantiationData InstantiationData{};
    };
    std::vector<InstantiationInfo> m_NewInstances;
    std::vector<lux::SceneInstanceHandle> m_DeletedInstances;

    struct ReplacementsInfo
    {
        lux::SceneHandle Original{};
        lux::SceneHandle Replacement{};
    };
    std::vector<ReplacementsInfo> m_ReplacedScenes;
    struct MaterialUpdatesInfo
    {
        lux::SceneHandle Scene{};
    };
    std::vector<MaterialUpdatesInfo> m_UpdatedMaterials;
};

template <typename Fn>
requires requires (Fn fn, lux::CommonLight& light, Transform3d& localTransform)
{
    { fn(light, localTransform) } -> std::same_as<bool>;
}
void Scene::IterateLights(lux::LightType lightType, Fn&& callback)
{
    for (auto& node : m_HierarchyInfo.Nodes)
    {
        if (node.Type != lux::SceneHierarchyNodeType::Light)
            continue;

        lux::CommonLight& light = m_Lights.Get(node.PayloadIndex);
        if (light.Type == lightType && callback(light, node.LocalTransform))
            break;
    }
}
