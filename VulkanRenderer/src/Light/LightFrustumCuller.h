#pragma once

class SceneLight;
class Camera;

class LightFrustumCuller
{
public:
    static void Cull(SceneLight& light, const Camera& camera);
    static void CullDepthSort(SceneLight& light, const Camera& camera);
};
