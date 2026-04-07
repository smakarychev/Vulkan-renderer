#pragma once

#include <CoreLib/types.h>

class SceneInfo;

class SceneInstance
{
    friend class Scene;
private:
    const SceneInfo* m_SceneInfo{};
};

using SceneInstanceHandle = u32;
