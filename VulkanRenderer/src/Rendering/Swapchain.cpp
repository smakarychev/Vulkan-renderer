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
    ImageSubresource drawSubresource = {
        .Image = &m_DrawImage,
        .Description = {.Mipmaps = 1, .Layers = 1}};
    ImageSubresource presentSubresource = {
        .Image = &m_ColorImages[imageIndex],
        .Description = {.Mipmaps = 1, .Layers = 1}};
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

    DependencyInfo presentToDestinationTransition = Device::CreateDependencyInfo({
        .LayoutTransitionInfo = presentToDestinationTransitionInfo});
    DependencyInfo destinationToPresentTransition = Device::CreateDependencyInfo({
        .LayoutTransitionInfo = destinationToPresentTransitionInfo});
    deletionQueue.Enqueue(presentToDestinationTransition);
    deletionQueue.Enqueue(destinationToPresentTransition);

    barrier.Wait(cmd, presentToDestinationTransition);

    ImageBlitInfo source = {
        .Image = &m_DrawImage,
        .MipmapBase = (u32)drawSubresource.Description.MipmapBase,
        .LayerBase = (u32)drawSubresource.Description.LayerBase,
        .Layers = (u32)drawSubresource.Description.Layers,
        .Top = m_DrawImage.Description().Dimensions()};
    ImageBlitInfo destination = {
        .Image = &m_ColorImages[imageIndex],
        .MipmapBase = (u32)presentSubresource.Description.MipmapBase,
        .LayerBase = (u32)presentSubresource.Description.LayerBase,
        .Layers = (u32)presentSubresource.Description.Layers,
        .Top = m_ColorImages[imageIndex].Description().Dimensions()};
    
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
    m_DrawImage = Device::CreateImage({
        .Description = ImageDescription{
            .Width = m_DrawResolution.x,
            .Height = m_DrawResolution.y,
            .Format = m_DrawFormat,
            .Usage = ImageUsage::Source | ImageUsage::Destination | ImageUsage::Storage | ImageUsage::Color}},
        Device::DummyDeletionQueue());

    return m_DrawImage;
}

Image Swapchain::CreateDepthImage()
{
    Image depth = Device::CreateImage({
        .Description = ImageDescription{
            .Width = m_DrawResolution.x,
            .Height = m_DrawResolution.y,
            .Format = m_DepthFormat,
            .Usage = ImageUsage::Depth | ImageUsage::Stencil | ImageUsage::Sampled}},
        Device::DummyDeletionQueue());

    return depth;
}

