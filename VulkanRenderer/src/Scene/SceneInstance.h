#pragma once

#include "types.h"

class SceneInfo;

class SceneInstance
{
    friend class Scene;
    friend class SceneHierarchy;
    friend class SceneLight;
    friend class SceneGeometry;
    u32 m_InstanceId{0};
    const SceneInfo* m_SceneInfo{};
};
