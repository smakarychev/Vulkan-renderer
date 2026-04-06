#pragma once

#include <CoreLib/types.h>

class SceneInfo;

class SceneInstance
{
    friend class Scene;
    friend class SceneLight;
    friend class SceneGeometry;
    friend class SceneRenderObjectSet;
    friend struct std::hash<SceneInstance>;
public:
    auto operator<=>(const SceneInstance&) const = default;
private:
    u32 m_InstanceId{0};
    const SceneInfo* m_SceneInfo{};
};

namespace std
{
template <>
struct hash<SceneInstance>
{
    usize operator()(const SceneInstance instance) const noexcept
    {
        return std::hash<u32>{}(instance.m_InstanceId);
    }
};
}
