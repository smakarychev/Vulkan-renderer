#pragma once

#include "RenderGraph/Passes/Culling/CullMetaPass.h"

struct ShadowPassExecutionInfo;
struct ShadowPassInitInfo;

class DirectionalShadowPass
{
public:
    struct PassData
    {
        RG::Resource ShadowMap{};
        RG::Resource ShadowUbo{};
        f32 Near{1.0f};
        f32 Far{100.0f};
    };
public:
    DirectionalShadowPass(RG::Graph& renderGraph, const ShadowPassInitInfo& info);
    void AddToGraph(RG::Graph& renderGraph, const ShadowPassExecutionInfo& info);
private:
    static Camera CreateShadowCamera(const Camera& mainCamera, const glm::vec3& lightDirection, f32 viewDistance);
private:
    std::shared_ptr<CullMetaPass> m_Pass{};

    std::unique_ptr<Camera> m_Camera{};
};
