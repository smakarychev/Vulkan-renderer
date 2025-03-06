#pragma once

class SceneLight2;
class Camera;
class SceneLight;

class LightFrustumCuller
{
public:
    static void Cull(SceneLight& light, const Camera& camera);
    static void CullDepthSort(SceneLight& light, const Camera& camera);

    static void Cull(SceneLight2& light, const Camera& camera);
    static void CullDepthSort(SceneLight2& light, const Camera& camera);
};
