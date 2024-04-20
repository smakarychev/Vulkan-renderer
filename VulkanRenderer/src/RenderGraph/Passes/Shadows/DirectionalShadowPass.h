#pragma once

#include "Light/Light.h"
#include "RenderGraph/Passes/Culling/CullMetaPass.h"

struct DirectionalShadowPassInitInfo
{
    const RG::Geometry* Geometry{nullptr};
};

struct DirectionalShadowPassExecutionInfo
{
    glm::uvec2 Resolution{};
    /* DirectionalShadowPass will construct the suitable shadow camera based on main camera frustum */
    const Camera* MainCamera{nullptr};
    const DirectionalLight* DirectionalLight{nullptr};
    f32 ViewDistance{100};
};

class DirectionalShadowPass
{
public:
    struct PassData
    {
        RG::Resource ShadowMap{};
        f32 Near{1.0f};
        f32 Far{100.0f};
        glm::mat4 ShadowViewProjection{1.0f};
    };
public:
    DirectionalShadowPass(RG::Graph& renderGraph, const DirectionalShadowPassInitInfo& info);
    void AddToGraph(RG::Graph& renderGraph, const DirectionalShadowPassExecutionInfo& info);
private:
    static Camera CreateShadowCamera(const Camera& mainCamera, const glm::vec3& lightDirection, f32 viewDistance);
private:
    std::shared_ptr<CullMetaPass> m_Pass{};

    std::unique_ptr<Camera> m_Camera{};
};
