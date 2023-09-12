#include "Swapchain.h"

#include "Attachment.h"
#include "Device.h"
#include "Driver.h"
#include "RenderCommand.h"
#include "utils.h"
#include "VulkanUtils.h"
#include "GLFW/glfw3.h"

Swapchain Swapchain::Builder::Build()
{
    m_CreateInfo.DepthStencilFormat = ChooseDepthFormat();
    m_CreateInfo.FrameSyncs = CreateSynchronizationStructures();
    Swapchain swapchain = Swapchain::Create(m_CreateInfo);
    Driver::DeletionQueue().AddDeleter([swapchain](){ Swapchain::Destroy(swapchain); });

    return swapchain;
}

Swapchain Swapchain::Builder::BuildManualLifetime()
{
    m_CreateInfo.DepthStencilFormat = ChooseDepthFormat();
    m_CreateInfo.FrameSyncs = CreateSynchronizationStructures();
    return Swapchain::Create(m_CreateInfo);
}

Swapchain::Builder& Swapchain::Builder::DefaultHints()
{
    CreateInfoHint createInfoHint = {};

    createInfoHint.DesiredFormats = {{{.format = VK_FORMAT_B8G8R8A8_SRGB, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}}};

    createInfoHint.DesiredPresentModes = {{VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_FIFO_RELAXED_KHR}};

    m_CreateInfoHint = createInfoHint;
    
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
    
    createInfo.Extent = ChooseExtent(details.Capabilities);

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

VkFormat Swapchain::Builder::ChooseDepthFormat()
{
    return VK_FORMAT_D32_SFLOAT;
}

std::vector<SwapchainFrameSync> Swapchain::Builder::CreateSynchronizationStructures()
{
    ASSERT(m_BufferedFrames != 0, "Buffered frames count is unset")
    std::vector<SwapchainFrameSync> swapchainFrameSyncs;
    swapchainFrameSyncs.reserve(m_BufferedFrames);
    for (u32 i = 0; i < m_BufferedFrames; i++)
    {
        Fence renderFence = Fence::Builder().
            StartSignaled(true).Build();
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
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
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
    swapchain.m_ColorFormat = createInfo.ColorFormat.format;
    swapchain.m_DepthFormat = createInfo.DepthStencilFormat;
    swapchain.m_ColorImageCount = createInfo.ImageCount;
    swapchain.m_SwapchainFrameSync = createInfo.FrameSyncs;
    swapchain.m_ColorImages = swapchain.CreateColorImages();
    swapchain.m_DepthImage = swapchain.CreateDepthImage();

    return swapchain;
}

void Swapchain::Destroy(const Swapchain& swapchain)
{
    for (const auto& colorImage : swapchain.m_ColorImages)
        vkDestroyImageView(Driver::DeviceHandle(), colorImage.GetImageData().View, nullptr);
    vkDestroySwapchainKHR(Driver::DeviceHandle(), swapchain.m_Swapchain, nullptr);
}

u32 Swapchain::AcquireImage(u32 frameNumber)
{
    VulkanCheck(RenderCommand::WaitForFence(m_SwapchainFrameSync[frameNumber].RenderFence),
        "Error while waiting for fences");
    VulkanCheck(RenderCommand::ResetFence(m_SwapchainFrameSync[frameNumber].RenderFence),
        "Error while resetting fences");
    u32 imageIndex;
    VulkanCheck(RenderCommand::AcquireNextImage(*this, m_SwapchainFrameSync[frameNumber], imageIndex),
        "Failed to acquire new swapchain image");

    return imageIndex;
}

void Swapchain::PresentImage(const QueueInfo& queueInfo, u32 imageIndex, u32 frameNumber)
{
    VulkanCheck(RenderCommand::Present(*this, queueInfo, m_SwapchainFrameSync[frameNumber], imageIndex),
        "Failed to present image");
}

const SwapchainFrameSync& Swapchain::GetFrameSync(u32 frameNumber) const
{
    return m_SwapchainFrameSync[frameNumber];
}

std::vector<Image> Swapchain::CreateColorImages() const
{
    u32 imageCount = 0;
    vkGetSwapchainImagesKHR(Driver::DeviceHandle(), m_Swapchain, &imageCount, nullptr);
    std::vector<VkImage> images(imageCount);
    vkGetSwapchainImagesKHR(Driver::DeviceHandle(), m_Swapchain, &imageCount, images.data());

    std::vector<VkImageView> imageViews(imageCount);
    for (u32 i = 0; i < imageCount; i++)
        imageViews[i] = vkUtils::createImageView(Driver::DeviceHandle(), images[i], m_ColorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);

    std::vector<Image> colorImages(imageCount);
    for (u32 i = 0; i < imageCount; i++)
    {
        ImageData imageData = {
            .Image = images[i],
            .View = imageViews[i],
            .Width = m_Extent.width,
            .Height = m_Extent.height};
    
        colorImages[i] = Image::Builder().
            FromImageData(imageData).
            Build();
    }

    return colorImages;
}

Image Swapchain::CreateDepthImage()
{
    Image depth = Image::Builder().
        SetExtent(m_Extent).
        SetFormat(m_DepthFormat).
        SetUsage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT).
        Build();

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

std::vector<AttachmentTemplate> Swapchain::GetAttachmentTemplates() const
{
    AttachmentTemplate color = AttachmentTemplate::Builder().
        PresentationDefaults().
        SetFormat(m_ColorFormat).
        Build();

    AttachmentTemplate depth = AttachmentTemplate::Builder().
        DepthDefaults().
        SetFormat(m_DepthFormat).
        Build();

    return {color, depth};
}

std::vector<Attachment> Swapchain::GetAttachments(u32 imageIndex) const
{
    Attachment color = Attachment::Builder().
        SetType(AttachmentType::Color).
        FromImageData(m_ColorImages[imageIndex].GetImageData()).
        Build();

    Attachment depth = Attachment::Builder().
        SetType(AttachmentType::DepthStencil).
        FromImageData(m_DepthImage.GetImageData()).
        Build();

    return {color, depth};
}

std::vector<Framebuffer> Swapchain::GetFramebuffers(const RenderPass& renderPass) const
{
    std::vector<Framebuffer> framebuffers;
    framebuffers.reserve(m_ColorImageCount);

    for (u32 i = 0; i < m_ColorImageCount; i++)
    {
        std::vector<Attachment> attachments = GetAttachments(i);
        Framebuffer framebuffer = Framebuffer::Builder().
            SetRenderPass(renderPass).
            SetAttachments(attachments).
            Build();

        framebuffers.push_back(framebuffer);
    }

    return framebuffers;
}

