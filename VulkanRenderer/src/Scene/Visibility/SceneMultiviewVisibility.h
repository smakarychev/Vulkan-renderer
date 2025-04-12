#pragma once

#include "SceneVisibility.h"

#include <array>

class SceneMultiviewVisibility
{
public:
    static constexpr u32 MAX_VIEWS = 64;
public:
    bool AddVisibility(const SceneVisibility& visibility);

    u32 ViewCount() const { return m_ViewCount; }
    const SceneRenderObjectSet* ObjectSet() const;
    Span<const SceneVisibility* const> Visibilities() const;
    
private:
    std::array<const SceneVisibility*, MAX_VIEWS> m_Visibilities{};
    u32 m_ViewCount{0};
};
