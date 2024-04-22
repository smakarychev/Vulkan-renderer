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
        .DepthClamp()
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

    m_Camera = std::make_unique<Camera>(CreateShadowCamera(*info.MainCamera,
        info.DirectionalLight->Direction, info.ViewDistance));
    
    Resource shadow = renderGraph.CreateResource("DirectionalShadow.ShadowMap",
        GraphTextureDescription{
            .Width = SHADOW_MAP_RESOLUTION,
            .Height = SHADOW_MAP_RESOLUTION,
            .Format = Format::D32_FLOAT});
    
    // todo: to cvar
    static constexpr f32 DEPTH_CONSTANT_BIAS = -1.0f;
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
    
    glm::vec3 up = abs(lightDirection.y) < 0.999f ?
        glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(0.0f, 0.0f, 1.0f);

    f32 boundingSphereRadius = 0.0f;
    for (auto& p : corners)
        boundingSphereRadius = std::max(boundingSphereRadius, glm::distance2(p, centroid));
    boundingSphereRadius = std::sqrt(boundingSphereRadius);
    static constexpr f32 RADIUS_SNAP = 16.0f;
    boundingSphereRadius = std::ceil(boundingSphereRadius * RADIUS_SNAP) / RADIUS_SNAP;

    glm::vec3 max = glm::vec3{boundingSphereRadius};
    glm::vec3 min = -max;

    glm::vec3 cameraPosition = centroid + lightDirection * min.z;

    Camera shadowCamera = Camera::Orthographic({
        .BaseInfo = {
            .Position = cameraPosition,
            .Orientation = glm::normalize(glm::quatLookAt(lightDirection, up)),
            .Near = 0.0f,
            .Far = max.z - min.z,
            .ViewportWidth = SHADOW_MAP_RESOLUTION,
            .ViewportHeight = SHADOW_MAP_RESOLUTION},
        .Left = min.x,
        .Right = max.x,
        .Bottom = min.y,
        .Top = max.y});

    /* stabilize the camera */
    glm::mat4 shadowMatrix = shadowCamera.GetViewProjection();
    glm::vec4 shadowOrigin = glm::vec4{0.0f, 0.0f, 0.0f, 1.0f};
    /* transform the world origin to shadow-projected space, and scale by half resolution, to
     * get into the pixel space;
     * then snap to the closest pixel and transform the offset back to world space units.
     * this also scales z and w coordinate, but it is ignored */
    shadowOrigin = shadowMatrix * shadowOrigin * ((f32)SHADOW_MAP_RESOLUTION / 2.0f);
    glm::vec4 shadowOriginRounded = glm::round(shadowOrigin);
    glm::vec4 roundingOffset = shadowOriginRounded - shadowOrigin;
    roundingOffset.z = roundingOffset.w = 0.0f;
    roundingOffset *= 2.0f / (f32)SHADOW_MAP_RESOLUTION;
    glm::mat4 stabilizedProjection = shadowCamera.GetProjection();
    stabilizedProjection[3] += roundingOffset;
    shadowCamera.SetProjection(stabilizedProjection);

    return shadowCamera;
}
