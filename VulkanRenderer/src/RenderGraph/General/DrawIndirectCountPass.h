#pragma once

#include <glm/glm.hpp>

#include "FrameContext.h"
#include "Core/Camera.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RenderPassCommon.h"
#include "RenderGraph/RenderPassGeometry.h"
#include "Vulkan/RenderCommand.h"

template <typename Specialization>
class DrawIndirectCountPass
{
public:
    struct CameraUBO
    {
        glm::mat4 ViewProjection;
    };
    struct PassData
    {
        RenderGraph::Resource CameraUbo;
        RenderGraph::Resource ObjectsSsbo;
        RenderGraph::Resource CommandsIndirect;
        RenderGraph::Resource CountIndirect;
        RenderGraph::Resource ColorOut;
        RenderGraph::Resource DepthOut;

        RenderGraph::PipelineData* PipelineData{nullptr};
    };
public:
    DrawIndirectCountPass(RenderGraph::Graph& renderGraph, const std::string& name);
    void AddToGraph(RenderGraph::Graph& renderGraph, RenderGraph::Resource color, RenderGraph::Resource depth,
        const glm::uvec2& resolution, bool clearDepth,
        RenderGraph::Resource commands, RenderGraph::Resource count, const RenderPassGeometry& geometry);
private:
    RenderGraph::Pass* m_Pass{nullptr};
    std::string m_Name;
    
    RenderGraph::PipelineData m_PipelineData;
};

template <typename Specialization>
DrawIndirectCountPass<Specialization>::DrawIndirectCountPass(RenderGraph::Graph& renderGraph, const std::string& name)
    : m_Name(name)
{
    ShaderPipelineTemplate* drawIndirectCountTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
          "../assets/shaders/processed/render-graph/general/draw-indirect-count-vert.shader",
          "../assets/shaders/processed/render-graph/general/draw-indirect-count-frag.shader"},
      "render-graph-draw-ic-pass-template", renderGraph.GetArenaAllocators());

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

template <typename Specialization>
void DrawIndirectCountPass<Specialization>::AddToGraph(RenderGraph::Graph& renderGraph,
    RenderGraph::Resource color, RenderGraph::Resource depth, const glm::uvec2& resolution, bool clearDepth,
    RenderGraph::Resource commands, RenderGraph::Resource count, const RenderPassGeometry& geometry)
{
    using namespace RenderGraph;
    using enum ResourceAccessFlags;

    static ShaderDescriptors::BindingInfo cameraBinding = m_PipelineData.ResourceDescriptors.GetBindingInfo("u_camera");
    static ShaderDescriptors::BindingInfo objectsBinding =
        m_PipelineData.ResourceDescriptors.GetBindingInfo("u_objects");
    static ShaderDescriptors::BindingInfo commandsBinding =
        m_PipelineData.ResourceDescriptors.GetBindingInfo("u_commands");

    m_Pass = &renderGraph.AddRenderPass<PassData>(m_Name,
        [&](Graph& graph, PassData& passData)
        {
            passData.CameraUbo = graph.CreateResource(m_Name + "-camera",
                GraphBufferDescription{.SizeBytes = sizeof(CameraUBO)});
            passData.CameraUbo = graph.Read(passData.CameraUbo, Vertex | Uniform | Upload);
            passData.ObjectsSsbo = graph.AddExternal(m_Name + "-objects",
                geometry.GetRenderObjectsBuffer());
            passData.CommandsIndirect = graph.Read(commands, Vertex | Indirect);
            passData.CountIndirect = graph.Read(count, Vertex | Indirect);

            if (color.IsValid())
            {
                passData.ColorOut = graph.RenderTarget(color, AttachmentLoad::Load, AttachmentStore::Store);
            }
            else
            {
                passData.ColorOut = graph.CreateResource(m_Name + "-color", GraphTextureDescription{
                    .Width = resolution.x,
                    .Height = resolution.y,
                    .Format = Format::RGBA16_FLOAT});
                passData.ColorOut = graph.RenderTarget(passData.ColorOut, AttachmentLoad::Clear, AttachmentStore::Store,
                    glm::vec4{0.01f, 0.01f, 0.01f, 1.0f});
            }
            passData.DepthOut = graph.DepthStencilTarget(depth,
                clearDepth ? AttachmentLoad::Clear : AttachmentLoad::Load, AttachmentStore::Store, 0.0f);
            
            passData.PipelineData = &m_PipelineData;

            graph.GetBlackboard().UpdateOutput(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            GPU_PROFILE_FRAME("Draw indirect count")

            CameraUBO camera = {};
            camera.ViewProjection = frameContext.Camera->GetViewProjection();
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
