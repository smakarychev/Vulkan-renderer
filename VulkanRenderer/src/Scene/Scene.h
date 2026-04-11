#pragma once

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
        const SceneInfo* SceneInfo{nullptr};
        SceneInstanceHandle Instance{};
        u32 RenderObjectsOffset{0};
    };
    struct DeletedInstanceData
    {
        const SceneInfo* SceneInfo{nullptr};
        SceneInstanceHandle Instance{};
    };
public:
    static Scene CreateEmpty(DeletionQueue& deletionQueue);
    const SceneGeometry& Geometry() const { return m_Geometry; }
    SceneGeometry& Geometry() { return m_Geometry; }

    const SceneLight& Lights() const { return m_Lights; }
    SceneLight& Lights() { return m_Lights; }

    Signal<NewInstanceData>& GetInstanceAddedSignal() { return m_InstanceAddedSignal; }
    Signal<DeletedInstanceData>& GetInstanceDeletedSignal() { return m_InstanceDeletedSignal; }
    
    SceneInstanceHandle Instantiate(const SceneInfo& sceneInfo, const SceneInstantiationData& instantiationData);
    void Delete(SceneInstanceHandle instance);
    void ReplaceScene(const SceneInfo& original, const SceneInfo& replacement);

    void OnUpdate(FrameContext& ctx);
    
    template <typename Fn>
    requires requires (Fn fn, CommonLight& light, Transform3d& localTransform)
    {
        { fn(light, localTransform) } -> std::same_as<bool>;
    }
    void IterateLights(LightType lightType, Fn&& callback);
private:
    SceneInstanceHandle RegisterSceneInstance(const SceneInfo& sceneInfo);
    void MapSceneInstanceToSceneInfo(const SceneInfo& sceneInfo, SceneInstanceHandle handle);
    void InstantiateHandle(SceneInstanceHandle handle, const SceneInstantiationData& instantiationData);
    NewInstanceData AddToHierarchy(SceneInstanceHandle instance, const Transform3d& baseTransform, FrameContext& ctx);
    void HandleReplacements(FrameContext& ctx);
    void HandleSpawnAndSweep(FrameContext& ctx, bool reclaimHandles);
    void Spawn(FrameContext& ctx);
    void Sweep(bool reclaimHandles);
    void UpdateHierarchy(FrameContext& ctx);
private:
    SceneGeometry m_Geometry{};
    SceneLight m_Lights{};
    
    SceneHierarchyInfo m_HierarchyInfo{};
    std::vector<glm::mat4> m_RenderObjectPreviousTransforms;

    struct RegisteredSceneInfo
    {
        bool HasGeometry{false};
        std::unordered_set<SceneInstanceHandle> Instances;        
    };
    std::unordered_map<const SceneInfo*, RegisteredSceneInfo> m_SceneInstancesMap{};
    lux::FreeList<SceneInstance> m_ActiveInstances;
    u32 m_MaxRenderObjectIndex{0};

    Signal<NewInstanceData> m_InstanceAddedSignal{};
    Signal<DeletedInstanceData> m_InstanceDeletedSignal{};

    std::vector<bool> m_InstanceIsAlive;
    struct InstantiationInfo
    {
        SceneInstanceHandle Instance{};
        SceneInstantiationData InstantiationData{};
    };
    std::vector<InstantiationInfo> m_NewInstances;
    std::vector<SceneInstanceHandle> m_DeletedInstances;

    struct ReplacementsInfo
    {
        const SceneInfo* Original{nullptr};
        const SceneInfo* Replacement{nullptr};
    };
    std::vector<ReplacementsInfo> m_ReplacedScenes;
};

template <typename Fn>
requires requires (Fn fn, CommonLight& light, Transform3d& localTransform)
{
    { fn(light, localTransform) } -> std::same_as<bool>;
}
void Scene::IterateLights(LightType lightType, Fn&& callback)
{
    for (auto& node : m_HierarchyInfo.Nodes)
    {
        if (node.Type != SceneHierarchyNodeType::Light)
            continue;

        CommonLight& light = m_Lights.Get(node.PayloadIndex);
        if (light.Type == lightType && callback(light, node.LocalTransform))
            break;
    }
}
