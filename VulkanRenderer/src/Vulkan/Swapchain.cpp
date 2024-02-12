#include "Swapchain.h"

#include "Device.h"
#include "Driver.h"
#include "RenderCommand.h"
#include "utils/utils.h"
#include "VulkanUtils.h"
#include "GLFW/glfw3.h"

Swapchain Swapchain::Builder::Build()
{
    PreBuild();
    
    Swapchain swapchain = Swapchain::Create(m_CreateInfo);
    Driver::DeletionQueue().AddDeleter([swapchain](){ Swapchain::Destroy(swapchain); });

    return swapchain;
}

Swapchain Swapchain::Builder::BuildManualLifetime()
{
    PreBuild();
    
    return Swapchain::Create(m_CreateInfo);
}

Swapchain::Builder& Swapchain::Builder::DefaultHints()
{
    CreateInfoHint createInfoHint = {};

    createInfoHint.DesiredFormats = {{{.format = VK_FORMAT_B8G8R8A8_SRGB, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}}};

    createInfoHint.DesiredPresentModes = {{VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_RELAXED_KHR}};

    m_CreateInfoHint = createInfoHint;
    
    return *this;
}

Swapchain::Builder& Swapchain::Builder::SetDrawResolution(const glm::uvec2& resolution)
{
    ASSERT(resolution.x != 0 && resolution.y != 0, "Draw resolution must be greater than zero")
    m_CreateInfo.DrawExtent = {resolution.x, resolution.y};

    return *this;
}

Swapchain::Builder& Swapchain::Builder::FromDetails(const SurfaceDetails& details)
{
    CreateInfo createInfo = {};

    createInfo.Capabilities = details.Capabilities;
    
    createInfo.ColorFormat = utils::getIntersectionOrDefault(m_CreateInfoHint.DesiredFormats, details.Formats,
        [](VkSurfaceFormatKHR des, VkSurfaceFormatKHR avail) { return des.format == avail.format && des.colorSpace == avail.colorSpace; });

    createInfo.PresentMode = utils::getIntersectionOrDefault(m_CreateInfoHint.DesiredPresentModes, details.PresentModes,
        [](VkPresentModeKHR des, VkPresentModeKHR avail) { return des == avail; });
    
    createInfo.ImageCount = ChooseImageCount(details.Capabilities);

    m_CreateInfo = createInfo;
    
    return *this;
}

Swapchain::Builder& Swapchain::Builder::SetDevice(const Device& device)
{
    Driver::Unpack(device, m_CreateInfo);
    
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
    m_CreateInfo.DrawFormat = ImageFormat::RGBA16_FLOAT;
    m_CreateInfo.DepthStencilFormat = ChooseDepthFormat();
    
    if (m_CreateInfo.FrameSyncs.empty())
        m_CreateInfo.FrameSyncs = CreateSynchronizationStructures();
}

VkExtent2D Swapchain::Builder::ChooseExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
    return capabilities.currentExtent;
}

u32 Swapchain::Builder::ChooseImageCount(const VkSurfaceCapabilitiesKHR& capabilities)
{
    if (capabilities.maxImageCount == 0)
        return capabilities.minImageCount + 1;

    return std::min(capabilities.minImageCount + 1, capabilities.maxImageCount); 
}

