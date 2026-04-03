#include "rendererpch.h"

#include "SceneMultiviewVisibility.h"
#include "Scene/SceneRenderObjectSet.h"

void SceneMultiviewVisibility::Init(const SceneRenderObjectSet& set)
{
    m_Set = &set;
}

void SceneMultiviewVisibility::OnUpdate(FrameContext&)
{
    m_ViewCount = 0;
}

SceneVisibilityHandle SceneMultiviewVisibility::AddVisibility(const SceneView& view)
{
    if (m_ViewCount >= MAX_VIEWS)
    {
        LUX_LOG_ERROR("Multiview visibility cannot take more than {} views", MAX_VIEWS);
        return {SceneVisibilityHandle::INVALID};
    }

    m_Visibilities[m_ViewCount] = view;
    
    const SceneVisibilityHandle toReturn {.Handle = m_ViewCount};
    m_ViewCount++;

    return toReturn;
}

const SceneView& SceneMultiviewVisibility::View(SceneVisibilityHandle handle) const
{
    ASSERT(handle <= m_ViewCount, "Invalid visibility handle")

    return m_Visibilities[handle];
}

const SceneRenderObjectSet& SceneMultiviewVisibility::ObjectSet() const
{
    return *m_Set;
}