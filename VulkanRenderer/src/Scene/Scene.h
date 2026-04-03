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
        const SceneInfo* SceneInfo{nullptr};
        u32 RenderObjectsOffset{0};
    };
public:
    static Scene CreateEmpty(DeletionQueue& deletionQueue);
    const SceneGeometry& Geometry() const { return m_Geometry; }
    SceneGeometry& Geometry() { return m_Geometry; }

    const SceneLight& Lights() const { return m_Lights; }
    SceneLight& Lights() { return m_Lights; }

    Signal<NewInstanceData>& GetInstanceAddedSignal() { return m_InstanceAddedSignal; }
    
    SceneInstance Instantiate(const SceneInfo& sceneInfo, const SceneInstantiationData& instantiationData,
        FrameContext& ctx);
    void Delete(SceneInstance instance);

    void OnUpdate(FrameContext& ctx);
    
    template <typename Fn>
    requires requires (Fn fn, CommonLight& light, Transform3d& localTransform)
    {
        { fn(light, localTransform) } -> std::same_as<bool>;
    }
    void IterateLights(LightType lightType, Fn&& callback);

    bool SceneInstanceIsAlive(const SceneInstance& instance) { return m_InstancesStatus[instance.m_InstanceId]; }
private:
    SceneInstance RegisterSceneInstance(const SceneInfo& sceneInfo);
    void AddToHierarchy(SceneInstance instance, const Transform3d& baseTransform);
    void UpdateHierarchy(FrameContext& ctx);
private:
    SceneGeometry m_Geometry{};
    SceneLight m_Lights{};
    
    u32 m_LastRenderObject{0};
    u32 m_LastLight{0};
    
    SceneHierarchyInfo m_HierarchyInfo{};
    std::vector<glm::mat4> m_RenderObjectPreviousTransforms;
    
    
    std::unordered_map<const SceneInfo*, u32> m_SceneInstancesMap{};
    u32 m_ActiveInstances{0};

    Signal<NewInstanceData> m_InstanceAddedSignal{};

    std::vector<bool> m_InstancesStatus;
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
