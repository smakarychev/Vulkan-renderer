#include "DirectionalShadowPass.h"

#include "imgui/imgui.h"

DirectionalShadowPass::DirectionalShadowPass(RG::Graph& renderGraph, const DirectionalShadowPassInitInfo& info)
{
    ShaderPipelineTemplate* shadowTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
        "../assets/shaders/processed/render-graph/shadows/directional-vert.shader"},
        "Pass.Shadow.Directional", renderGraph.GetArenaAllocators());

    ShaderPipeline::Builder pipelineBuilder = ShaderPipeline::Builder()
        .SetTemplate(shadowTemplate)
        .SetRenderingDetails({
            .DepthFormat = Format::D32_FLOAT})
        /* enable depth bias */
        .DynamicStates(DynamicStates::Default | DynamicStates::DepthBias)
        .AlphaBlending(AlphaBlending::None)
        .UseDescriptorBuffer();
    
    ShaderPipeline trianglePipeline = pipelineBuilder
        .Build();
    ShaderPipeline meshletPipeline = pipelineBuilder
        .AddSpecialization("COMPOUND_INDEX", false)
        .Build();

    CullMetaPassInitInfo shaderPassInitInfo = {
        .Geometry = info.Geometry,
        .DrawTrianglesPipeline = &trianglePipeline,
        .DrawMeshletsPipeline = &meshletPipeline,
        .DrawFeatures = RG::DrawFeatures::Positions,
        .CameraType = CameraType::Orthographic};

    m_Pass = std::make_shared<CullMetaPass>(renderGraph, shaderPassInitInfo, "DirectionalShadow");
}

void DirectionalShadowPass::AddToGraph(RG::Graph& renderGraph, const DirectionalShadowPassExecutionInfo& info)
{
    using namespace RG;

    m_Camera = std::make_unique<Camera>(CreateShadowCamera(*info.MainCamera, info.LightDirection, info.ViewDistance));

    
    Resource shadow = renderGraph.CreateResource("DirectionalShadow.ShadowMap",
        GraphTextureDescription{
            .Width = SHADOW_MAP_RESOLUTION,
            .Height = SHADOW_MAP_RESOLUTION,
            .Format = Format::D32_FLOAT});
    
    // todo: to cvar
    static constexpr f32 DEPTH_CONSTANT_BIAS = 0.0f;
    static constexpr f32 DEPTH_SLOPE_BIAS = -1.5f;
    
    m_Pass->AddToGraph(renderGraph, {
        .Resolution = glm::uvec2{SHADOW_MAP_RESOLUTION},
        .Camera = m_Camera.get(),
        .Depth = CullMetaPassExecutionInfo::DepthInfo{
            .Depth = shadow,
            .OnLoad = AttachmentLoad::Clear,
            .DepthBias = DepthBias{.Constant = DEPTH_CONSTANT_BIAS, .Slope = DEPTH_SLOPE_BIAS},
            .ClearValue = {.DepthStencil = {.Depth = 0.0f, .Stencil = 0}}}});

    auto& output = renderGraph.GetBlackboard().Get<CullMetaPass::PassData>(m_Pass->GetNameHash());
    PassData passData = {
        .ShadowMap = *output.DrawAttachmentResources.DepthTarget,
        .Near = m_Camera->GetFrustumPlanes().Near,
        .Far = m_Camera->GetFrustumPlanes().Far,
        .ShadowViewProjection = m_Camera->GetViewProjection()};
    renderGraph.GetBlackboard().Update(passData);
}

Camera DirectionalShadowPass::CreateShadowCamera(const Camera& mainCamera, const glm::vec3& lightDirection,
    f32 viewDistance)
{
    // todo: to cvar
    static constexpr f32 CAMERA_FAR = 40.0f;
    
    /* get world space location of frustum corners */
    FrustumCorners corners = mainCamera.GetFrustumCorners(viewDistance);
    
    /* find the centroid */
    glm::vec3 centroid = {};
    for (auto& p : corners)
        centroid += p;
    centroid /= (f32)corners.size();
    
    /* offset in the opposite light direction */
    glm::vec3 shadowCameraPosition = centroid - lightDirection * CAMERA_FAR;
    glm::vec3 up = abs(lightDirection.y) < 0.999f ?
        glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(0.0f, 0.0f, 1.0f);
    glm::mat4 view = glm::lookAt(shadowCameraPosition, centroid, up);
    
    /* compute the bounds of original frustum in the newly formed view matrix */
    glm::vec3 min{std::numeric_limits<f32>::max()};
    glm::vec3 max = -min;
    for (auto& p : corners)
    {
        glm::vec3 viewLocal = view * glm::vec4{p, 1.0f};
        /* this can be written a little bit more efficiently, since either min or max is updated; but honestly
         * I do not think this will be noticeable at all */
        min = glm::min(min, viewLocal);
        max = glm::max(max, viewLocal);
    }

    f32 near = -max.z - CAMERA_FAR;
    f32 far = -min.z;

    return Camera::Orthographic({
        .BaseInfo = {
            .Position = shadowCameraPosition,
            .Orientation = glm::normalize(glm::quatLookAt(lightDirection, up)),
            .Near = near,
            .Far = far,
            .ViewportWidth = SHADOW_MAP_RESOLUTION,
            .ViewportHeight = SHADOW_MAP_RESOLUTION},
        .Left = min.x,
        .Right = max.x,
        .Bottom = min.y,
        .Top = max.y});
}
