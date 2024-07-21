#include "DirectionalShadowPass.h"

#include "ShadowPassesCommon.h"
#include "ShadowPassesUtils.h"
#include "Light/Light.h"

DirectionalShadowPass::DirectionalShadowPass(RG::Graph& renderGraph, const ShadowPassInitInfo& info)
{
    ShaderPipelineTemplate* shadowTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
        "../assets/shaders/processed/render-graph/shadows/directional-vert.stage"},
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
        .ClampDepth = true,
        .CameraType = CameraType::Orthographic};

    m_Pass = std::make_shared<CullMetaPass>(renderGraph, shaderPassInitInfo, "DirectionalShadow");
}

void DirectionalShadowPass::AddToGraph(RG::Graph& renderGraph, const ShadowPassExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    m_Camera = std::make_unique<Camera>(CreateShadowCamera(*info.MainCamera,
        info.DirectionalLight->Direction, info.ViewDistance, info.GeometryBounds));
    
    Resource shadow = renderGraph.CreateResource("DirectionalShadow.ShadowMap",
        GraphTextureDescription{
            .Width = SHADOW_MAP_RESOLUTION,
            .Height = SHADOW_MAP_RESOLUTION,
            .Format = Format::D32_FLOAT});
    
    m_Pass->AddToGraph(renderGraph, {
        .Resolution = glm::uvec2{SHADOW_MAP_RESOLUTION},
        .Camera = m_Camera.get(),
        .DrawInfo = {
            .Attachments = {
                .Depth = DepthStencilAttachment{
                    .Resource = shadow,
                    .Description = {
                        .OnLoad = AttachmentLoad::Clear,
                        .ClearDepth = 0.0f,
                        .ClearStencil = 0},
                    .DepthBias = DepthBias{.Constant = DEPTH_CONSTANT_BIAS, .Slope = DEPTH_SLOPE_BIAS}}}}});
    auto& output = renderGraph.GetBlackboard().Get<CullMetaPass::PassData>(m_Pass->GetNameHash());

    /* because this pass is just a wrapper around meta cull pass, in order to actually upload data to resource,
    * we need an additional dummy pass
    */
    struct PassDataDummy
    {
        Resource Shadow{};
        glm::mat4 CameraViewProjection{};
    };
    renderGraph.AddRenderPass<PassDataDummy>(PassName{"Shadow.Directional.Dummy"},
        [&](Graph& graph, PassDataDummy& passData)
        {
            passData.Shadow = graph.CreateResource("Shadow.Directional.Data", GraphBufferDescription{
                .SizeBytes = sizeof(glm::mat4)});

            passData.Shadow = graph.Write(passData.Shadow, Vertex | Uniform | Upload);

            passData.CameraViewProjection = m_Camera->GetViewProjection();

            graph.GetBlackboard().Update(passData);
        },
        [=](PassDataDummy& passData, FrameContext& frameContext, const Resources& resources)
        {
            resources.GetBuffer(passData.Shadow, passData.CameraViewProjection, *frameContext.ResourceUploader);
        });

    PassData passData = {
        .ShadowMap = *output.DrawAttachmentResources.Depth,
        .Shadow = renderGraph.GetBlackboard().Get<PassDataDummy>().Shadow,
        .Near = m_Camera->GetFrustumPlanes().Near,
        .Far = m_Camera->GetFrustumPlanes().Far};
    renderGraph.GetBlackboard().Update(passData);
}

Camera DirectionalShadowPass::CreateShadowCamera(const Camera& mainCamera, const glm::vec3& lightDirection,
    f32 viewDistance, const AABB& geometryBounds)
{
    /* get world space location of frustum corners */
    FrustumCorners corners = mainCamera.GetFrustumCorners(viewDistance);
    
    ShadowProjectionBounds bounds = ShadowUtils::projectionBoundsSphereWorld(corners, geometryBounds);

    glm::vec3 cameraPosition = bounds.Centroid + lightDirection * bounds.Min.z;

    glm::vec3 up = abs(lightDirection.y) < 0.999f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(0.0f, 0.0f, 1.0f);
    Camera shadowCamera = Camera::Orthographic({
        .BaseInfo = {
            .Position = cameraPosition,
            .Orientation = glm::normalize(glm::quatLookAt(lightDirection, up)),
            .Near = 0.0f,
            .Far = bounds.Max.z - bounds.Min.z,
            .ViewportWidth = SHADOW_MAP_RESOLUTION,
            .ViewportHeight = SHADOW_MAP_RESOLUTION},
        .Left = bounds.Min.x,
        .Right = bounds.Max.x,
        .Bottom = bounds.Min.y,
        .Top = bounds.Max.y});

    /* stabilize the camera */
    ShadowUtils::stabilizeShadowProjection(shadowCamera, SHADOW_MAP_RESOLUTION);

    return shadowCamera;
}
