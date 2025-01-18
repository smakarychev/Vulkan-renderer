#include "SsaoPass.h"

#include "FrameContext.h"
#include "Core/Camera.h"
#include "Core/Random.h"
#include "imgui/imgui.h"
#include "Rendering/Shader/ShaderCache.h"
#include "utils/MathUtils.h"
#include "Vulkan/RenderCommand.h"

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
            pixel = ImageUtils::toRGBA8SNorm(glm::vec4{randomDir, 1.0f});
        }

        Texture noise = Device::CreateImage({
            .DataSource = Span<const std::byte>(pixels),
            .Description = ImageDescription{
                .Width = RANDOM_SIZE,
                .Height = RANDOM_SIZE,
                .Format = Format::RGBA8_SNORM,
                .Usage = ImageUsage::Sampled | ImageUsage::Destination}});

        // generate samples
        std::vector<glm::vec4> samples(count);
        for (u32 i = 0; i < samples.size(); i++)
        {
            glm::vec3 sample = glm::vec3{
                Random::Float(-1.0f, 1.0f),
                Random::Float(-1.0f, 1.0f),
                Random::Float(0.0f, 1.0f)};
            sample = glm::normalize(sample);
            sample *= Random::Float();
            f32 scale = (f32)i / samples.size();
            scale = MathUtils::lerp(0.1f, 1.0f, scale * scale);
            sample *= scale;
            samples[i] = glm::vec4{sample, 1.0f};
        }

        Buffer samplesBuffer = Device::CreateBuffer({
            .SizeBytes = samples.size() * sizeof(glm::vec4),
            .Usage = BufferUsage::Ordinary | BufferUsage::Mappable | BufferUsage::Uniform,
            .InitialData = samples});
        Device::DeletionQueue().Enqueue(samplesBuffer);

        return {noise, samplesBuffer};
    }
}

