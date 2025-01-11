#include "Swapchain.h"

#include "Vulkan/Device.h"
#include "Vulkan/RenderCommand.h"
#include "utils/utils.h"

void Swapchain::Destroy(const Swapchain& swapchain)
{
    Device::Destroy(swapchain.Handle());
}

void Swapchain::DestroyImages(const Swapchain& swapchain)
{
    Device::DestroySwapchainImages(swapchain);
}

u32 Swapchain::AcquireImage(u32 frameNumber)
{
    return Device::AcquireNextImage(*this, m_SwapchainFrameSync[frameNumber]);
}

bool Swapchain::PresentImage(QueueKind queueKind, u32 imageIndex, u32 frameNumber)
{
    return Device::Present(*this, queueKind, m_SwapchainFrameSync[frameNumber], imageIndex);
}

void Swapchain::PreparePresent(const CommandBuffer& cmd, u32 imageIndex)
{
    ImageSubresource drawSubresource = m_DrawImage.Subresource(0, 1, 0, 1);
    ImageSubresource presentSubresource = m_ColorImages[imageIndex].Subresource(0, 1, 0, 1);
    Barrier barrier = {};
    DeletionQueue deletionQueue = {};

    LayoutTransitionInfo presentToDestinationTransitionInfo = {
        .ImageSubresource = presentSubresource,
        .SourceStage = PipelineStage::ColorOutput,
        .DestinationStage = PipelineStage::Bottom,
        .SourceAccess = PipelineAccess::ReadColorAttachment | PipelineAccess::WriteColorAttachment,
        .DestinationAccess = PipelineAccess::None,
        .OldLayout = ImageLayout::Undefined,
        .NewLayout = ImageLayout::Destination 
    }; 

    LayoutTransitionInfo destinationToPresentTransitionInfo = presentToDestinationTransitionInfo;
    destinationToPresentTransitionInfo.OldLayout = ImageLayout::Destination;
    destinationToPresentTransitionInfo.NewLayout = ImageLayout::Present;

    DependencyInfo presentToDestinationTransition = DependencyInfo::Builder()
        .LayoutTransition(presentToDestinationTransitionInfo)
        .Build(deletionQueue);
    DependencyInfo destinationToPresentTransition = DependencyInfo::Builder()
        .LayoutTransition(destinationToPresentTransitionInfo)
        .Build(deletionQueue);

    barrier.Wait(cmd, presentToDestinationTransition);

    ImageBlitInfo source = m_DrawImage.BlitInfo(
        drawSubresource.Description.MipmapBase, drawSubresource.Description.LayerBase,
        drawSubresource.Description.Layers);
    ImageBlitInfo destination = m_ColorImages[imageIndex].BlitInfo(
        presentSubresource.Description.MipmapBase, presentSubresource.Description.LayerBase,
        presentSubresource.Description.Layers);
    
    RenderCommand::BlitImage(cmd, source, destination, ImageFilter::Linear);

    barrier.Wait(cmd, destinationToPresentTransition);
}

const SwapchainFrameSync& Swapchain::GetFrameSync(u32 frameNumber) const
{
    return m_SwapchainFrameSync[frameNumber];
}

const std::vector<SwapchainFrameSync>& Swapchain::GetFrameSync() const
{
    return m_SwapchainFrameSync;
}

std::vector<Image> Swapchain::CreateColorImages() const
{
    return Device::CreateSwapchainImages(*this);
}

Image Swapchain::CreateDrawImage()
{
    m_DrawImage = Image::Builder({
            .Width = m_DrawResolution.x,
            .Height = m_DrawResolution.y,
            .Format = m_DrawFormat,
            .Usage = ImageUsage::Source | ImageUsage::Destination | ImageUsage::Storage | ImageUsage::Color})
        .BuildManualLifetime();

    return m_DrawImage;
}

Image Swapchain::CreateDepthImage()
{
    Image depth = Image::Builder({
            .Width = m_DrawResolution.x,
            .Height = m_DrawResolution.y,
            .Format = m_DepthFormat,
            .Usage = ImageUsage::Depth | ImageUsage::Stencil | ImageUsage::Sampled})
        .BuildManualLifetime();

    return depth;
}

