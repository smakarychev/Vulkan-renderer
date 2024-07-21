#include "PbrVisibilityBufferIBLPass.h"

#include "FrameContext.h"
#include "Core/Camera.h"
#include "Scene/SceneGeometry.h"
#include "RenderGraph/RGUtils.h"
#include "Vulkan/RenderCommand.h"

PbrVisibilityBufferIBL::PbrVisibilityBufferIBL(RG::Graph& renderGraph, const PbrVisibilityBufferInitInfo& info)
{
    ShaderPipelineTemplate* pbrTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
          "../assets/shaders/processed/render-graph/common/fullscreen-vert.stage",
          "../assets/shaders/processed/render-graph/pbr/pbr-visibility-buffer-ibl-frag.stage"},
      "Pass.PBR.Visibility.IBL", renderGraph.GetArenaAllocators());

    m_PipelineData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(pbrTemplate)
        .SetRenderingDetails({
            .ColorFormats = {Format::RGBA16_FLOAT}})
        .AddSpecialization("MAX_REFLECTION_LOD",
            (f32)Image::CalculateMipmapCount({PREFILTER_RESOLUTION, PREFILTER_RESOLUTION}))
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

void PbrVisibilityBufferIBL::AddToGraph(RG::Graph& renderGraph, const PbrVisibilityBufferExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    std::string name = "Pbr.VisibilityBuffer";
    m_Pass = &renderGraph.AddRenderPass<PassData>(PassName{name},
        [&](Graph& graph, PassData& passData)
        {
            passData.Commands = graph.AddExternal(name + ".Commands", info.Geometry->GetCommandsBuffer());
            passData.Objects = graph.AddExternal(name + ".Objects", info.Geometry->GetRenderObjectsBuffer());
            auto& attributes = info.Geometry->GetAttributeBuffers();
            passData.Positions = graph.AddExternal(name + ".Positions", attributes.Positions);
            passData.Normals = graph.AddExternal(name + ".Normals", attributes.Normals);
            passData.Tangents = graph.AddExternal(name + ".Tangents", attributes.Tangents);
            passData.UVs = graph.AddExternal(name + ".UVs", attributes.UVs);
            passData.Indices = graph.AddExternal(name + ".Indices", attributes.Indices);

            const TextureDescription& visibilityDescription =
                Resources(graph).GetTextureDescription(info.VisibilityTexture);
            
            Resource color = RgUtils::ensureResource(info.ColorIn, graph, name + ".Color",
                GraphTextureDescription{
                   .Width = visibilityDescription.Width,
                   .Height = visibilityDescription.Height,
                   .Format = Format::RGBA16_FLOAT});

            passData.LightsResources = RgUtils::readSceneLight(*info.SceneLights, graph, Pixel);
            passData.IBL = RgUtils::readIBLData(info.IBL, graph, Pixel);
            passData.SSAO = RgUtils::readSSAOData(info.SSAO, graph, Pixel);
            passData.CSMData = RgUtils::readCSMData(info.CSMData, graph, Pixel);

            auto& graphGlobals = graph.GetGlobalResources();
            
            passData.VisibilityTexture = graph.Read(info.VisibilityTexture, Pixel | Sampled);
            
            passData.Camera = graph.Read(graphGlobals.MainCameraGPU, Pixel | Uniform);
            passData.ShadingSettings = graph.Read(graphGlobals.ShadingSettings, Pixel | Uniform);
            passData.Commands = graph.Read(passData.Commands, Pixel | Storage);
            passData.Objects = graph.Read(passData.Objects, Pixel | Storage);
            passData.Positions = graph.Read(passData.Positions, Pixel | Storage);
            passData.Normals = graph.Read(passData.Normals, Pixel | Storage);
            passData.Tangents = graph.Read(passData.Tangents, Pixel | Storage);
            passData.UVs = graph.Read(passData.UVs, Pixel | Storage);
            passData.Indices = graph.Read(passData.Indices, Pixel | Storage);

            passData.ColorOut = graph.RenderTarget(color,
                info.ColorIn.IsValid() ? AttachmentLoad::Load : AttachmentLoad::Clear,
                AttachmentStore::Store);

            passData.PipelineData = &m_PipelineData;
            
            graph.GetBlackboard().Update(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            GPU_PROFILE_FRAME("PBR Visibility pass")

            const Texture& visibility = resources.GetTexture(passData.VisibilityTexture);
            const Buffer& cameraBuffer = resources.GetBuffer(passData.Camera);
            const Buffer& shadingSettings = resources.GetBuffer(passData.ShadingSettings);

            const Buffer& commands = resources.GetBuffer(passData.Commands);
            const Buffer& objects = resources.GetBuffer(passData.Objects);
            const Buffer& positions = resources.GetBuffer(passData.Positions);
            const Buffer& normals = resources.GetBuffer(passData.Normals);
            const Buffer& tangents = resources.GetBuffer(passData.Tangents);
            const Buffer& uvs = resources.GetBuffer(passData.UVs);
            const Buffer& indices = resources.GetBuffer(passData.Indices);

            auto& pipeline = passData.PipelineData->Pipeline;
            auto& samplerDescriptors = passData.PipelineData->ImmutableSamplerDescriptors;
            auto& resourceDescriptors = passData.PipelineData->ResourceDescriptors;
            auto& materialDescriptors = passData.PipelineData->MaterialDescriptors;

            resourceDescriptors.UpdateBinding("u_visibility_texture", visibility.BindingInfo(ImageFilter::Nearest,
                ImageLayout::Readonly));
            RgUtils::updateSceneLightBindings(resourceDescriptors, resources, passData.LightsResources);
            RgUtils::updateIBLBindings(resourceDescriptors, resources, passData.IBL);
            RgUtils::updateSSAOBindings(resourceDescriptors, resources, passData.SSAO);
            RgUtils::updateCSMBindings(resourceDescriptors, resources, passData.CSMData);
            resourceDescriptors.UpdateBinding("u_camera", cameraBuffer.BindingInfo());
            resourceDescriptors.UpdateBinding("u_shading", shadingSettings.BindingInfo());
            resourceDescriptors.UpdateBinding("u_commands", commands.BindingInfo());
            resourceDescriptors.UpdateBinding("u_objects", objects.BindingInfo());
            resourceDescriptors.UpdateBinding("u_positions", positions.BindingInfo());
            resourceDescriptors.UpdateBinding("u_normals", normals.BindingInfo());
            resourceDescriptors.UpdateBinding("u_tangents", tangents.BindingInfo());
            resourceDescriptors.UpdateBinding("u_uv", uvs.BindingInfo());
            resourceDescriptors.UpdateBinding("u_indices", indices.BindingInfo());
            
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