RG::Pass& Passes::Ssao::addToGraph(std::string_view name, u32 sampleCount, RG::Graph& renderGraph, RG::Resource depthIn)
{
    struct SettingsUBO
    {
        f32 Power{1.0f};
        f32 Radius{0.075f};
        u32 Samples{32};
    };
    struct CameraUBO
    {
        glm::mat4 Projection{glm::mat4{1.0f}};
        glm::mat4 ProjectionInverse{glm::mat4{1.0f}};
        f32 Near{0.0f};
        f32 Far{1000.0f};
    };
    struct Samples
    {
        Texture NoiseTexture{};
        Buffer SamplesBuffer{};
    };

    using namespace RG;
    using enum ResourceAccessFlags;

    Pass& pass = renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("SSAO.Setup")
            
            graph.SetShader("../assets/shaders/ssao.shader",
                ShaderOverrides{
                    ShaderOverride{{"MAX_SAMPLES"}, MAX_SAMPLES_COUNT}});
            
            if (!graph.TryGetBlackboardValue<Samples>())
            {
                auto&& [noise, samplesBuffer] = generateSamples(sampleCount);
                Samples samples = {.NoiseTexture = noise, .SamplesBuffer = samplesBuffer};
                graph.UpdateBlackboard(samples);
            }
            Samples& samples = graph.GetBlackboardValue<Samples>();
            
            passData.NoiseTexture = graph.AddExternal(std::string{name} + ".NoiseTexture", samples.NoiseTexture);
            passData.Settings = graph.CreateResource(std::string{name} + ".Settings", GraphBufferDescription{
                .SizeBytes = sizeof(SettingsUBO)});
            passData.Camera = graph.CreateResource(std::string{name} + ".Camera", GraphBufferDescription{
                .SizeBytes = sizeof(CameraUBO)});
            passData.Samples = graph.AddExternal(std::string{name} + ".Samples", samples.SamplesBuffer);
            const TextureDescription& depthDescription = Resources(graph).GetTextureDescription(depthIn);
            passData.SSAO = graph.CreateResource(std::string{name} + ".SSAO", GraphTextureDescription{
                .Width = depthDescription.Width,
                .Height = depthDescription.Height,
                .Format = Format::R8_UNORM});

            passData.DepthIn = graph.Read(depthIn, Compute | Sampled);
            passData.NoiseTexture = graph.Read(passData.NoiseTexture, Compute | Sampled);
            passData.Settings = graph.Read(passData.Settings, Compute | Uniform);
            passData.Camera = graph.Read(passData.Camera, Compute | Uniform);
            passData.Samples = graph.Read(passData.Samples, Compute | Uniform);
            passData.SSAO = graph.Write(passData.SSAO, Compute | Storage);

            passData.MaxSampleCount = sampleCount;

            auto& globalResources = graph.GetGlobalResources();
            
            auto& settings = graph.GetOrCreateBlackboardValue<SettingsUBO>();
            ImGui::Begin("AO settings");
            ImGui::DragInt("Samples", (i32*)&settings.Samples, 0.25f, 0, passData.MaxSampleCount);
            ImGui::DragFloat("Power", &settings.Power, 1e-3f, 0.0f, 5.0f);
            ImGui::DragFloat("Radius", &settings.Radius, 1e-3f, 0.0f, 1.0f);
            ImGui::End();
            graph.Upload(passData.Settings, settings);
            
            CameraUBO camera = {
                .Projection = globalResources.PrimaryCamera->GetProjection(),
                .ProjectionInverse = glm::inverse(globalResources.PrimaryCamera->GetProjection()),
                .Near = globalResources.PrimaryCamera->GetFrustumPlanes().Near,
                .Far = globalResources.PrimaryCamera->GetFrustumPlanes().Far};
            graph.Upload(passData.Camera, camera);
            
            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("SSAO")
            GPU_PROFILE_FRAME("SSAO")

            const Buffer& setting = resources.GetBuffer(passData.Settings);
            const Buffer& cameraBuffer = resources.GetBuffer(passData.Camera);
            const Buffer& samples = resources.GetBuffer(passData.Samples);

            const Texture& depthTexture = resources.GetTexture(passData.DepthIn);
            const Texture& noiseTexture = resources.GetTexture(passData.NoiseTexture);
            const Texture& ssaoTexture = resources.GetTexture(passData.SSAO);
            
            const Shader& shader = resources.GetGraph()->GetShader();
            auto pipeline = shader.Pipeline(); 
            auto& samplerDescriptors = shader.Descriptors(ShaderDescriptorsKind::Sampler);
            auto& resourceDescriptors = shader.Descriptors(ShaderDescriptorsKind::Resource);

            resourceDescriptors.UpdateBinding("u_settings", setting.BindingInfo());
            resourceDescriptors.UpdateBinding("u_camera", cameraBuffer.BindingInfo());
            resourceDescriptors.UpdateBinding("u_samples", samples.BindingInfo());
            resourceDescriptors.UpdateBinding("u_depth_texture", depthTexture.BindingInfo(
                ImageFilter::Linear, ImageLayout::DepthReadonly));
            resourceDescriptors.UpdateBinding("u_noise_texture", noiseTexture.BindingInfo(
                ImageFilter::Nearest, ImageLayout::Readonly));
            resourceDescriptors.UpdateBinding("u_ssao", ssaoTexture.BindingInfo(
                ImageFilter::Linear, ImageLayout::General));

            struct PushConstants
            {
                glm::vec2 SsaoSizeInverse;
                glm::vec2 SsaoSize;
                glm::vec2 NoiseSizeInverse;
            };
            PushConstants pushConstants = {
                .SsaoSizeInverse = 1.0f / glm::vec2(
                    (f32)ssaoTexture.Description().Width, (f32)ssaoTexture.Description().Height),
                .SsaoSize = glm::vec2(
                    (f32)ssaoTexture.Description().Width, (f32)ssaoTexture.Description().Height),
                .NoiseSizeInverse = 1.0f / glm::vec2(
                    (f32)noiseTexture.Description().Width, (f32)noiseTexture.Description().Height)};
            
            auto& cmd = frameContext.Cmd;
            samplerDescriptors.BindComputeImmutableSamplers(cmd, shader.GetLayout());
            RenderCommand::BindCompute(cmd, pipeline);
            RenderCommand::PushConstants(cmd, shader.GetLayout(), pushConstants);
            resourceDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), shader.GetLayout());

            RenderCommand::Dispatch(cmd,
                {ssaoTexture.Description().Width, ssaoTexture.Description().Height, 1},
                {16, 16, 1});
        });

    return pass;
}
