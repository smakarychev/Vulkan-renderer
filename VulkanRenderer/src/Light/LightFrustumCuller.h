#pragma once
#include "Core/Camera.h"

class SceneLight;

class LightFrustumCuller
{
public:
    static void Cull(SceneLight& light, const Camera& camera);
    static void CullDepthSort(SceneLight& light, const Camera& camera);
};
