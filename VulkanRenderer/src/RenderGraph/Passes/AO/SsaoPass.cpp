#include "rendererpch.h"

#include "SsaoPass.h"

#include "FrameContext.h"
#include "Core/Camera.h"
#include "imgui/imgui.h"
#include "Rendering/Shader/ShaderCache.h"
#include "Math/Random.h"
#include "Math/CoreMath.h"
#include "RenderGraph/RGGraph.h"
#include "RenderGraph/RGCommon.h"
#include "Rendering/Image/ImageUtility.h"

#include "RenderGraph/Passes/Generated/SsaoBindGroup.generated.h"

namespace
{
    constexpr u32 MAX_SAMPLES_COUNT{256};
    
    std::pair<Texture, Buffer> generateSamples(u32 count)
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
            pixel = Images::toRGBA8SNorm(glm::vec4{randomDir, 1.0f});
        }

        Texture noise = Device::CreateImage({
            .DataSource = Span<const std::byte>(pixels),
            .Description = ImageDescription{
                .Width = RANDOM_SIZE,
                .Height = RANDOM_SIZE,
                .Format = Format::RGBA8_SNORM,
                .Usage = ImageUsage::Sampled | ImageUsage::Destination},
            .CalculateMipmaps = false});

        std::vector<glm::vec4> samples(count);
        for (u32 i = 0; i < samples.size(); i++)
        {
            glm::vec3 sample = glm::vec3{
                Random::Float(-1.0f, 1.0f),
                Random::Float(-1.0f, 1.0f),
                Random::Float(0.0f, 1.0f)};
            sample = glm::normalize(sample);
            sample *= Random::Float();
            f32 scale = (f32)i / (f32)samples.size();
            scale = Math::lerp(0.1f, 1.0f, scale * scale);
            sample *= scale;
            samples[i] = glm::vec4{sample, 1.0f};
        }

        Buffer samplesBuffer = Device::CreateBuffer({
            .SizeBytes = samples.size() * sizeof(glm::vec4),
            .Usage = BufferUsage::Ordinary | BufferUsage::Mappable | BufferUsage::Uniform,
            .InitialData = samples});

        return {noise, samplesBuffer};
    }
}

Passes::Ssao::PassData& Passes::Ssao::addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    struct SettingsUBO
    {
        f32 Power{1.0f};
        f32 Radius{1.0f};
        u32 Samples{32};
    };
    struct Samples
    {
        Texture NoiseTexture{};
        Buffer SamplesBuffer{};
    };

    struct PassDataPrivate : PassData
    {
        Resource Depth{};
        Resource NoiseTexture{};
        Resource Settings{};
        Resource ViewInfo{};
        Resource Samples{};
    };

    return renderGraph.AddRenderPass<PassDataPrivate>(name,
        [&](Graph& graph, PassDataPrivate& passData)
        {
            CPU_PROFILE_FRAME("SSAO.Setup")
            
            graph.SetShader("ssao"_hsv,
                ShaderSpecializations{
                    ShaderSpecialization{"MAX_SAMPLES"_hsv, MAX_SAMPLES_COUNT}});
            
            if (!graph.TryGetBlackboardValue<Samples>())
            {
                auto&& [noise, samplesBuffer] = generateSamples(info.MaxSampleCount);
                Samples samples = {.NoiseTexture = noise, .SamplesBuffer = samplesBuffer};
                graph.UpdateBlackboard(samples);
            }
            Samples& samples = graph.GetBlackboardValue<Samples>();
            
            passData.NoiseTexture = graph.Import("NoiseTexture"_hsv, samples.NoiseTexture, ImageLayout::Readonly);
            passData.Settings = graph.Create("Settings"_hsv, RGBufferDescription{
                .SizeBytes = sizeof(SettingsUBO)});
            passData.Samples = graph.Import("Samples"_hsv, samples.SamplesBuffer);
            passData.SSAO = graph.Create("SSAO"_hsv, RGImageDescription{
                .Inference = RGImageInference::Size2d,
                .Reference = info.Depth,
                .Format = Format::R8_UNORM});

            auto& globalResources = graph.GetGlobalResources();
            
            passData.Depth = graph.ReadImage(info.Depth, Compute | Sampled);
            passData.NoiseTexture = graph.ReadImage(passData.NoiseTexture, Compute | Sampled);
            passData.Settings = graph.ReadBuffer(passData.Settings, Compute | Uniform);
            passData.ViewInfo = graph.ReadBuffer(globalResources.PrimaryViewInfoResource, Compute | Uniform);
            passData.Samples = graph.ReadBuffer(passData.Samples, Compute | Uniform);
            passData.SSAO = graph.WriteImage(passData.SSAO, Compute | Storage);

            auto& settings = graph.GetOrCreateBlackboardValue<SettingsUBO>();
            ImGui::Begin("AO settings");
            ImGui::DragInt("Samples", (i32*)&settings.Samples, 0.25f, 0, (i32)info.MaxSampleCount);
            ImGui::DragFloat("Power", &settings.Power, 1e-3f, 0.0f, 5.0f);
            ImGui::DragFloat("Radius", &settings.Radius, 1e-3f, 0.0f, 1.0f);
            ImGui::End();
            passData.Settings = graph.Upload(passData.Settings, settings);
        },
        [=](const PassDataPrivate& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("SSAO")
            GPU_PROFILE_FRAME("SSAO")

            auto& noiseDescription = graph.GetImageDescription(passData.NoiseTexture);
            auto& ssaoDescription = graph.GetImageDescription(passData.SSAO);
            
            const Shader& shader = graph.GetShader();SsaoShaderBindGroup bindGroup(shader);
            bindGroup.SetSettings(graph.GetBufferBinding(passData.Settings));
            bindGroup.SetViewInfo(graph.GetBufferBinding(passData.ViewInfo));
            bindGroup.SetSamples(graph.GetBufferBinding(passData.Samples));
            bindGroup.SetDepthTexture(graph.GetImageBinding(passData.Depth));
            bindGroup.SetNoiseTexture(graph.GetImageBinding(passData.NoiseTexture));
            bindGroup.SetSsao(graph.GetImageBinding(passData.SSAO));

            struct PushConstants
            {
                glm::vec2 SsaoSizeInverse;
                glm::vec2 SsaoSize;
                glm::vec2 NoiseSizeInverse;
            };
            PushConstants pushConstants = {
                .SsaoSizeInverse = 1.0f / glm::vec2((f32)ssaoDescription.Width, (f32)ssaoDescription.Height),
                .SsaoSize = glm::vec2((f32)ssaoDescription.Width, (f32)ssaoDescription.Height),
                .NoiseSizeInverse = 1.0f / glm::vec2((f32)noiseDescription.Width, (f32)noiseDescription.Height)};
            
            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, graph.GetFrameAllocators());
            cmd.PushConstants({
                .PipelineLayout = shader.GetLayout(),
                .Data = {pushConstants}});
            cmd.Dispatch({
                .Invocations = {ssaoDescription.Width, ssaoDescription.Height, 1},
                .GroupSize = {16, 16, 1}});
        });
}