#include "PbrVisibilityBuffer.h"

#include "FrameContext.h"
#include "Core/Camera.h"
#include "RenderGraph/RenderPassGeometry.h"
#include "Vulkan/RenderCommand.h"

PbrVisibilityBuffer::PbrVisibilityBuffer(RenderGraph::Graph& renderGraph, const PbrVisibilityBufferInitInfo& info)
{
    ShaderPipelineTemplate* pbrTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
          "../assets/shaders/processed/render-graph/common/fullscreen-vert.shader",
          "../assets/shaders/processed/render-graph/pbr/pbr-visibility-buffer-frag.shader"},
      "Pass.PBR.Visibility", renderGraph.GetArenaAllocators());

    m_PipelineData.Pipeline = ShaderPipeline::Builder()
       .SetTemplate(pbrTemplate)
       .SetRenderingDetails({
           .ColorFormats = {Format::RGBA16_FLOAT}})
       .UseDescriptorBuffer()
       .Build();

    m_PipelineData.ImmutableSamplerDescriptors = ShaderDescriptors::Builder()
        .SetTemplate(pbrTemplate, DescriptorAllocatorKind::Samplers)
        .ExtractSet(0)
        .Build();

    m_PipelineData.ResourceDescriptors = ShaderDescriptors::Builder()
        .SetTemplate(pbrTemplate, DescriptorAllocatorKind::Resources)
        .ExtractSet(1)
        .Build();
    
    m_PipelineData.MaterialDescriptors = *info.MaterialDescriptors;
}

