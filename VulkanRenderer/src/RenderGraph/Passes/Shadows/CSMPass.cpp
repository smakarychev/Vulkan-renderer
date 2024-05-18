#include "CSMPass.h"

#include "ShadowPassesCommon.h"
#include "ShadowPassesUtils.h"
#include "imgui/imgui.h"
#include "Light/Light.h"
#include "RenderGraph/Passes/Culling/MutiviewCulling/CullMetaMultiviewPass.h"
#include "utils/MathUtils.h"

CSMPass::CSMPass(RG::Graph& renderGraph, const ShadowPassInitInfo& info)
{
    ShaderPipelineTemplate* shadowTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
        "../assets/shaders/processed/render-graph/shadows/directional-vert.shader"},
        "Pass.Shadow.CSM", renderGraph.GetArenaAllocators());

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

    for (u32 i = 0; i < SHADOW_CASCADES; i++)
        m_MultiviewData.AddView({
            .Geometry = info.Geometry,
            .DrawFeatures = RG::DrawFeatures::Positions,
            .DrawMeshletsPipeline = &meshletPipeline,
            .DrawTrianglesPipeline = &trianglePipeline,
            .CullTriangles = false}); // todo: change me to true once triangle culling is working

    m_MultiviewData.Finalize();
    
    CullMetaMultiviewPassInitInfo multiviewPassInitInfo = {
        .MultiviewData = &m_MultiviewData};
    
    m_Pass = std::make_shared<CullMetaMultiviewPass>(renderGraph, "CSM", multiviewPassInitInfo);
}

void CSMPass::AddToGraph(RG::Graph& renderGraph, const ShadowPassExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    std::vector cascades = CalculateDepthCascades(*info.MainCamera, info.ViewDistance);
    m_Cameras = CreateShadowCameras(*info.MainCamera, info.DirectionalLight->Direction, cascades);

    std::vector<ImageSubresourceDescription::Packed> cascadeViews(SHADOW_CASCADES);
    for (u32 i = 0;  i < SHADOW_CASCADES; i++)
        cascadeViews[i] = ImageSubresourceDescription::Pack({
            .MipmapBase = 0, .Mipmaps = 1, .LayerBase = i, .Layers = 1});
    
    Resource shadow = renderGraph.CreateResource("CSM.ShadowMap", GraphTextureDescription{
        .Width = SHADOW_MAP_RESOLUTION,
        .Height = SHADOW_MAP_RESOLUTION,
        .Layers = SHADOW_CASCADES,
        .Format = Format::D32_FLOAT,
        .Kind = ImageKind::Image2dArray,
        .AdditionalViews = cascadeViews});
    
    std::vector<glm::mat4> matrices(SHADOW_CASCADES);
    for (u32 i = 0; i < SHADOW_CASCADES; i++)
        matrices[i] = m_Cameras[i].GetViewProjection();

    for (u32 i = 0; i < SHADOW_CASCADES; i++)
        m_MultiviewData.UpdateView(i, {
            .Resolution = glm::uvec2{SHADOW_MAP_RESOLUTION},
            .Camera = &m_Cameras[i],
            .ClampDepth = true,
            .DrawAttachments = {
                .Depth = DepthStencilAttachment{
                    .Resource = shadow,
                    .Description = {
                        .Subresource = cascadeViews[i],
                        .OnLoad = AttachmentLoad::Clear,
                        .ClearDepth = 0.0f,
                        .ClearStencil = 0},
                    .DepthBias = DepthBias{.Constant = DEPTH_CONSTANT_BIAS, .Slope = DEPTH_SLOPE_BIAS}}}});
    
    m_Pass->AddToGraph(renderGraph);
    auto& multiviewOutput = renderGraph.GetBlackboard().Get<CullMetaMultiviewPass::PassData>(m_Pass->GetNameHash());

    /* because this pass is just a wrapper around meta cull pass, in order to actually upload data to resource,
     * we need an additional dummy pass
     */
    struct PassDataDummy
    {
        Resource CSM{};
    };
    struct Ubo
    {
        u32 CascadeCount{0};
        std::array<f32, MAX_SHADOW_CASCADES> Cascades{};
        std::array<glm::mat4, MAX_SHADOW_CASCADES> Matrices{};
    };
    renderGraph.AddRenderPass<PassDataDummy>(PassName{"CSM.Dummy"},
        [&](Graph& graph, PassDataDummy& passData)
        {
            passData.CSM = graph.CreateResource("CSM.Data", GraphBufferDescription{
                .SizeBytes = sizeof(Ubo)});

            passData.CSM = graph.Write(passData.CSM, Vertex | Uniform | Upload);

            graph.GetBlackboard().Update(passData);
        },
        [=](PassDataDummy& passData, FrameContext& frameContext, const Resources& resources)
        {
            Ubo ubo = {};
            ubo.CascadeCount = SHADOW_CASCADES;
            std::ranges::copy(cascades.begin(), cascades.end(), ubo.Cascades.begin());
            std::ranges::copy(matrices.begin(), matrices.end(), ubo.Matrices.begin());
            resources.GetBuffer(passData.CSM, ubo, *frameContext.ResourceUploader);
        });
    
    PassData passData = {
        .ShadowMap = *multiviewOutput.DrawAttachmentResources.back().Depth,
        .CSM = renderGraph.GetBlackboard().Get<PassDataDummy>().CSM,
        .Near = m_Cameras.front().GetFrustumPlanes().Near,
        .Far = m_Cameras.back().GetFrustumPlanes().Far};
    renderGraph.GetBlackboard().Update(passData);
}

