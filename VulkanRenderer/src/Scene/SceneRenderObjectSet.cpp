#include "SceneRenderObjectSet.h"

void SceneRenderObjectSet::Init(std::string_view name, Scene& scene, Span<const ScenePass> passes)
{
    m_Name = name;
    m_Passes.append_range(passes);
    m_NewInstanceHandler = SignalHandler<InstanceData>([this](const InstanceData& instanceData)
    {
        OnNewSceneInstance(instanceData);
    });
    m_NewInstanceHandler.Connect(scene.GetInstanceAddedSignal());
}

void SceneRenderObjectSet::OnNewSceneInstance(const InstanceData& instanceData)
{
    const SceneGeometryInfo& geometry = instanceData.SceneInfo->m_Geometry;
    for (u32 renderObjectIndex = 0; renderObjectIndex < geometry.Meshes.size(); renderObjectIndex++)
    {
        const bool isFiltered = std::ranges::any_of(m_Passes, [&geometry, renderObjectIndex](ScenePass& pass) {
            return pass.Filter(geometry, renderObjectIndex);
        });
        
        if (isFiltered)
            m_RenderObjects.push_back({.RenderObjectIndex = renderObjectIndex});
    }
}

