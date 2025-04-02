#include "SkyboxPass.h"

#include "FrameContext.h"
#include "Core/Camera.h"
#include "RenderGraph/RGUtils.h"
#include "RenderGraph/Passes/Generated/SkyboxBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

RG::Pass& Passes::Skybox::addToGraph(StringId name, RG::Graph& renderGraph, Texture skybox,
    RG::Resource colorOut, RG::Resource depthIn, const glm::uvec2& resolution, f32 lodBias)
{
    return addToGraph(name, renderGraph, renderGraph.AddExternal("Skybox"_hsv, skybox),
        colorOut, depthIn, resolution, lodBias);
}

RG::Pass& Passes::Skybox::addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource skybox,
    RG::Resource colorOut, RG::Resource depthIn, const glm::uvec2& resolution, f32 lodBias)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    Pass& pass = renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Skybox.Setup")

            graph.SetShader("skybox.shader");
            
            passData.ColorOut = RgUtils::ensureResource(colorOut, graph, "Color"_hsv,
                GraphTextureDescription{
                    .Width = resolution.x,
                    .Height = resolution.y,
                    .Format = Format::RGBA16_FLOAT});
            ASSERT(depthIn.IsValid(), "Depth has to be provided")

            passData.Projection = graph.CreateResource("Projection"_hsv, GraphBufferDescription{
                .SizeBytes = sizeof(ProjectionUBO)});
            
            auto& globalResources = graph.GetGlobalResources();
      
            passData.Skybox = graph.Read(skybox, Pixel | Sampled);
            passData.ColorOut = graph.RenderTarget(passData.ColorOut, AttachmentLoad::Load, AttachmentStore::Store);
            passData.DepthOut = graph.DepthStencilTarget(depthIn, AttachmentLoad::Load, AttachmentStore::Store);
            passData.Projection = graph.Read(passData.Projection, Vertex | Uniform);
            ProjectionUBO projection = {
                .ProjectionInverse = glm::inverse(globalResources.PrimaryCamera->GetProjection()),
                .ViewInverse = glm::inverse(globalResources.PrimaryCamera->GetView())};
            graph.Upload(passData.Projection, projection);

            passData.ShadingSettings = graph.Read(globalResources.ShadingSettings, Pixel | Uniform);

            passData.LodBias = lodBias;

            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Skybox")
            GPU_PROFILE_FRAME("Skybox")

            Texture skyboxTexture = resources.GetTexture(passData.Skybox);
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