void PbrVisibilityBuffer::AddToGraph(RenderGraph::Graph& renderGraph, const PbrVisibilityBufferExecutionInfo& info)
{
    using namespace RenderGraph;
    using enum ResourceAccessFlags;

    std::string name = "Pbr.VisibilityBuffer";
    m_Pass = &renderGraph.AddRenderPass<PassData>(PassName{name},
        [&](Graph& graph, PassData& passData)
        {
            passData.CameraUbo = graph.CreateResource(name + ".Camera", GraphBufferDescription{
                .SizeBytes = sizeof(CameraUBO)});
            passData.CommandsSsbo = graph.AddExternal(name + ".Commands", info.Geometry->GetCommandsBuffer());
            passData.ObjectsSsbo = graph.AddExternal(name + ".Objects", info.Geometry->GetRenderObjectsBuffer());
            auto& attributes = info.Geometry->GetAttributeBuffers();
            passData.PositionsSsbo = graph.AddExternal(name + ".Positions", attributes.Positions);
            passData.NormalsSsbo = graph.AddExternal(name + ".Normals", attributes.Normals);
            passData.TangentsSsbo = graph.AddExternal(name + ".Tangents", attributes.Tangents);
            passData.UVsSsbo = graph.AddExternal(name + ".UVs", attributes.UVs);
            passData.IndicesSsbo = graph.AddExternal(name + ".Indices", attributes.Indices);

            const TextureDescription& visibilityDescription =
                Resources(graph).GetTextureDescription(info.VisibilityTexture);
            Resource color = info.ColorIn.IsValid() ? info.ColorIn :
                graph.CreateResource(name + ".Color", GraphTextureDescription{
                    .Width = visibilityDescription.Width,
                    .Height = visibilityDescription.Height,
                    .Format = Format::RGBA16_FLOAT});

            passData.VisibilityTexture = graph.Read(info.VisibilityTexture, Pixel | Sampled);
            passData.CameraUbo = graph.Read(passData.CameraUbo, Pixel | Uniform | Upload);
            passData.CommandsSsbo = graph.Read(passData.CommandsSsbo, Pixel | Storage);
            passData.ObjectsSsbo = graph.Read(passData.ObjectsSsbo, Pixel | Storage);
            passData.PositionsSsbo = graph.Read(passData.PositionsSsbo, Pixel | Storage);
            passData.NormalsSsbo = graph.Read(passData.NormalsSsbo, Pixel | Storage);
            passData.TangentsSsbo = graph.Read(passData.TangentsSsbo, Pixel | Storage);
            passData.UVsSsbo = graph.Read(passData.UVsSsbo, Pixel | Storage);
            passData.IndicesSsbo = graph.Read(passData.IndicesSsbo, Pixel | Storage);

            passData.ColorOut = graph.RenderTarget(color,
                info.ColorIn.IsValid() ? AttachmentLoad::Load : AttachmentLoad::Clear,
                AttachmentStore::Store);

            passData.PipelineData = &m_PipelineData;
            
            graph.GetBlackboard().RegisterOutput(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            GPU_PROFILE_FRAME("PBR Visibility pass")

            const Texture& visibility = resources.GetTexture(passData.VisibilityTexture);

            CameraUBO camera = {
                .View = frameContext.MainCamera->GetView(),
                .Projection = frameContext.MainCamera->GetProjection(),
                .ViewProjection = frameContext.MainCamera->GetViewProjection(),
                .ViewProjectionInverse = glm::inverse(frameContext.MainCamera->GetViewProjection()),
                .CameraPosition = glm::vec4{frameContext.MainCamera->GetPosition(), 1.0f},
                .Resolution = frameContext.Resolution,
                .FrustumNear = frameContext.MainCamera->GetFrustumPlanes().Near,
                .FrustumFar = frameContext.MainCamera->GetFrustumPlanes().Far};
            const Buffer& cameraUbo = resources.GetBuffer(passData.CameraUbo, camera,
                *frameContext.ResourceUploader);

            const Buffer& commandsSsbo = resources.GetBuffer(passData.CommandsSsbo);
            const Buffer& objectsSsbo = resources.GetBuffer(passData.ObjectsSsbo);
            const Buffer& positionsSsbo = resources.GetBuffer(passData.PositionsSsbo);
            const Buffer& normalsSsbo = resources.GetBuffer(passData.NormalsSsbo);
            const Buffer& tangentsSsbo = resources.GetBuffer(passData.TangentsSsbo);
            const Buffer& uvsSsbo = resources.GetBuffer(passData.UVsSsbo);
            const Buffer& indicesSsbo = resources.GetBuffer(passData.IndicesSsbo);

            auto& pipeline = passData.PipelineData->Pipeline;
            auto& samplerDescriptors = passData.PipelineData->ImmutableSamplerDescriptors;
            auto& resourceDescriptors = passData.PipelineData->ResourceDescriptors;
            auto& materialDescriptors = passData.PipelineData->MaterialDescriptors;

            resourceDescriptors.UpdateBinding("u_visibility_texture", visibility.CreateBindingInfo(ImageFilter::Nearest,
                ImageLayout::ReadOnly));
            resourceDescriptors.UpdateBinding("u_camera", cameraUbo.CreateBindingInfo());
            resourceDescriptors.UpdateBinding("u_commands", commandsSsbo.CreateBindingInfo());
            resourceDescriptors.UpdateBinding("u_objects", objectsSsbo.CreateBindingInfo());
            resourceDescriptors.UpdateBinding("u_positions", positionsSsbo.CreateBindingInfo());
            resourceDescriptors.UpdateBinding("u_normals", normalsSsbo.CreateBindingInfo());
            resourceDescriptors.UpdateBinding("u_tangents", tangentsSsbo.CreateBindingInfo());
            resourceDescriptors.UpdateBinding("u_uv", uvsSsbo.CreateBindingInfo());
            resourceDescriptors.UpdateBinding("u_indices", indicesSsbo.CreateBindingInfo());
            
            auto& cmd = frameContext.Cmd;
            samplerDescriptors.BindGraphicsImmutableSamplers(cmd, pipeline.GetLayout());
            pipeline.BindGraphics(cmd);
            resourceDescriptors.BindGraphics(cmd, resources.GetGraph()->GetArenaAllocators(),
                pipeline.GetLayout());
            materialDescriptors.BindGraphics(cmd, resources.GetGraph()->GetArenaAllocators(),
                pipeline.GetLayout());

            RenderCommand::Draw(cmd, 3);
        });
}