std::vector<f32> CSMPass::CalculateDepthCascades(const Camera& mainCamera, f32 viewDistance)
{
    static f32 SPLIT_LAMBDA = 0.0f;
    ImGui::Begin("CSM lambda");
    ImGui::DragFloat("Lambda", &SPLIT_LAMBDA, 1e-2f, 0.0f, 1.0f);
    ImGui::End();

    f32 near = mainCamera.GetFrustumPlanes().Near;
    f32 far = std::min(mainCamera.GetFrustumPlanes().Far, viewDistance);

    f32 depthRange = far - near;
    f32 depthRatio = far / near;
    
    std::vector depthSplits(SHADOW_CASCADES, 0.0f);
    for (u32 i = 0; i < SHADOW_CASCADES; i++)
    {
        /* https://developer.nvidia.com/gpugems/gpugems3/
         * part-ii-light-and-shadows/chapter-10-parallel-split-shadow-maps-programmable-gpus */
        f32 iOverN = (f32)(i + 1) / (f32)SHADOW_CASCADES;
        f32 cLog = near * std::pow(depthRatio, iOverN);
        f32 cUniform = near + depthRange * iOverN;

        f32 split = MathUtils::lerp(cLog, cUniform, SPLIT_LAMBDA);
        depthSplits[i] = split;
    }

    return depthSplits;
}

std::vector<Camera> CSMPass::CreateShadowCameras(const Camera& mainCamera, const glm::vec3& lightDirection,
        const std::vector<f32>& cascades)
{
    f32 near = mainCamera.GetFrustumPlanes().Near;

    /* create shadow cameras */
    std::vector<Camera> cameras;
    cameras.reserve(SHADOW_CASCADES);
    
    glm::vec3 up = abs(lightDirection.y) < 0.999f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(0.0f, 0.0f, 1.0f);
    for (u32 i = 0; i < SHADOW_CASCADES; i++)
    {
        f32 previousDepth = i > 0 ? cascades[i - 1] : near;
        f32 depth = cascades[i];
        
        FrustumCorners corners = mainCamera.GetFrustumCorners(previousDepth, depth);
        ShadowProjectionBounds bounds = ShadowUtils::projectionBoundsSphereWorld(corners);

        glm::vec3 cameraPosition = bounds.Centroid + lightDirection * bounds.Min.z;
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

        cameras.push_back(shadowCamera);
    }
    
    return cameras;
}
