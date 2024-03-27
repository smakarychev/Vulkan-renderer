#include "DrawIndirectCountPass.h"

#include "FrameContext.h"
#include "RenderGraph/RenderPassGeometry.h"
#include "Vulkan/RenderCommand.h"

DrawIndirectCountPass::DrawIndirectCountPass(RenderGraph::Graph& renderGraph, std::string_view name)
    : m_Name(name)
{
    ShaderPipelineTemplate* drawIndirectCountTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
          "../assets/shaders/processed/render-graph/general/draw-indirect-count-vert.shader",
          "../assets/shaders/processed/render-graph/general/draw-indirect-count-frag.shader"},
      "Pass.DrawIndirectCount", renderGraph.GetArenaAllocators());

    m_PipelineData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(drawIndirectCountTemplate)
        .SetRenderingDetails({
            .ColorFormats = {Format::RGBA16_FLOAT},
            .DepthFormat = Format::D32_FLOAT})
        .CompatibleWithVertex(VertexP3N3T3UV2::GetInputDescriptionDI())
        .UseDescriptorBuffer()
        .Build();

    m_PipelineData.ResourceDescriptors = ShaderDescriptors::Builder()
        .SetTemplate(drawIndirectCountTemplate, DescriptorAllocatorKind::Resources)
        .ExtractSet(1)
        .Build();
}

void DrawIndirectCountPass::AddToGraph(RenderGraph::Graph& renderGraph,
    const RenderPassGeometry& geometry, const DrawIndirectCountPassInitInfo& initInfo)
{
    using namespace RenderGraph;
    using enum ResourceAccessFlags;

    static ShaderDescriptors::BindingInfo cameraBinding =
        m_PipelineData.ResourceDescriptors.GetBindingInfo("u_camera");
    static ShaderDescriptors::BindingInfo objectsBinding =
        m_PipelineData.ResourceDescriptors.GetBindingInfo("u_objects");
    static ShaderDescriptors::BindingInfo commandsBinding =
        m_PipelineData.ResourceDescriptors.GetBindingInfo("u_commands");
    
    m_Pass = &renderGraph.AddRenderPass<PassData>(m_Name,
        [&](Graph& graph, PassData& passData)
        {
            passData.CameraUbo = graph.CreateResource(m_Name.Name() + ".Camera",
                GraphBufferDescription{.SizeBytes = sizeof(CameraUBO)});
            passData.CameraUbo = graph.Read(passData.CameraUbo, Vertex | Uniform | Upload);
            passData.ObjectsSsbo = graph.AddExternal(m_Name.Name() + ".Objects",
                geometry.GetRenderObjectsBuffer());
            passData.CommandsIndirect = graph.Read(initInfo.Commands, Vertex | Indirect);
            passData.CountIndirect = graph.Read(initInfo.CommandCount, Vertex | Indirect);

            if (initInfo.Color.IsValid())
            {
                passData.ColorOut = graph.RenderTarget(initInfo.Color, AttachmentLoad::Load, AttachmentStore::Store);
            }
            else
            {
                passData.ColorOut = graph.CreateResource(m_Name.Name() + ".Color", GraphTextureDescription{
                    .Width = initInfo.Resolution.x,
                    .Height = initInfo.Resolution.y,
                    .Format = Format::RGBA16_FLOAT});
                passData.ColorOut = graph.RenderTarget(passData.ColorOut, AttachmentLoad::Clear, AttachmentStore::Store,
                    glm::vec4{0.01f, 0.01f, 0.01f, 1.0f});
            }
            passData.DepthOut = graph.DepthStencilTarget(initInfo.Depth,
                initInfo.ClearDepth ? AttachmentLoad::Clear : AttachmentLoad::Load, AttachmentStore::Store, 0.0f);
            
            passData.PipelineData = &m_PipelineData;

            graph.GetBlackboard().UpdateOutput(m_Name.Hash(), passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            GPU_PROFILE_FRAME("Draw indirect count")

            CameraUBO camera = {};
            camera.ViewProjection = frameContext.MainCamera->GetViewProjection();
            const Buffer& cameraUbo = resources.GetBuffer(passData.CameraUbo, camera,
                *frameContext.ResourceUploader);
            const Buffer& objectsSsbo = resources.GetBuffer(passData.ObjectsSsbo);
            const Buffer& commandsDraw = resources.GetBuffer(passData.CommandsIndirect);
            const Buffer& countDraw = resources.GetBuffer(passData.CountIndirect);

            auto& pipeline = passData.PipelineData->Pipeline;
            auto& resourceDescriptors = passData.PipelineData->ResourceDescriptors;

            resourceDescriptors.UpdateBinding(cameraBinding, cameraUbo.CreateBindingInfo());
            resourceDescriptors.UpdateBinding(objectsBinding, objectsSsbo.CreateBindingInfo());
            resourceDescriptors.UpdateBinding(commandsBinding, commandsDraw.CreateBindingInfo());

            auto& cmd = frameContext.Cmd;
            RenderCommand::BindIndexU8Buffer(cmd, geometry.GetAttributeBuffers().Indices, 0);
            RenderCommand::BindVertexBuffers(cmd,
                {
                    geometry.GetAttributeBuffers().Positions,
                    geometry.GetAttributeBuffers().Normals,
                    geometry.GetAttributeBuffers().Tangents,
                    geometry.GetAttributeBuffers().UVs},
                {0, 0, 0, 0});
            
            pipeline.BindGraphics(cmd);
            resourceDescriptors.BindGraphics(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());
            RenderCommand::DrawIndexedIndirectCount(cmd,
                commandsDraw, 0,
                countDraw, 0,
                geometry.GetMeshletCount());
        });
}
