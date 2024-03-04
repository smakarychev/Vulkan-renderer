#include "TestPass.h"

#include "Renderer.h"
#include "Core/Random.h"
#include "RenderGraph/RenderGraphBlackboard.h"
#include "Vulkan/RenderCommand.h"


TestPass::TestPass(RenderGraph::Graph& renderGraph, const Texture& colorTarget)
{
    using namespace RenderGraph;

    ShaderPipelineTemplate* testPassPipelineTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate(
        {
            "../assets/shaders/processed/render-graph/test-vert.shader",
            "../assets/shaders/processed/render-graph/test-frag.shader"},
        "render-graph-test-pass-template", renderGraph.GetDescriptorResourceAllocator());

    ShaderPipelineTemplate* testPassPostPipelineTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate(
       {
           "../assets/shaders/processed/render-graph/test-vert.shader",
           "../assets/shaders/processed/render-graph/test-post-frag.shader"},
       "render-graph-test-pass-post-template", renderGraph.GetDescriptorSamplerAllocator());

    ShaderPipeline testPipeline = ShaderPipeline::Builder()
        .SetTemplate(testPassPipelineTemplate)
        .SetRenderingDetails({
            .ColorFormats = {Format::RGBA16_FLOAT}})
        .UseDescriptorBuffer()
        .Build();

    ShaderPipeline testPostPipeline = ShaderPipeline::Builder()
        .SetTemplate(testPassPostPipelineTemplate)
        .SetRenderingDetails({
            .ColorFormats = {Format::RGBA16_FLOAT}})
        .UseDescriptorBuffer()
        .Build();


    DescriptorSetBinding uboBinding = {
        .Binding = 0,
        .Type = DescriptorType::UniformBuffer,
        .Count = 1,
        .Shaders = ShaderStage::Pixel,
        .HasImmutableSampler = false};
    
    Descriptors testDescriptors = renderGraph.GetDescriptorResourceAllocator().Allocate(
        testPassPipelineTemplate->GetDescriptorsLayout(0),
        {
            .Bindings = {uboBinding},
            .Flags = {DescriptorFlags::None}});

    DescriptorSetBinding samplerBinding = {
        .Binding = 0,
        .Type = DescriptorType::Sampler,
        .Count = 1,
        .Shaders = ShaderStage::Pixel,
        .HasImmutableSampler = false};
    
    DescriptorSetBinding textureBinding = {
        .Binding = 0,
        .Type = DescriptorType::Image,
        .Count = 1,
        .Shaders = ShaderStage::Pixel,
        .HasImmutableSampler = false};
    
    DescriptorSetBinding timeBinding = {
        .Binding = 1,
        .Type = DescriptorType::UniformBuffer,
        .Count = 1,
        .Shaders = ShaderStage::Pixel,
        .HasImmutableSampler = false};

    Descriptors testPostSamplerDescriptors = renderGraph.GetDescriptorSamplerAllocator().Allocate(
        testPassPostPipelineTemplate->GetDescriptorsLayout(0),
        {
            .Bindings = {samplerBinding},
            .Flags = {DescriptorFlags::None}});

    Descriptors testPostResourceDescriptors = renderGraph.GetDescriptorResourceAllocator().Allocate(
        testPassPostPipelineTemplate->GetDescriptorsLayout(1),
        {
            .Bindings = {textureBinding, timeBinding},
            .Flags = {DescriptorFlags::None, DescriptorFlags::None}});
    
    Blackboard blackboard = {};
    
    u64 tick = 0;

    m_TestPass = &renderGraph.AddRenderPass<TestPassData>("test-pass",
        [&](Graph& graph, TestPassData& passData)
        {
            passData.ColorUbo = graph.CreateResource("test-color-ubo", GraphBufferDescription{
                .SizeBytes = sizeof(glm::vec3)});
            passData.ColorUbo = graph.Read(passData.ColorUbo,
                ResourceAccessFlags::Pixel | ResourceAccessFlags::Uniform);

            passData.ColorTarget = graph.CreateResource("test-color-target", GraphTextureDescription{
                .Width = colorTarget.GetDescription().Width,
                .Height = colorTarget.GetDescription().Height,
                .Format = colorTarget.GetDescription().Format,
                .Kind = colorTarget.GetDescription().Kind,
                .MipmapFilter = colorTarget.GetDescription().MipmapFilter});
            passData.ColorTarget = graph.RenderTarget(passData.ColorTarget,
                AttachmentLoad::Clear, AttachmentStore::Store, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

            blackboard.RegisterOutput(passData);
        },
        [=](TestPassData& passData, FrameContext& frameContext, const Resources& resources) mutable
        {
            TestPassData data = {
                .Color = glm::vec3{
                    f32(glm::sin(glm::radians((f64)tick) / 100 * 3.0) + 1.0 * 0.5),
                    f32(glm::sin(glm::radians((f64)tick / 100 / 3.0)) + 1.0 * 0.5),
                    f32(glm::cos(glm::radians((f64)tick / 100 / 2.0)) + 1.0 * 0.5),
                }};
            tick++;

            const Buffer& colorUbo = resources.GetBuffer(passData.ColorUbo, data.Color, *frameContext.ResourceUploader);
            
            testDescriptors.UpdateBinding(0, colorUbo.CreateSubresource(), DescriptorType::UniformBuffer);
            
            testPipeline.BindGraphics(frameContext.Cmd);
            Driver::Bind(frameContext.Cmd, {&resources.GetGraph()->GetDescriptorResourceAllocator()});
            Driver::BindGraphics(frameContext.Cmd, {&resources.GetGraph()->GetDescriptorResourceAllocator()},
                testPipeline.GetPipelineLayout(), testDescriptors, 0);
            
            RenderCommand::Draw(frameContext.Cmd, 3);
        });

    m_TestPassPost = &renderGraph.AddRenderPass<TestPassPostData>("test-pass-post",
        [&](Graph& graph, TestPassPostData& passData)
        {
            TestPassData testPassData = blackboard.GetOutput<TestPassData>();

            passData.ColorIn = graph.Read(testPassData.ColorTarget,
                ResourceAccessFlags::Pixel | ResourceAccessFlags::Sampled);
            
            passData.ColorTarget = graph.AddExternal("test-post-color-target", colorTarget);
            passData.ColorTarget = graph.RenderTarget(passData.ColorTarget,
                AttachmentLoad::Load, AttachmentStore::Store);

            passData.TimeUbo = graph.CreateResource("test-pass-post-time", GraphBufferDescription{
                .SizeBytes = sizeof(f32)});
            passData.TimeUbo = graph.Read(passData.TimeUbo,
                ResourceAccessFlags::Pixel | ResourceAccessFlags::Uniform);
        },
        [=](TestPassPostData& passData, FrameContext& frameContext, const Resources& resources) mutable
        {
            const Texture& colorIn = resources.GetTexture(passData.ColorIn);
            const Buffer& time = resources.GetBuffer(passData.TimeUbo, (f32)tick, *frameContext.ResourceUploader);
            tick++;

            testPostSamplerDescriptors.UpdateBinding(0, colorIn.CreateBindingInfo(
                ImageFilter::Linear, ImageLayout::ReadOnly),
                DescriptorType::Sampler);
            testPostResourceDescriptors.UpdateBinding(0, colorIn.CreateBindingInfo(
                ImageFilter::Linear, ImageLayout::ReadOnly),
                DescriptorType::Image);
            testPostResourceDescriptors.UpdateBinding(1, time.CreateSubresource(), DescriptorType::UniformBuffer);
            
            testPostPipeline.BindGraphics(frameContext.Cmd);
            Driver::Bind(frameContext.Cmd, {
                &resources.GetGraph()->GetDescriptorSamplerAllocator(),
                &resources.GetGraph()->GetDescriptorResourceAllocator()});
            Driver::BindGraphics(frameContext.Cmd, {
                &resources.GetGraph()->GetDescriptorSamplerAllocator(),
                &resources.GetGraph()->GetDescriptorResourceAllocator()},
                testPostPipeline.GetPipelineLayout(), testPostSamplerDescriptors, 0);
            Driver::BindGraphics(frameContext.Cmd, {
                &resources.GetGraph()->GetDescriptorSamplerAllocator(),
                &resources.GetGraph()->GetDescriptorResourceAllocator()},
                testPostPipeline.GetPipelineLayout(), testPostResourceDescriptors, 1);

            RenderCommand::Draw(frameContext.Cmd, 3);
        });
}
