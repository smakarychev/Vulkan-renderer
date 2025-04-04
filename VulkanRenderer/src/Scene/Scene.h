#pragma once

#include "RenderObject.h"
#include "SceneAsset.h"
#include "SceneGeometry2.h"
#include "SceneHierarchy.h"
#include "SceneLight2.h"
#include "Signals/Signal.h"

namespace assetLib
{
    struct SceneInfo;
}

class BindlessTextureDescriptorsRingBuffer;

/* used to instantiate a scene */
class SceneInfo
{
    friend class Scene;
    friend class SceneGeometry2;
    friend class SceneLight2;
    friend class SceneHierarchy;
    friend class SceneRenderObjectSet;
public:
    static SceneInfo* LoadFromAsset(std::string_view assetPath,
        BindlessTextureDescriptorsRingBuffer& texturesRingBuffer, DeletionQueue& deletionQueue);
private:
    SceneGeometryInfo m_Geometry{};
    SceneLightInfo m_Lights{};
    SceneHierarchyInfo m_Hierarchy{};
};

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
        u32 MeshletsOffset{0};
    };
public:
    static Scene CreateEmpty(DeletionQueue& deletionQueue);
    const SceneGeometry2& Geometry() const { return m_Geometry; }
    SceneGeometry2& Geometry() { return m_Geometry; }
    SceneLight2& Lights() { return m_Lights; }
    SceneHierarchy& Hierarchy() { return m_Hierarchy; }

    Signal<NewInstanceData>& GetInstanceAddedSignal() { return m_InstanceAddedSignal; }
    
    SceneInstance Instantiate(const SceneInfo& sceneInfo, const SceneInstantiationData& instantiationData,
        FrameContext& ctx);
private:
    SceneInstance RegisterSceneInstance(const SceneInfo& sceneInfo);
private:
    SceneGeometry2 m_Geometry{};
    SceneLight2 m_Lights{};
    SceneHierarchy m_Hierarchy{};
    
    std::unordered_map<const SceneInfo*, u32> m_SceneInstancesMap{};
    u32 m_ActiveInstances{0};

    Signal<NewInstanceData> m_InstanceAddedSignal{};
};