#include "HiZVisualize.h"

#include "Renderer.h"
#include "imgui/imgui.h"
#include "Rendering/ShaderCache.h"
#include "Vulkan/RenderCommand.h"

HiZVisualize::HiZVisualize(RG::Graph& renderGraph)
{
}

void HiZVisualize::AddToGraph(RG::Graph& renderGraph, RG::Resource hiz)
{
    using namespace RG;

    static ShaderDescriptors::BindingInfo samplerBindingInfo =
        Experimental::ShaderCache::Get("HiZ.Visualize").Descriptors(Experimental::DescriptorsKind::Sampler).GetBindingInfo("u_sampler");
    static ShaderDescriptors::BindingInfo hizBindingInfo =
        Experimental::ShaderCache::Get("HiZ.Visualize").Descriptors(Experimental::DescriptorsKind::Resource).GetBindingInfo("u_hiz");

    m_Pass = &renderGraph.AddRenderPass<PassData>({"HiZ.Visualize"},
        [&](Graph& graph, PassData& passData)
        {
            passData.HiZ = graph.Read(hiz,
                ResourceAccessFlags::Pixel | ResourceAccessFlags::Sampled);
            const TextureDescription& hizDescription = graph.GetTextureDescription(hiz);

            passData.ColorOut = graph.CreateResource("HiZ.Visualize.ColorOut", GraphTextureDescription{
                .Width = hizDescription.Width,
                .Height = hizDescription.Height,
                .Format = Format::RGBA16_FLOAT});
            passData.ColorOut = graph.RenderTarget(passData.ColorOut,
                AttachmentLoad::Load, AttachmentStore::Store);

            passData.PushConstants = &m_PushConstants;

            graph.GetBlackboard().Update(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            GPU_PROFILE_FRAME("HiZ visualize")
            
            const Texture& hizTexture = resources.GetTexture(passData.HiZ);
            auto& pushConstants = *passData.PushConstants;
            ImGui::Begin("HiZ visualize");
            ImGui::DragInt("mip level", (i32*)&pushConstants.MipLevel, 1.0f, 0, 10);            
            ImGui::DragFloat("intensity", &pushConstants.IntensityScale, 10.0f, 1.0f, 1e+4f);            
            ImGui::End();

            auto& pipeline = Experimental::ShaderCache::Get("HiZ.Visualize").Pipeline(); 
            auto& samplerDescriptors = Experimental::ShaderCache::Get("HiZ.Visualize").Descriptors(Experimental::DescriptorsKind::Sampler);
            auto& resourceDescriptors = Experimental::ShaderCache::Get("HiZ.Visualize").Descriptors(Experimental::DescriptorsKind::Resource);
            
            samplerDescriptors.UpdateBinding(samplerBindingInfo,
                hizTexture.BindingInfo(ImageFilter::Nearest, ImageLayout::Readonly));
            resourceDescriptors.UpdateBinding(hizBindingInfo,
                hizTexture.BindingInfo(ImageFilter::Nearest, ImageLayout::Readonly));

            pipeline.BindGraphics(frameContext.Cmd);
            RenderCommand::PushConstants(frameContext.Cmd, pipeline.GetLayout(), pushConstants);
            samplerDescriptors.BindGraphics(frameContext.Cmd, resources.GetGraph()->GetArenaAllocators(),
                pipeline.GetLayout());
            resourceDescriptors.BindGraphics(frameContext.Cmd, resources.GetGraph()->GetArenaAllocators(),
                pipeline.GetLayout());

            RenderCommand::Draw(frameContext.Cmd, 3);
        });
}