ImageFormat Swapchain::Builder::ChooseDepthFormat()
{
    return ImageFormat::D32_FLOAT;
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
    Swapchain swapchain = {};

    swapchain.m_Window = createInfo.Window;
    
    VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.surface = createInfo.Surface;
    swapchainCreateInfo.imageColorSpace = createInfo.ColorFormat.colorSpace;
    swapchainCreateInfo.imageFormat = createInfo.ColorFormat.format;
    VkExtent2D extent = swapchain.GetValidExtent(createInfo.Capabilities);
    swapchainCreateInfo.imageExtent = extent;
    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapchainCreateInfo.minImageCount = createInfo.ImageCount;
    swapchainCreateInfo.presentMode = createInfo.PresentMode;

    if (createInfo.Queues->Graphics.Family == createInfo.Queues->Presentation.Family)
    {
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    else
    {
        std::vector<u32> queueFamilies = createInfo.Queues->AsFamilySet();
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchainCreateInfo.queueFamilyIndexCount = (u32)queueFamilies.size();
        swapchainCreateInfo.pQueueFamilyIndices = queueFamilies.data();
    }
    swapchainCreateInfo.preTransform = createInfo.Capabilities.currentTransform;
    swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainCreateInfo.clipped = VK_TRUE;
    swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

    VulkanCheck(vkCreateSwapchainKHR(Driver::DeviceHandle(), &swapchainCreateInfo, nullptr, &swapchain.m_Swapchain),
        "Failed to create swapchain");
    swapchain.m_Extent = extent;
    swapchain.m_DrawExtent = createInfo.DrawExtent.width != 0 ? createInfo.DrawExtent : extent;
    swapchain.m_ColorFormat = createInfo.ColorFormat.format;
    swapchain.m_DrawFormat = createInfo.DrawFormat;
    swapchain.m_DepthFormat = createInfo.DepthStencilFormat;
    swapchain.m_ColorImageCount = createInfo.ImageCount;
    swapchain.m_SwapchainFrameSync = createInfo.FrameSyncs;
    swapchain.m_ColorImages = swapchain.CreateColorImages();
    swapchain.m_DrawImage = swapchain.CreateDrawImage();
    swapchain.m_DepthImage = swapchain.CreateDepthImage();

    return swapchain;
}

void Swapchain::Destroy(const Swapchain& swapchain)
{
    Driver::DestroySwapchainImages(swapchain);
    Image::Destroy(swapchain.m_DrawImage);
    Image::Destroy(swapchain.m_DepthImage);
    vkDestroySwapchainKHR(Driver::DeviceHandle(), swapchain.m_Swapchain, nullptr);
}

u32 Swapchain::AcquireImage(u32 frameNumber)
{
    VulkanCheck(RenderCommand::WaitForFence(m_SwapchainFrameSync[frameNumber].RenderFence),
        "Error while waiting for fences");

    u32 imageIndex;
    
    VkResult res = RenderCommand::AcquireNextImage(*this, m_SwapchainFrameSync[frameNumber], imageIndex);
    if (res == VK_ERROR_OUT_OF_DATE_KHR)
        return INVALID_SWAPCHAIN_IMAGE;
    
    ASSERT(res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR, "Failed to acquire swapchain image")

    VulkanCheck(RenderCommand::ResetFence(m_SwapchainFrameSync[frameNumber].RenderFence),
        "Error while resetting fences");
    
    return imageIndex;
}

bool Swapchain::PresentImage(const QueueInfo& queueInfo, u32 imageIndex, u32 frameNumber)
{
    VkResult res = RenderCommand::Present(*this, queueInfo, m_SwapchainFrameSync[frameNumber], imageIndex);

    ASSERT(res == VK_SUCCESS || res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR, "Failed to present image")

    return res == VK_SUCCESS;
}

void Swapchain::PrepareRendering(const CommandBuffer& cmd)
{
    ImageSubresource drawSubresource = m_DrawImage.CreateSubresource();
    ImageSubresource depthSubresource = m_DepthImage.CreateSubresource();
    Barrier barrier = {};
    
    DependencyInfo drawTransition = DependencyInfo::Builder()
        .LayoutTransition({
            .ImageSubresource = &drawSubresource,
            .SourceStage = PipelineStage::Top,
            .DestinationStage = PipelineStage::ColorOutput,
            .SourceAccess = PipelineAccess::None,
            .DestinationAccess = PipelineAccess::ReadColorAttachment | PipelineAccess::WriteColorAttachment,
            .OldLayout = ImageLayout::Undefined,
            .NewLayout = ImageLayout::ColorAttachment})
        .Build();
    
    DependencyInfo depthTransition = DependencyInfo::Builder()
        .LayoutTransition({
            .ImageSubresource = &depthSubresource,
            .SourceStage = PipelineStage::Top,
            .DestinationStage = PipelineStage::DepthEarly | PipelineStage::DepthLate,
            .SourceAccess = PipelineAccess::None,
            .DestinationAccess = PipelineAccess::ReadDepthStencilAttachment | PipelineAccess::WriteDepthStencilAttachment,
            .OldLayout = ImageLayout::Undefined,
            .NewLayout = ImageLayout::DepthAttachment})
        .Build();

    barrier.Wait(cmd, drawTransition);
    barrier.Wait(cmd, depthTransition);
}

void Swapchain::PreparePresent(const CommandBuffer& cmd, u32 imageIndex)
{
    ImageSubresource drawSubresource = m_DrawImage.CreateSubresource(0, 1, 0, 1);
    ImageSubresource presentSubresource = m_ColorImages[imageIndex].CreateSubresource(0, 1, 0, 1);
    Barrier barrier = {};

    LayoutTransitionInfo drawToSourceTransitionInfo = {
        .ImageSubresource = &drawSubresource,
        .SourceStage = PipelineStage::ColorOutput,
        .DestinationStage = PipelineStage::Bottom,
        .SourceAccess = PipelineAccess::ReadColorAttachment | PipelineAccess::WriteColorAttachment,
        .DestinationAccess = PipelineAccess::None,
        .OldLayout = ImageLayout::ColorAttachment,
        .NewLayout = ImageLayout::Source};
    
    LayoutTransitionInfo presentToDestinationTransitionInfo = drawToSourceTransitionInfo;
    presentToDestinationTransitionInfo.ImageSubresource = &presentSubresource;
    presentToDestinationTransitionInfo.OldLayout = ImageLayout::Undefined;
    presentToDestinationTransitionInfo.NewLayout = ImageLayout::Destination;

    LayoutTransitionInfo destinationToPresentTransitionInfo = presentToDestinationTransitionInfo;
    destinationToPresentTransitionInfo.OldLayout = ImageLayout::Destination;
    destinationToPresentTransitionInfo.NewLayout = ImageLayout::Present;

    
    DependencyInfo drawToSourceTransition = DependencyInfo::Builder()
        .LayoutTransition(drawToSourceTransitionInfo)
        .Build();
    DependencyInfo presentToDestinationTransition = DependencyInfo::Builder()
        .LayoutTransition(presentToDestinationTransitionInfo)
        .Build();
    DependencyInfo destinationToPresentTransition = DependencyInfo::Builder()
        .LayoutTransition(destinationToPresentTransitionInfo)
        .Build();

    barrier.Wait(cmd, drawToSourceTransition);
    barrier.Wait(cmd, presentToDestinationTransition);

    ImageBlitInfo source = m_DrawImage.CreateImageBlitInfo(
        drawSubresource.MipmapBase, drawSubresource.LayerBase, drawSubresource.Layers);
    ImageBlitInfo destination = m_ColorImages[imageIndex].CreateImageBlitInfo(
        presentSubresource.MipmapBase, presentSubresource.LayerBase, presentSubresource.Layers);
    
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
        .SetExtent(glm::uvec2{m_DrawExtent.width, m_DrawExtent.height})
        .SetFormat(m_DrawFormat)
        .SetUsage(ImageUsage::Source | ImageUsage::Destination | ImageUsage::Storage | ImageUsage::Color)
        .BuildManualLifetime();

    return m_DrawImage;
}

Image Swapchain::CreateDepthImage()
{
    Image depth = Image::Builder()
        .SetExtent(glm::uvec2{m_Extent.width, m_Extent.height})
        .SetFormat(m_DepthFormat)
        .SetUsage(ImageUsage::Depth | ImageUsage::Stencil | ImageUsage::Sampled)
        .BuildManualLifetime();

    return depth;
}

VkExtent2D Swapchain::GetValidExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
    VkExtent2D extent = capabilities.currentExtent;

    if (extent.width != std::numeric_limits<u32>::max())
        return extent;

    // indication that extent might not be same as window size
    i32 windowWidth, windowHeight;
    glfwGetFramebufferSize(m_Window, &windowWidth, &windowHeight);
    
    extent.width = std::clamp(windowWidth, (i32)capabilities.minImageExtent.width, (i32)capabilities.maxImageExtent.width);
    extent.height = std::clamp(windowHeight, (i32)capabilities.minImageExtent.height, (i32)capabilities.maxImageExtent.height);

    return extent;
}

RenderingDetails Swapchain::GetRenderingDetails() const
{
    return {{m_DrawFormat}, m_DepthFormat};
}

