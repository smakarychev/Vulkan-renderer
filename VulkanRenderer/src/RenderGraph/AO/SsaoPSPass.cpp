#include "SsaoPSPass.h"

#include "FrameContext.h"
#include "Core/Camera.h"
#include "Core/Random.h"
#include "imgui/imgui.h"
#include "Vulkan/RenderCommand.h"

SsaoPSPass::SsaoPSPass(RenderGraph::Graph& renderGraph)
{
    static constexpr u32 RANDOM_SIZE = 4;
    std::vector<u32> pixels((u64)RANDOM_SIZE * (u64)RANDOM_SIZE);
    for (u32& pixel : pixels)
    {
        glm::vec3 randomDir =  {
            Random::Float(-1.0f, 1.0f),
            Random::Float(-1.0f, 1.0f),
            0.0f};
        randomDir = glm::normalize(randomDir);
        pixel = ImageUtils::toRGBA8SNorm(glm::vec4{randomDir, 1.0f});
    }

    m_NoiseTexture = Texture::Builder({
            .Width = RANDOM_SIZE,
            .Height = RANDOM_SIZE,
            .Format = Format::RGBA8_SNORM,
            .Usage = ImageUsage::Sampled | ImageUsage::Destination})
        .FromPixels(pixels)
        .Build();

    ShaderPipelineTemplate* ssaoTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
        "../assets/shaders/processed/render-graph/common/fullscreen-vert.shader",
        "../assets/shaders/processed/render-graph/ao/ssao-ps-frag.shader"},
        "Pass.SSAO.PS", renderGraph.GetArenaAllocators());

    m_PipelineData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(ssaoTemplate)
        .SetRenderingDetails({
            .ColorFormats = {Format::R8_UNORM}})
        .UseDescriptorBuffer()
        .Build();

    m_PipelineData.SamplerDescriptors = ShaderDescriptors::Builder()
        .SetTemplate(ssaoTemplate, DescriptorAllocatorKind::Samplers)
        .ExtractSet(0)
        .Build();

    m_PipelineData.ResourceDescriptors = ShaderDescriptors::Builder()
        .SetTemplate(ssaoTemplate, DescriptorAllocatorKind::Resources)
        .ExtractSet(1)
        .Build();
}

void SsaoPSPass::AddToGraph(RenderGraph::Graph& renderGraph, RenderGraph::Resource depthIn)
{
    using namespace RenderGraph;
    using enum ResourceAccessFlags;

    std::string name = "SSAO.PS";
    m_Pass = &renderGraph.AddRenderPass<PassData>(PassName{name},
        [&](Graph& graph, PassData& passData)
        {
            const TextureDescription& depthDescription = Resources(graph).GetTextureDescription(depthIn);
            passData.NoiseTexture = graph.AddExternal(name + ".NoiseTexture", m_NoiseTexture);
            passData.SettingsUbo = graph.CreateResource(name + ".Settings", GraphBufferDescription{
                .SizeBytes = sizeof(SettingsUBO)});
            passData.CameraUbo = graph.CreateResource(name + ".Camera", GraphBufferDescription{
                .SizeBytes = sizeof(CameraUBO)});
            passData.SSAO = graph.CreateResource(name + ".SSAO", GraphTextureDescription{
                .Width = depthDescription.Width,
                .Height = depthDescription.Height,
                .Format = Format::R8_UNORM});

            passData.DepthIn = graph.Read(depthIn, Pixel | Sampled);
            passData.NoiseTexture = graph.Read(passData.NoiseTexture, Pixel | Sampled);
            passData.SettingsUbo = graph.Read(passData.SettingsUbo, Pixel | Uniform | Upload);
            passData.CameraUbo = graph.Read(passData.CameraUbo, Pixel | Uniform | Upload);
            passData.SSAO = graph.RenderTarget(passData.SSAO, AttachmentLoad::Clear, AttachmentStore::Store,
                glm::vec4{0.0f, 0.0f, 0.0f, 1.0f});

            passData.PipelineData = &m_PipelineData;

            passData.Settings = &m_Settings;

            graph.GetBlackboard().UpdateOutput(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            GPU_PROFILE_FRAME("SSAO.PS")

            auto& settings = *passData.Settings;
            ImGui::Begin("AO settings");
            ImGui::DragFloat("TotalStrength", &settings.TotalStrength, 1e-3f, 0.0f, 1.0f);
            ImGui::DragFloat("Base", &settings.Base, 1e-3f, 0.0f, 1.0f);
            ImGui::DragFloat("Area", &settings.Area, 1e-5f, 0.0f, 1.0f);
            ImGui::DragFloat("Falloff", &settings.Falloff, 1e-7f, 0.0f, 1.0f, "%.3e");
            ImGui::DragFloat("Radius", &settings.Radius, 1e-6f, 0.0f, 1.0f, "%.3e");
            ImGui::End();
            const Buffer& settingUbo = resources.GetBuffer(passData.SettingsUbo, settings,
                *frameContext.ResourceUploader);

            CameraUBO camera = {
                .ProjectionInverse = glm::inverse(frameContext.MainCamera->GetProjection())};
            const Buffer& cameraUbo = resources.GetBuffer(passData.CameraUbo, camera,
                *frameContext.ResourceUploader);

            const Texture& depthTexture = resources.GetTexture(passData.DepthIn);
            const Texture& noiseTexture = resources.GetTexture(passData.NoiseTexture);
            
            auto& pipeline = passData.PipelineData->Pipeline;    
            auto& samplerDescriptors = passData.PipelineData->SamplerDescriptors;    
            auto& resourceDescriptors = passData.PipelineData->ResourceDescriptors;    

            resourceDescriptors.UpdateBinding("u_settings", settingUbo.CreateBindingInfo());
            resourceDescriptors.UpdateBinding("u_camera", cameraUbo.CreateBindingInfo());
            resourceDescriptors.UpdateBinding("u_depth_texture", depthTexture.CreateBindingInfo(
                ImageFilter::Linear, ImageLayout::DepthReadonly));
            resourceDescriptors.UpdateBinding("u_noise_texture", noiseTexture.CreateBindingInfo(
                ImageFilter::Nearest, ImageLayout::Readonly));

            auto& cmd = frameContext.Cmd;
            samplerDescriptors.BindGraphicsImmutableSamplers(cmd, pipeline.GetLayout());
            pipeline.BindGraphics(cmd);
            resourceDescriptors.BindGraphics(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());

            RenderCommand::Draw(cmd, 3);
        });
}
