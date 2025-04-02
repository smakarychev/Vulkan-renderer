#include "DrawIndirectCountPass.h"

#include "CameraGPU.h"
#include "FrameContext.h"
#include "RenderGraph/RGUtils.h"
#include "Rendering/Shader/ShaderCache.h"
#include "Scene/SceneGeometry.h"

RG::Pass& Passes::Draw::IndirectCount::addToGraph(StringId name, RG::Graph& renderGraph,
    const DrawIndirectCountPassExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    struct PassDataPrivate
    {
        Resource Camera{};
        DrawAttributeBuffers AttributeBuffers{};
        Resource Objects{};
        Resource Commands{};
        Resource Count{};
        
        DrawAttachmentResources DrawAttachmentResources{};
    };
    
    Pass& pass = renderGraph.AddRenderPass<PassDataPrivate>(name,
        [&](Graph& graph, PassDataPrivate& passData)
        {
            CPU_PROFILE_FRAME("Draw.Indirect.Count.Setup")

            passData.Camera = graph.CreateResource(
                "Camera"_hsv, GraphBufferDescription{.SizeBytes = sizeof(CameraGPU)});
            passData.Camera = graph.Read(passData.Camera, Vertex | Pixel | Uniform);
            CameraGPU cameraGPU = CameraGPU::FromCamera(*info.Camera, info.Resolution);
            graph.Upload(passData.Camera, cameraGPU);

            passData.AttributeBuffers = RgUtils::readDrawAttributes(*info.Geometry, graph, Vertex);

            passData.Objects = graph.AddExternal("Objects"_hsv,
                info.Geometry->GetRenderObjectsBuffer());
            passData.Commands = graph.Read(info.Commands, Vertex | Indirect);
            passData.Count = graph.Read(info.CommandCount, Vertex | Indirect);

            passData.DrawAttachmentResources = RgUtils::readWriteDrawAttachments(info.DrawInfo.Attachments, graph);

            info.DrawInfo.DrawSetup(graph);

            PassData passDataPublic = {};
            passDataPublic.DrawAttachmentResources  = passData.DrawAttachmentResources;
            
            graph.UpdateBlackboard(passDataPublic);
        },
        [=](PassDataPrivate& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Draw.Indirect.Count")
            GPU_PROFILE_FRAME("Draw.Indirect.Count")

            Buffer commandsDraw = resources.GetBuffer(passData.Commands);
            Buffer countDraw = resources.GetBuffer(passData.Count);
            auto& cmd = frameContext.CommandList;
            
            const Shader& shader = info.DrawInfo.DrawBind(cmd, resources, {
                .Camera = passData.Camera,
                .Objects = passData.Objects,
                .Commands = passData.Commands,
                .DrawAttributes = passData.AttributeBuffers});

            cmd.BindIndexU8Buffer({
                .Buffer = info.Geometry->GetAttributeBuffers().Indices});
            u32 offsetCommands = std::min(info.CommandsOffset, info.Geometry->GetMeshletCount());
            u32 toDrawCommands = info.Geometry->GetMeshletCount() - offsetCommands;
            cmd.PushConstants({
                .PipelineLayout = shader.GetLayout(), 
                .Data = {offsetCommands}});
            cmd.DrawIndexedIndirectCount({
                .DrawBuffer = commandsDraw, .DrawOffset = offsetCommands * sizeof(IndirectDrawCommand),
                .CountBuffer = countDraw, .CountOffset = info.CountOffset * sizeof(u32),
                .MaxCount = toDrawCommands});
        });

    return pass;
}
