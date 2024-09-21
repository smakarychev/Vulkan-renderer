#include "PbrVisibilityBufferIBLPass.h"

#include "FrameContext.h"
#include "Scene/SceneGeometry.h"
#include "RenderGraph/RGUtils.h"
#include "Rendering/ShaderCache.h"
#include "Vulkan/RenderCommand.h"

RG::Pass& Passes::Pbr::VisibilityIbl::addToGraph(std::string_view name, RG::Graph& renderGraph,
    const PbrVisibilityBufferExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    Pass& pass = renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Pbr.Visibility.IBL.Setup")

            bool useHybrid = info.Tiles.IsValid() && info.Clusters.IsValid();
            bool useTiled = !useHybrid && info.Tiles.IsValid();
            bool useClustered = !useHybrid &&  info.Clusters.IsValid();

            graph.SetShader("../assets/shaders/pbr-visibility-ibl.shader",
                ShaderOverrides{}
                    .Add(
                        {"MAX_REFLECTION_LOD"},
                        (f32)Image::CalculateMipmapCount({PREFILTER_RESOLUTION, PREFILTER_RESOLUTION}))
                    .Add({"USE_TILED_LIGHTING"}, useTiled)
                    .Add({"USE_CLUSTERED_LIGHTING"}, useClustered)
                    .Add({"USE_HYBRID_LIGHTING"}, useHybrid));

            passData.Commands = graph.AddExternal(std::string{name} + ".Commands", info.Geometry->GetCommandsBuffer());
            passData.Objects = graph.AddExternal(std::string{name} + ".Objects",
                info.Geometry->GetRenderObjectsBuffer());
            auto& attributes = info.Geometry->GetAttributeBuffers();
            passData.Positions = graph.AddExternal(std::string{name} + ".Positions", attributes.Positions);
            passData.Normals = graph.AddExternal(std::string{name} + ".Normals", attributes.Normals);
            passData.Tangents = graph.AddExternal(std::string{name} + ".Tangents", attributes.Tangents);
            passData.UVs = graph.AddExternal(std::string{name} + ".UVs", attributes.UVs);
            passData.Indices = graph.AddExternal(std::string{name} + ".Indices", attributes.Indices);

            const TextureDescription& visibilityDescription =
                Resources(graph).GetTextureDescription(info.VisibilityTexture);
            
            Resource color = RgUtils::ensureResource(info.ColorIn, graph, std::string{name} + ".Color",
                GraphTextureDescription{
                   .Width = visibilityDescription.Width,
                   .Height = visibilityDescription.Height,
                   .Format = Format::RGBA16_FLOAT});

            passData.LightsResources = RgUtils::readSceneLight(*info.SceneLights, graph, Pixel);
            if (info.Clusters.IsValid())
                passData.Clusters = graph.Read(info.Clusters, Pixel | Storage);
            if (info.Tiles.IsValid())
            {
                passData.Tiles = graph.Read(info.Tiles, Pixel | Storage);
                passData.ZBins = graph.Read(info.ZBins, Pixel | Storage);
            }
            passData.IBL = RgUtils::readIBLData(info.IBL, graph, Pixel);
            passData.SSAO = RgUtils::readSSAOData(info.SSAO, graph, Pixel);
            passData.CSMData = RgUtils::readCSMData(info.CSMData, graph, Pixel);

            auto& graphGlobals = graph.GetGlobalResources();
            
            passData.VisibilityTexture = graph.Read(info.VisibilityTexture, Pixel | Sampled);
            
            passData.Camera = graph.Read(graphGlobals.PrimaryCameraGPU, Pixel | Uniform);
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

            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("PBR Visibility pass")
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

            const Shader& shader = resources.GetGraph()->GetShader();
            auto& pipeline = shader.Pipeline(); 
            auto& samplerDescriptors = shader.Descriptors(ShaderDescriptorsKind::Sampler);
            auto& resourceDescriptors = shader.Descriptors(ShaderDescriptorsKind::Resource);
            auto& materialDescriptors = shader.Descriptors(ShaderDescriptorsKind::Materials);

            resourceDescriptors.UpdateBinding("u_visibility_texture", visibility.BindingInfo(ImageFilter::Nearest,
                ImageLayout::Readonly));
            RgUtils::updateSceneLightBindings(resourceDescriptors, resources, passData.LightsResources);
            if (passData.Clusters.IsValid())
                resourceDescriptors.UpdateBinding("u_clusters", resources.GetBuffer(passData.Clusters).BindingInfo());
            if (passData.Tiles.IsValid())
            {
                resourceDescriptors.UpdateBinding("u_tiles", resources.GetBuffer(passData.Tiles).BindingInfo());
                resourceDescriptors.UpdateBinding("u_zbins", resources.GetBuffer(passData.ZBins).BindingInfo());
            }
            RgUtils::updateIBLBindings(resourceDescriptors, resources, passData.IBL);
            RgUtils::updateSSAOBindings(resourceDescriptors, resources, passData.SSAO);
            RgUtils::updateCSMBindings(resourceDescriptors, resources, passData.CSMData);
            resourceDescriptors.UpdateBinding("u_camera", cameraBuffer.BindingInfo());
            resourceDescriptors.UpdateBinding("u_shading", shadingSettings.BindingInfo());
            resourceDescriptors.UpdateBinding("u_commands", commands.BindingInfo());
            resourceDescriptors.UpdateBinding("u_objects", objects.BindingInfo());
            resourceDescriptors.UpdateBinding(UNIFORM_POSITIONS, positions.BindingInfo());
            resourceDescriptors.UpdateBinding(UNIFORM_NORMALS, normals.BindingInfo());
            resourceDescriptors.UpdateBinding(UNIFORM_TANGENTS, tangents.BindingInfo());
            resourceDescriptors.UpdateBinding(UNIFORM_UV, uvs.BindingInfo());
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

    return pass;
}
