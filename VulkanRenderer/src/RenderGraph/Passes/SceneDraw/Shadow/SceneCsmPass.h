#pragma once

#include "Settings.h"
#include "Light/Light.h"
#include "Math/Geometry.h"
#include "RenderGraph/RGDrawResources.h"
#include "RenderGraph/RGResource.h"
#include "Scene/ScenePass.h"

struct SceneDrawPassDescription;
class SceneMultiviewVisibility;
class ScenePass;
class Camera;
class SceneLight;
class SceneGeometry;

namespace Passes::SceneCsm
{
    struct CsmInfo
    {
        u32 CascadeCount{0};
        std::array<f32, MAX_SHADOW_CASCADES> Cascades{};
        std::array<glm::mat4, MAX_SHADOW_CASCADES> ViewProjections{};
        std::array<glm::mat4, MAX_SHADOW_CASCADES> Views{};
        std::array<f32, MAX_SHADOW_CASCADES> Near{};
        std::array<f32, MAX_SHADOW_CASCADES> Far{};
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
        RG::Resource CsmInfo{};
        f32 Near{1.0f};
        f32 Far{100.0f};
    };
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);

    ScenePassCreateInfo getScenePassCreateInfo(StringId name);
}
