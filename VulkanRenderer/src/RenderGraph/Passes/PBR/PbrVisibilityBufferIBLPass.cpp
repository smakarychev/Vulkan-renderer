#include "PbrVisibilityBufferIBLPass.h"

#include "FrameContext.h"
#include "Core/Camera.h"
#include "RenderGraph/RGGeometry.h"
#include "RenderGraph/RGUtils.h"
#include "Vulkan/RenderCommand.h"

PbrVisibilityBufferIBL::PbrVisibilityBufferIBL(RG::Graph& renderGraph, const PbrVisibilityBufferInitInfo& info)
{
    ShaderPipelineTemplate* pbrTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
          "../assets/shaders/processed/render-graph/common/fullscreen-vert.shader",
          "../assets/shaders/processed/render-graph/pbr/pbr-visibility-buffer-ibl-frag.shader"},
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
            
            Resource color = RgUtils::ensureResource(info.ColorIn, graph, name + ".Color",
                GraphTextureDescription{
                   .Width = visibilityDescription.Width,
                   .Height = visibilityDescription.Height,
                   .Format = Format::RGBA16_FLOAT});

            Resource irradiance = info.IBL.Irradiance;
            Resource prefilter = info.IBL.PrefilterEnvironment;
            Resource brdf = info.IBL.BRDF;
            
            ASSERT(irradiance.IsValid(), "Must provide irradiance map")
            ASSERT(prefilter.IsValid(), "Must provide prefilter map")
            ASSERT(brdf.IsValid(), "Must provide brdf")
            
            Resource ssao = RgUtils::ensureResource(info.SSAO.SSAOTexture, graph, "SSAO.Dummy",
                ImageUtils::DefaultTexture::White);

            auto& graphGlobals = graph.GetGlobalResources();
            
            passData.VisibilityTexture = graph.Read(info.VisibilityTexture, Pixel | Sampled);
            passData.IBL.Irradiance = graph.Read(irradiance, Pixel | Sampled);
            passData.IBL.PrefilterEnvironment = graph.Read(prefilter, Pixel | Sampled);
            passData.IBL.BRDF = graph.Read(brdf, Pixel | Sampled);
            passData.SSAO.SSAOTexture = graph.Read(ssao, Pixel | Sampled);
            passData.CameraUbo = graph.Read(graphGlobals.MainCameraGPU, Pixel | Uniform);
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
            
            graph.GetBlackboard().Register(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            GPU_PROFILE_FRAME("PBR Visibility pass")

            const Texture& visibility = resources.GetTexture(passData.VisibilityTexture);
            const Texture& irradiance = resources.GetTexture(passData.IBL.Irradiance);
            const Texture& prefilter = resources.GetTexture(passData.IBL.PrefilterEnvironment);
            const Texture& brdf = resources.GetTexture(passData.IBL.BRDF);
            const Texture& ssao = resources.GetTexture(passData.SSAO.SSAOTexture);

            const Buffer& cameraUbo = resources.GetBuffer(passData.CameraUbo);

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

            resourceDescriptors.UpdateBinding("u_visibility_texture", visibility.BindingInfo(ImageFilter::Nearest,
                ImageLayout::Readonly));
            resourceDescriptors.UpdateBinding("u_irradiance_map", irradiance.BindingInfo(ImageFilter::Linear,
                ImageLayout::Readonly));
            resourceDescriptors.UpdateBinding("u_prefilter_map", prefilter.BindingInfo(ImageFilter::Linear,
                ImageLayout::Readonly));
            resourceDescriptors.UpdateBinding("u_brdf", brdf.BindingInfo(ImageFilter::Linear,
                ImageLayout::Readonly));
            resourceDescriptors.UpdateBinding("u_ssao_texture", ssao.BindingInfo(ImageFilter::Linear,
                ImageLayout::Readonly));
            resourceDescriptors.UpdateBinding("u_camera", cameraUbo.BindingInfo());
            resourceDescriptors.UpdateBinding("u_commands", commandsSsbo.BindingInfo());
            resourceDescriptors.UpdateBinding("u_objects", objectsSsbo.BindingInfo());
            resourceDescriptors.UpdateBinding("u_positions", positionsSsbo.BindingInfo());
            resourceDescriptors.UpdateBinding("u_normals", normalsSsbo.BindingInfo());
            resourceDescriptors.UpdateBinding("u_tangents", tangentsSsbo.BindingInfo());
            resourceDescriptors.UpdateBinding("u_uv", uvsSsbo.BindingInfo());
            resourceDescriptors.UpdateBinding("u_indices", indicesSsbo.BindingInfo());
            
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