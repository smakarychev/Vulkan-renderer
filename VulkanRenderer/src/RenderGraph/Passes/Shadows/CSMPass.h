#pragma once

#include "Core/Camera.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/Passes/Culling/CullMetaPass.h"

class CullMetaPass;
struct ShadowPassExecutionInfo;
struct ShadowPassInitInfo;

class CSMPass
{
public:
    struct PassData
    {
        RG::Resource ShadowMap{};
        RG::Resource CSM{};
        f32 Near{1.0f};
        f32 Far{100.0f};
    };
public:
    CSMPass(RG::Graph& renderGraph, const ShadowPassInitInfo& info);
    void AddToGraph(RG::Graph& renderGraph, const ShadowPassExecutionInfo& info);
private:
    static std::vector<f32> CalculateDepthCascades(const Camera& mainCamera, f32 viewDistance);
    static std::vector<Camera> CreateShadowCameras(const Camera& mainCamera, const glm::vec3& lightDirection,
        const std::vector<f32>& cascades);
private:
    std::vector<CullMetaPass> m_Passes{};

    std::vector<Camera> m_Cameras{};
};
