#pragma once

#include "Light/Light.h"
#include "Math/Geometry.h"
#include "RenderGraph/RGDrawResources.h"
#include "RenderGraph/RGResource.h"
#include "RenderGraph/Passes/Generated/Types/CsmDataUniform.generated.h"
#include "Scene/ScenePass.h"

class SceneDrawPassViewAttachments;
struct SceneDrawPassDescription;
class SceneMultiviewVisibility;
class ScenePass;
class Camera;
class SceneLight;
class SceneGeometry;

namespace Passes::SceneCsm
{
struct CsmInfo : ::gen::CsmData
{
};
struct ExecutionInfo
{
    const ScenePass* Pass{nullptr}; 
    const SceneGeometry* Geometry{nullptr};
    SceneMultiviewVisibility* MultiviewVisibility{nullptr};
    /* pass will construct the suitable shadow camera based on main camera frustum */
    const Camera* MainCamera{nullptr};
    DirectionalLight DirectionalLight{};
    f32 ShadowMin{0};
    f32 ShadowMax{100};
    bool StabilizeCascades{false};
    AABB GeometryBounds{};
};
struct PassData
{
    std::vector<SceneDrawPassDescription> MetaPassDescriptions;
    RG::CsmData CsmData{};
};
PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
void mergeCsm(RG::Graph& renderGraph, PassData& passData, const ScenePass& scenePass,
    const SceneDrawPassViewAttachments& attachments);
ScenePassCreateInfo getScenePassCreateInfo(StringId name);
}
