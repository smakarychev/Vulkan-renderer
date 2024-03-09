#include "TestPass3d.h"

#include "Model.h"
#include "Renderer.h"
#include "RenderGraph/ModelCollection.h"
#include "Vulkan/RenderCommand.h"

TestPass3d::TestPass3d(RenderGraph::Graph& renderGraph, const Texture& depthTarget,
    ResourceUploader* resourceUploader)
{
    Model* car = Model::LoadFromAsset("../assets/models/car/scene.model");
    m_Collection.RegisterModel(car, "car");
    m_Collection.AddModelInstance("car", {glm::mat4{1.0f}});
    m_Geometry = RenderPassGeometry::FromModelCollectionFiltered(m_Collection, *resourceUploader,
        [](auto& obj) { return true; });
    
    CreatePass(renderGraph, depthTarget, &m_Geometry);
}

void TestPass3d::CreatePass(RenderGraph::Graph& renderGraph, const Texture& depthTarget,
    RenderPassGeometry* geometry)
{
    using namespace RenderGraph;
    
    struct CameraData
    {
        glm::mat4 View;
        glm::mat4 Projection;
        glm::mat4 ViewProjection;
    };

    ShaderPipelineTemplate* testPass3dPipelineTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate(
       {
           "../assets/shaders/processed/render-graph/test3d-vert.shader",
           "../assets/shaders/processed/render-graph/test3d-frag.shader"},
           "render-graph-test-pass-3d-template", renderGraph.GetArenaAllocators());
    
    ShaderPipeline pipeline = ShaderPipeline::Builder()
        .SetTemplate(testPass3dPipelineTemplate)
        .UseDescriptorBuffer()
        .SetRenderingDetails({
            .ColorFormats = {Format::RGBA16_FLOAT},
            .DepthFormat = Format::D32_FLOAT})
        .CompatibleWithVertex(VertexP3N3T3UV2::GetInputDescriptionDI())
        .Build();

    ShaderDescriptors descriptors = ShaderDescriptors::Builder()
        .SetTemplate(testPass3dPipelineTemplate, DescriptorAllocatorKind::Resources)
        .ExtractSet(0)
        .Build();

    ShaderDescriptors::BindingInfo cameraBinding = descriptors.GetBindingInfo("u_camera");
    
    m_Pass = &renderGraph.AddRenderPass<PassData>("test-pass-3d",
        [&](Graph& graph, PassData& passData)
        {
            passData.CameraUbo = graph.CreateResource("pass-test-3d-camera-ubo", GraphBufferDescription{
                .SizeBytes = sizeof(CameraData)});
            passData.CameraUbo = graph.Read(passData.CameraUbo,
                ResourceAccessFlags::Vertex | ResourceAccessFlags::Uniform);
            
            //passData.ColorOut = graph.AddExternal("pass-test-3d-color", colorTarget);
            passData.ColorOut = graph.CreateResource("pass-test-3d-color", GraphTextureDescription{
                .Width = depthTarget.GetDescription().Width,
                .Height = depthTarget.GetDescription().Height,
                .Format = Format::RGBA16_FLOAT,
                .MipmapFilter = ImageFilter::Linear});
            passData.DepthOut = graph.AddExternal("pass-test-3d-depth", depthTarget);

            passData.ColorOut = graph.RenderTarget(passData.ColorOut,
                AttachmentLoad::Clear, AttachmentStore::Store, glm::vec4(0.5f, 0.5f, 0.5f, 1.0f));
            passData.DepthOut = graph.DepthStencilTarget(passData.DepthOut,
                AttachmentLoad::Clear, AttachmentStore::Store, 0.0f);

            renderGraph.GetBlackboard().RegisterOutput(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            GPU_PROFILE_FRAME("Test 3d")
            auto& cmd = frameContext.Cmd;

            CameraData cameraData = {
                .View = frameContext.Camera->GetView(),
                .Projection = frameContext.Camera->GetProjection(),
                .ViewProjection = frameContext.Camera->GetViewProjection()};
            const Buffer& cameraUbo = resources.GetBuffer(passData.CameraUbo, cameraData,
                *frameContext.ResourceUploader);

            descriptors.UpdateBinding(cameraBinding, cameraUbo.CreateBindingInfo());
            
            pipeline.BindGraphics(frameContext.Cmd);
            descriptors.BindGraphics(frameContext.Cmd, resources.GetGraph()->GetArenaAllocators(),
                pipeline.GetLayout());

            RenderCommand::BindIndexU8Buffer(cmd, geometry->GetAttributeBuffers().Indices, 0);
            RenderCommand::BindVertexBuffers(cmd,
                {
                    geometry->GetAttributeBuffers().Positions,
                    geometry->GetAttributeBuffers().Normals,
                    geometry->GetAttributeBuffers().Tangents,
                    geometry->GetAttributeBuffers().UVs
                },
                {0, 0, 0, 0});

            RenderCommand::DrawIndexedIndirect(cmd, geometry->GetCommandsBuffer(), 0, geometry->GetMeshletCount());
        });
}
