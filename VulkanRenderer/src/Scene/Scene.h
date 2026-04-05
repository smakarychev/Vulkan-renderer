#pragma once

#include "SceneGeometry.h"
#include "SceneLight.h"

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
        SceneInstance Instance{};
        u32 RenderObjectsOffset{0};
    };
    struct DeletedInstanceData
    {
        SceneInstance Instance{};
    };
public:
    static Scene CreateEmpty(DeletionQueue& deletionQueue);
    const SceneGeometry& Geometry() const { return m_Geometry; }
    SceneGeometry& Geometry() { return m_Geometry; }

    const SceneLight& Lights() const { return m_Lights; }
    SceneLight& Lights() { return m_Lights; }

    Signal<NewInstanceData>& GetInstanceAddedSignal() { return m_InstanceAddedSignal; }
    Signal<DeletedInstanceData>& GetInstanceDeletedSignal() { return m_InstanceDeletedSignal; }
    
    SceneInstance Instantiate(const SceneInfo& sceneInfo, const SceneInstantiationData& instantiationData);
    void Delete(SceneInstance instance);

    void OnUpdate(FrameContext& ctx);
    
    template <typename Fn>
    requires requires (Fn fn, CommonLight& light, Transform3d& localTransform)
    {
        { fn(light, localTransform) } -> std::same_as<bool>;
    }
    void IterateLights(LightType lightType, Fn&& callback);

    bool SceneInstanceIsAlive(const SceneInstance& instance) { return m_InstanceIsAlive[instance.m_InstanceId]; }
private:
    SceneInstance RegisterSceneInstance(const SceneInfo& sceneInfo);
    NewInstanceData AddToHierarchy(SceneInstance instance, const Transform3d& baseTransform, FrameContext& ctx);
    void Spawn(FrameContext& ctx);
    void Sweep();
    void UpdateHierarchy(FrameContext& ctx);
private:
    SceneGeometry m_Geometry{};
    SceneLight m_Lights{};
    
    SceneHierarchyInfo m_HierarchyInfo{};
    std::vector<glm::mat4> m_RenderObjectPreviousTransforms;

    std::unordered_map<const SceneInfo*, u32> m_SceneInstancesMap{};
    lux::FreeList<u32> m_ActiveInstancesIndices;
    u32 m_MaxRenderObjectIndex{0};

    Signal<NewInstanceData> m_InstanceAddedSignal{};
    Signal<DeletedInstanceData> m_InstanceDeletedSignal{};

    std::vector<bool> m_InstanceIsAlive;
    struct InstantiationInfo
    {
        SceneInstance Instance{};
        SceneInstantiationData InstantiationData{};
    };
    std::vector<InstantiationInfo> m_NewInstances;
    std::vector<SceneInstance> m_DeletedInstances;
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
