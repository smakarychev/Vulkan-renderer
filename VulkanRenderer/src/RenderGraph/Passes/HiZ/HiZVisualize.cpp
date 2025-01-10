#include "HiZVisualize.h"

#include "Renderer.h"
#include "imgui/imgui.h"
#include "Rendering/ShaderCache.h"
#include "Vulkan/RenderCommand.h"

RG::Pass& Passes::HiZVisualize::addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource hiz)
{
    using namespace RG;

    struct PushConstants
    {
        u32 MipLevel{0};
        f32 IntensityScale{10.0f};
    };

    Pass& pass = renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("HiZ.Visualize.Setup")

            graph.SetShader("../assets/shaders/hiz-visualize.shader");
            
            passData.HiZ = graph.Read(hiz,
                ResourceAccessFlags::Pixel | ResourceAccessFlags::Sampled);
            const TextureDescription& hizDescription = graph.GetTextureDescription(hiz);

            passData.ColorOut = graph.CreateResource("HiZ.Visualize.ColorOut", GraphTextureDescription{
                .Width = hizDescription.Width,
                .Height = hizDescription.Height,
                .Format = Format::RGBA16_FLOAT});
            passData.ColorOut = graph.RenderTarget(passData.ColorOut,
                AttachmentLoad::Load, AttachmentStore::Store);

            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("HiZ.Visualize")
            GPU_PROFILE_FRAME("HiZ.Visualize")
            
            const Texture& hizTexture = resources.GetTexture(passData.HiZ);
            PushConstants& pushConstants = resources.GetOrCreateValue<PushConstants>();
            ImGui::Begin("HiZ visualize");
            ImGui::DragInt("mip level", (i32*)&pushConstants.MipLevel, 1.0f, 0, 10);            
            ImGui::DragFloat("intensity", &pushConstants.IntensityScale, 10.0f, 1.0f, 1e+4f);            
            ImGui::End();

            const Shader& shader = resources.GetGraph()->GetShader();
            auto& pipeline = shader.Pipeline(); 
            auto& samplerDescriptors = shader.Descriptors(ShaderDescriptorsKind::Sampler);
            auto& resourceDescriptors = shader.Descriptors(ShaderDescriptorsKind::Resource);
            
            samplerDescriptors.UpdateBinding("u_sampler",
                hizTexture.BindingInfo(ImageFilter::Nearest, ImageLayout::Readonly));
            resourceDescriptors.UpdateBinding("u_hiz",
                hizTexture.BindingInfo(ImageFilter::Nearest, ImageLayout::Readonly));

            pipeline.BindGraphics(frameContext.Cmd);
            RenderCommand::PushConstants(frameContext.Cmd, shader.GetLayout(), pushConstants);
            samplerDescriptors.BindGraphics(frameContext.Cmd, resources.GetGraph()->GetArenaAllocators(),
                shader.GetLayout());
            resourceDescriptors.BindGraphics(frameContext.Cmd, resources.GetGraph()->GetArenaAllocators(),
                shader.GetLayout());

            RenderCommand::Draw(frameContext.Cmd, 3);
        });

    return pass;
}
