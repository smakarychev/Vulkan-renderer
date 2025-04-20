#include "SceneView.h"

#include "RenderGraph/Passes/HiZ/HiZCommon.h"

SceneViewGPU SceneViewGPU::FromSceneView(const SceneView& view)
{
    u32 viewFlags = {};
    viewFlags |= (u32)(view.Camera->GetType() == CameraType::Orthographic) << IS_ORTHOGRAPHIC_BIT;
    viewFlags |= (u32)enumHasAny(view.VisibilityFlags, SceneVisibilityFlags::ClampDepth);
    glm::uvec2 hizResolution = enumHasAny(view.VisibilityFlags, SceneVisibilityFlags::OcclusionCull) ?
        HiZ::calculateHizResolution(view.Resolution) : glm::uvec2(0);
    return {
        .ViewMatrix = view.Camera->GetView(),
        .ViewProjectionMatrix = view.Camera->GetViewProjection(),
        .FrustumPlanes = view.Camera->GetFrustumPlanes(),
        .ProjectionData = view.Camera->GetProjectionData(),
        .Resolution = glm::vec2{view.Resolution},
        .HiZResolution = glm::vec2{hizResolution},
        .ViewFlags = viewFlags};
}
