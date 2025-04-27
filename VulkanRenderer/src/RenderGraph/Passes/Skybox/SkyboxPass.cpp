#include "SkyboxPass.h"

#include "FrameContext.h"
#include "Core/Camera.h"
#include "RenderGraph/RGUtils.h"
#include "RenderGraph/Passes/Generated/SkyboxBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

RG::Pass& Passes::Skybox::addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    struct ProjectionUBO
    {
        glm::mat4 ProjectionInverse{1.0f};
        glm::mat4 ViewInverse{1.0f};
    };

    struct PassDataPrivate
    {
        Resource Color{};
        Resource Depth{};
        Resource Skybox{};
        Resource Projection{};
        Resource ShadingSettings{};
        f32 LodBias{0.0f};
    };
    
    Pass& pass = renderGraph.AddRenderPass<PassDataPrivate>(name,
        [&](Graph& graph, PassDataPrivate& passData)
        {
            CPU_PROFILE_FRAME("Skybox.Setup")

            graph.SetShader("skybox.shader");
            
            passData.Color = RgUtils::ensureResource(info.Color, graph, "Color"_hsv,
                GraphTextureDescription{
                    .Width = info.Resolution.x,
                    .Height = info.Resolution.y,
                    .Format = Format::RGBA16_FLOAT});
            ASSERT(info.Depth.IsValid(), "Depth has to be provided")

            passData.Projection = graph.CreateResource("Projection"_hsv, GraphBufferDescription{
                .SizeBytes = sizeof(ProjectionUBO)});
            
            auto& globalResources = graph.GetGlobalResources();

            const Resource skybox = info.SkyboxResource.IsValid() ?
                info.SkyboxResource : graph.AddExternal("Skybox"_hsv, info.SkyboxTexture);
      
            passData.Skybox = graph.Read(skybox, Pixel | Sampled);
            passData.Color = graph.RenderTarget(passData.Color, AttachmentLoad::Load, AttachmentStore::Store);
            passData.Depth = graph.DepthStencilTarget(info.Depth, AttachmentLoad::Load, AttachmentStore::Store);
            passData.Projection = graph.Read(passData.Projection, Vertex | Uniform);
            ProjectionUBO projection = {
                .ProjectionInverse = glm::inverse(globalResources.PrimaryCamera->GetProjection()),
                .ViewInverse = glm::inverse(globalResources.PrimaryCamera->GetView())};
            graph.Upload(passData.Projection, projection);

            passData.ShadingSettings = graph.Read(globalResources.ShadingSettings, Pixel | Uniform);

            passData.LodBias = info.LodBias;

            PassData passDataPublic = {};
            passDataPublic.Color = passData.Color;
            passDataPublic.Depth = passData.Depth;

            graph.UpdateBlackboard(passDataPublic);
        },
        [=](PassDataPrivate& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Skybox")
            GPU_PROFILE_FRAME("Skybox")

            const Texture skyboxTexture = resources.GetTexture(passData.Skybox);
            const Buffer projectionBuffer = resources.GetBuffer(passData.Projection);
            
            const Shader& shader = resources.GetGraph()->GetShader();
            SkyboxShaderBindGroup bindGroup(shader);

            bindGroup.SetSkybox({.Image = skyboxTexture}, ImageLayout::Readonly);
            bindGroup.SetProjection({.Buffer = projectionBuffer});
            bindGroup.SetShading({.Buffer = resources.GetBuffer(passData.ShadingSettings)});
            
            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, resources.GetGraph()->GetArenaAllocators());
            cmd.PushConstants({
            	.PipelineLayout = shader.GetLayout(), 
            	.Data = {passData.LodBias}});
            cmd.Draw({.VertexCount = 6});
        });

    return pass;
}
