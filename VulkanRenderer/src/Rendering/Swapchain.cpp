#include "Swapchain.h"

#include "Device.h"
#include "Vulkan/Driver.h"
#include "Vulkan/RenderCommand.h"
#include "utils/utils.h"

Swapchain Swapchain::Builder::Build()
{
    PreBuild();
    
    Swapchain swapchain = Swapchain::Create(m_CreateInfo);
    Driver::DeletionQueue().Enqueue(swapchain);

    return swapchain;
}

Swapchain Swapchain::Builder::BuildManualLifetime()
{
    PreBuild();
    
    return Swapchain::Create(m_CreateInfo);
}

Swapchain::Builder& Swapchain::Builder::SetDrawResolution(const glm::uvec2& resolution)
{
    ASSERT(resolution.x != 0 && resolution.y != 0, "Draw resolution must be greater than zero")
    m_CreateInfo.DrawResolution = resolution;

    return *this;
}

Swapchain::Builder& Swapchain::Builder::SetDevice(const Device& device)
{
    m_CreateInfo.Device = &device;
    
    return *this;
}

Swapchain::Builder& Swapchain::Builder::BufferedFrames(u32 count)
{
    m_BufferedFrames = count;

    return *this;
}

Swapchain::Builder& Swapchain::Builder::SetSyncStructures(const std::vector<SwapchainFrameSync>& syncs)
{
    m_CreateInfo.FrameSyncs = syncs;

    return *this;
}

void Swapchain::Builder::PreBuild()
{
    ASSERT(m_CreateInfo.Device, "Device is unset")
    
    m_CreateInfo.DrawFormat = Format::RGBA16_FLOAT;
    m_CreateInfo.DepthStencilFormat = ChooseDepthFormat();
    
    if (m_CreateInfo.FrameSyncs.empty())
        m_CreateInfo.FrameSyncs = CreateSynchronizationStructures();
}

Format Swapchain::Builder::ChooseDepthFormat()
{
    return Format::D32_FLOAT;
}

std::vector<SwapchainFrameSync> Swapchain::Builder::CreateSynchronizationStructures()
{
    ASSERT(m_BufferedFrames != 0, "Buffered frames count is unset")
    std::vector<SwapchainFrameSync> swapchainFrameSyncs;
    swapchainFrameSyncs.reserve(m_BufferedFrames);
    for (u32 i = 0; i < m_BufferedFrames; i++)
    {
        Fence renderFence = Fence::Builder()
            .StartSignaled(true)
            .Build();
        Semaphore renderSemaphore = Semaphore::Builder().Build();
        Semaphore presentSemaphore = Semaphore::Builder().Build();

        swapchainFrameSyncs.push_back({
            .RenderFence = renderFence,
            .RenderSemaphore = renderSemaphore,
            .PresentSemaphore = presentSemaphore});
    }

    return swapchainFrameSyncs;
}


Swapchain Swapchain::Create(const Builder::CreateInfo& createInfo)
{
    return Driver::Create(createInfo);
}

void Swapchain::Destroy(const Swapchain& swapchain)
{
    Driver::Destroy(swapchain.Handle());
}

void Swapchain::DestroyImages(const Swapchain& swapchain)
{
    Driver::DestroySwapchainImages(swapchain);
}

u32 Swapchain::AcquireImage(u32 frameNumber)
{
    return RenderCommand::AcquireNextImage(*this, m_SwapchainFrameSync[frameNumber]);
}

bool Swapchain::PresentImage(const QueueInfo& queueInfo, u32 imageIndex, u32 frameNumber)
{
    return RenderCommand::Present(*this, queueInfo, m_SwapchainFrameSync[frameNumber], imageIndex);
}

void Swapchain::PreparePresent(const CommandBuffer& cmd, u32 imageIndex)
{
    ImageSubresource drawSubresource = m_DrawImage.CreateSubresource(0, 1, 0, 1);
    ImageSubresource presentSubresource = m_ColorImages[imageIndex].CreateSubresource(0, 1, 0, 1);
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

    ImageBlitInfo source = m_DrawImage.CreateImageBlitInfo(
        drawSubresource.Description.MipmapBase, drawSubresource.Description.LayerBase,
        drawSubresource.Description.Layers);
    ImageBlitInfo destination = m_ColorImages[imageIndex].CreateImageBlitInfo(
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
    return Driver::CreateSwapchainImages(*this);
}

Image Swapchain::CreateDrawImage()
{
    m_DrawImage = Image::Builder()
        .SetExtent(glm::uvec2{m_DrawResolution.x, m_DrawResolution.y})
        .SetFormat(m_DrawFormat)
        .SetUsage(ImageUsage::Source | ImageUsage::Destination | ImageUsage::Storage | ImageUsage::Color)
        .BuildManualLifetime();

    return m_DrawImage;
}

Image Swapchain::CreateDepthImage()
{
    Image depth = Image::Builder()
        .SetExtent(glm::uvec2{m_DrawResolution.x, m_DrawResolution.y})
        .SetFormat(m_DepthFormat)
        .SetUsage(ImageUsage::Depth | ImageUsage::Stencil | ImageUsage::Sampled)
        .BuildManualLifetime();

    return depth;
}

RenderingDetails Swapchain::GetRenderingDetails() const
{
    return {{m_DrawFormat}, m_DepthFormat};
}

