#include "SsaoPass.h"

#include "FrameContext.h"
#include "Core/Camera.h"
#include "Core/Random.h"
#include "imgui/imgui.h"
#include "utils/MathUtils.h"
#include "Vulkan/RenderCommand.h"

SsaoPass::SsaoPass(RenderGraph::Graph& renderGraph, u32 sampleCount)
    : m_SampleCount(sampleCount)
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

    // generate samples
    std::vector<glm::vec4> samples(m_SampleCount);
    for (u32 i = 0; i < samples.size(); i++)
    {
        glm::vec3 sample = glm::vec3{
            Random::Float(-1.0f, 1.0f),
            Random::Float(-1.0f, 1.0f),
            Random::Float(0.0f, 1.0f)};
        sample = glm::normalize(sample);
        sample *= Random::Float();
        f32 scale = (f32)i / samples.size();
        scale = utils::lerp(0.1f, 1.0f, scale * scale);
        sample *= scale;
        samples[i] = glm::vec4{sample, 1.0f};
    }

    m_SamplesBuffer = Buffer::Builder()
        .SetUsage(BufferUsage::Uniform | BufferUsage::DeviceAddress | BufferUsage::Upload)
        .SetSizeBytes(samples.size() * sizeof(glm::vec4))
        .Build();
    m_SamplesBuffer.SetData(samples.data(), m_SamplesBuffer.GetSizeBytes());

    ShaderPipelineTemplate* ssaoTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
        "../assets/shaders/processed/render-graph/ao/ssao-comp.shader"},
        "Pass.SSAO", renderGraph.GetArenaAllocators());

    m_PipelineData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(ssaoTemplate)
        .AddSpecialization("MAX_SAMPLES", MAX_SAMPLES_COUNT)
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

    m_Settings.Samples = m_SampleCount;
}

void SsaoPass::AddToGraph(RenderGraph::Graph& renderGraph, RenderGraph::Resource depthIn)
{
    using namespace RenderGraph;
    using enum ResourceAccessFlags;

    std::string name = "SSAO";
    m_Pass = &renderGraph.AddRenderPass<PassData>(PassName{name},
        [&](Graph& graph, PassData& passData)
        {
            const TextureDescription& depthDescription = Resources(graph).GetTextureDescription(depthIn);
            passData.NoiseTexture = graph.AddExternal(name + ".NoiseTexture", m_NoiseTexture);
            passData.SettingsUbo = graph.CreateResource(name + ".Settings", GraphBufferDescription{
                .SizeBytes = sizeof(SettingsUBO)});
            passData.CameraUbo = graph.CreateResource(name + ".Camera", GraphBufferDescription{
                .SizeBytes = sizeof(CameraUBO)});
            passData.SamplesUbo = graph.AddExternal(name + ".Samples", m_SamplesBuffer);
            passData.SSAO = graph.CreateResource(name + ".SSAO", GraphTextureDescription{
                .Width = depthDescription.Width,
                .Height = depthDescription.Height,
                .Format = Format::R8_UNORM});

            passData.DepthIn = graph.Read(depthIn, Compute | Sampled);
            passData.NoiseTexture = graph.Read(passData.NoiseTexture, Compute | Sampled);
            passData.SettingsUbo = graph.Read(passData.SettingsUbo, Compute | Uniform | Upload);
            passData.CameraUbo = graph.Read(passData.CameraUbo, Compute | Uniform | Upload);
            passData.SamplesUbo = graph.Read(passData.SamplesUbo, Compute | Uniform);
            passData.SSAO = graph.Write(passData.SSAO, Compute | Storage);

            passData.PipelineData = &m_PipelineData;

            passData.Settings = &m_Settings;
            passData.SampleCount = m_SampleCount;

            graph.GetBlackboard().UpdateOutput(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            GPU_PROFILE_FRAME("SSAO")

            auto& settings = *passData.Settings;
            ImGui::Begin("AO settings");
            ImGui::DragInt("Samples", (i32*)&settings.Samples, 0.25f, 0, passData.SampleCount);
            ImGui::DragFloat("Power", &settings.Power, 1e-3f, 0.0f, 5.0f);
            ImGui::DragFloat("Radius", &settings.Radius, 1e-3f, 0.0f, 1.0f);
            ImGui::End();
            const Buffer& settingUbo = resources.GetBuffer(passData.SettingsUbo, settings,
                *frameContext.ResourceUploader);

            CameraUBO camera = {
                .Projection = frameContext.MainCamera->GetProjection(),
                .ProjectionInverse = glm::inverse(frameContext.MainCamera->GetProjection()),
                .Near = frameContext.MainCamera->GetFrustumPlanes().Near,
                .Far = frameContext.MainCamera->GetFrustumPlanes().Far};
            const Buffer& cameraUbo = resources.GetBuffer(passData.CameraUbo, camera,
                *frameContext.ResourceUploader);

            const Buffer& samplesUbo = resources.GetBuffer(passData.SamplesUbo);

            const Texture& depthTexture = resources.GetTexture(passData.DepthIn);
            const Texture& noiseTexture = resources.GetTexture(passData.NoiseTexture);
            const Texture& ssaoTexture = resources.GetTexture(passData.SSAO);
            
            auto& pipeline = passData.PipelineData->Pipeline;    
            auto& samplerDescriptors = passData.PipelineData->SamplerDescriptors;    
            auto& resourceDescriptors = passData.PipelineData->ResourceDescriptors;    

            resourceDescriptors.UpdateBinding("u_settings", settingUbo.CreateBindingInfo());
            resourceDescriptors.UpdateBinding("u_camera", cameraUbo.CreateBindingInfo());
            resourceDescriptors.UpdateBinding("u_samples", samplesUbo.CreateBindingInfo());
            resourceDescriptors.UpdateBinding("u_depth_texture", depthTexture.CreateBindingInfo(
                ImageFilter::Linear, ImageLayout::DepthReadonly));
            resourceDescriptors.UpdateBinding("u_noise_texture", noiseTexture.CreateBindingInfo(
                ImageFilter::Nearest, ImageLayout::Readonly));
            resourceDescriptors.UpdateBinding("u_ssao", ssaoTexture.CreateBindingInfo(
                ImageFilter::Linear, ImageLayout::General));

            struct PushConstants
            {
                glm::vec2 SsaoSizeInverse;
                glm::vec2 SsaoSize;
                glm::vec2 NoiseSizeInverse;
            };
            PushConstants pushConstants = {
                .SsaoSizeInverse = 1.0f / glm::vec2(
                    (f32)ssaoTexture.GetDescription().Width, (f32)ssaoTexture.GetDescription().Height),
                .SsaoSize = glm::vec2(
                    (f32)ssaoTexture.GetDescription().Width, (f32)ssaoTexture.GetDescription().Height),
                .NoiseSizeInverse = 1.0f / glm::vec2(
                    (f32)noiseTexture.GetDescription().Width, (f32)noiseTexture.GetDescription().Height)};
            
            auto& cmd = frameContext.Cmd;
            samplerDescriptors.BindComputeImmutableSamplers(cmd, pipeline.GetLayout());
            pipeline.BindCompute(cmd);
            RenderCommand::PushConstants(cmd, pipeline.GetLayout(), pushConstants);
            resourceDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());

            RenderCommand::Dispatch(cmd,
                {ssaoTexture.GetDescription().Width, ssaoTexture.GetDescription().Height, 1},
                {8, 8, 1});
        });
}
