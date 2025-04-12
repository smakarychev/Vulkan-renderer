#include "SceneMultiviewVisibility.h"

bool SceneMultiviewVisibility::AddVisibility(const SceneVisibility& visibility)
{
    if (m_ViewCount >= MAX_VIEWS)
    {
        LOG("Multiview visibility cannot take more than {} views", MAX_VIEWS);
        return false;
    }
    if (m_ViewCount > 0)
    {
        if (m_Visibilities[m_ViewCount - 1]->m_Set != visibility.m_Set)
        {
            LOG("Multiview visibility acts on visibilities of same set only");
            return false;
        }
    }

    m_Visibilities[m_ViewCount] = &visibility;
    m_ViewCount++;
    
    return true;
}

const SceneRenderObjectSet* SceneMultiviewVisibility::ObjectSet() const
{
    if (m_ViewCount == 0)
        return nullptr;

    return m_Visibilities.front()->m_Set;
}

Span<const SceneVisibility* const> SceneMultiviewVisibility::Visibilities() const
{
    return Span(m_Visibilities.data(), m_ViewCount);
}
