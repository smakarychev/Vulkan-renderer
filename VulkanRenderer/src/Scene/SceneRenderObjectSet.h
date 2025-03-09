#pragma once

#include "types.h"
#include "Scene.h"
#include "ScenePass.h"

class SceneInfo;

struct SceneRenderObjectHandle
{
    u32 RenderObjectIndex{0};
};

class SceneRenderObjectSet
{
public:
    void Init(std::string_view name, Scene& scene, Span<const ScenePass> passes);
private:
    using InstanceData = Scene::NewInstanceData;
    void OnNewSceneInstance(const InstanceData& instanceData);
private:
    using InstanceData = Scene::NewInstanceData;
    SignalHandler<InstanceData> m_NewInstanceHandler;
    std::vector<SceneRenderObjectHandle> m_RenderObjects;
    std::vector<ScenePass> m_Passes;
    
    std::string m_Name{};
};
