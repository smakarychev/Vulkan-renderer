#include "SceneView.h"

SceneViewGPU SceneViewGPU::FromSceneView(const SceneView& view)
{
    u32 viewFlags = {};
    viewFlags |= (u32)(view.Camera->GetType() == CameraType::Orthographic) << IS_ORTHOGRAPHIC_BIT;
    viewFlags |= (u32)enumHasAny(view.VisibilityFlags, SceneVisibilityFlags::ClampDepth);
    return {
        .ViewMatrix = view.Camera->GetView(),
        .ViewProjectionMatrix = view.Camera->GetViewProjection(),
        .FrustumPlanes = view.Camera->GetFrustumPlanes(),
        .ProjectionData = view.Camera->GetProjectionData(),
        .Resolution = glm::vec2{view.Resolution},
        .ViewFlags = viewFlags};
}