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
    m_CreateInfo.FrameSyncs = CreateSynchronizationStructures();
    Swapchain swapchain = Swapchain::Create(m_CreateInfo);
    Driver::s_DeletionQueue.AddDeleter([swapchain](){ Swapchain::Destroy(swapchain); });

    return swapchain;
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
    
    createInfo.Format = utils::getIntersectionOrDefault(m_CreateInfoHint.DesiredFormats, details.Formats,
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
    m_Device = &device;
    
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

std::vector<SwapchainFrameSync> Swapchain::Builder::CreateSynchronizationStructures()
{
    ASSERT(m_BufferedFrames != 0, "Buffered frames count is unset")
    std::vector<SwapchainFrameSync> swapchainFrameSyncs;
    swapchainFrameSyncs.reserve(m_BufferedFrames);
    for (u32 i = 0; i < m_BufferedFrames; i++)
    {
        Fence renderFence = Fence::Builder().
            SetDevice(*m_Device).
            StartSignaled(true).
            Build();
        Semaphore renderSemaphore = Semaphore::Builder().
            SetDevice(*m_Device).
            Build();
        Semaphore presentSemaphore = Semaphore::Builder().
            SetDevice(*m_Device).
            Build();

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
    
    VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.surface = createInfo.Surface;
    swapchainCreateInfo.imageColorSpace = createInfo.Format.colorSpace;
    swapchainCreateInfo.imageFormat = createInfo.Format.format;
    VkExtent2D extent = GetValidExtent(createInfo);
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

    VulkanCheck(vkCreateSwapchainKHR(createInfo.Device, &swapchainCreateInfo, nullptr, &swapchain.m_Swapchain),
        "Failed to create swapchain");
    swapchain.m_Device = createInfo.Device;
    swapchain.m_Extent = extent;
    swapchain.m_ColorFormat = createInfo.Format.format;
    swapchain.m_ImageCount = createInfo.ImageCount;
    swapchain.m_SwapchainFrameSync = createInfo.FrameSyncs;
    swapchain.m_ColorImages = swapchain.CreateColorImages(createInfo);

    return swapchain;
}

void Swapchain::Destroy(const Swapchain& swapchain)
{
    for (u32 i = 0; i < swapchain.m_ColorImages.size(); i++)
        vkDestroyImageView(swapchain.m_Device, swapchain.m_ColorImages[i].View, nullptr);
    vkDestroySwapchainKHR(swapchain.m_Device, swapchain.m_Swapchain, nullptr);
}

u32 Swapchain::AcquireImage()
{
    // todo: fix for multiple buffered frames
    VulkanCheck(RenderCommand::WaitForFence(m_SwapchainFrameSync.front().RenderFence),
        "Error while waiting for fences");
    VulkanCheck(RenderCommand::ResetFence(m_SwapchainFrameSync.front().RenderFence),
        "Error while resetting fences");
    u32 imageIndex;
    VulkanCheck(RenderCommand::AcquireNextImage(*this, m_SwapchainFrameSync.front().PresentSemaphore, imageIndex),
        "Failed to acquire new swapchain image");

    return imageIndex;
}

void Swapchain::PresentImage(const QueueInfo& queueInfo, u32 imageIndex)
{
    VulkanCheck(RenderCommand::Present(*this, queueInfo, m_SwapchainFrameSync.front().RenderSemaphore, imageIndex),
        "Failed to present image");
}

const SwapchainFrameSync& Swapchain::GetFrameSync() const
{
    // todo: fix for multiple buffered frames
    return m_SwapchainFrameSync.front();
}

std::vector<ImageData> Swapchain::CreateColorImages(const CreateInfo& createInfo) const
{
    u32 imageCount = 0;
    vkGetSwapchainImagesKHR(createInfo.Device, m_Swapchain, &imageCount, nullptr);
    std::vector<VkImage> images(imageCount);
    vkGetSwapchainImagesKHR(createInfo.Device, m_Swapchain, &imageCount, images.data());

    std::vector<VkImageView> imageViews(imageCount);
    for (u32 i = 0; i < imageCount; i++)
        imageViews[i] = vkUtils::createImageView(createInfo.Device, images[i], createInfo.Format.format, VK_IMAGE_ASPECT_COLOR_BIT, 1);

    std::vector<ImageData> colorImages(imageCount);
    for (u32 i = 0; i < imageCount; i++)
        colorImages[i] = {
            .Image = images[i],
            .View = imageViews[i],
            .Width = m_Extent.width,
            .Height = m_Extent.height};

    return colorImages;
}

VkExtent2D Swapchain::GetValidExtent(const CreateInfo& createInfo)
{
    VkExtent2D extent = createInfo.Extent;

    if (extent.width != std::numeric_limits<u32>::max())
        return extent;

    // indication that extent might not be same as window size
    i32 windowWidth, windowHeight;
    glfwGetFramebufferSize(createInfo.Window, &windowWidth, &windowHeight);

    const VkSurfaceCapabilitiesKHR& capabilities = createInfo.Capabilities;
    
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

    return {color};
}

std::vector<Attachment> Swapchain::GetAttachments(u32 imageIndex) const
{
    Attachment color = Attachment::Builder().
        SetType(AttachmentType::Color).
        FromImageData(m_ColorImages[imageIndex]).
        Build();

    return {color};
}

std::vector<Framebuffer> Swapchain::GetFramebuffers(const RenderPass& renderPass) const
{
    std::vector<Framebuffer> framebuffers;
    framebuffers.reserve(m_ImageCount);

    for (u32 i = 0; i < m_ImageCount; i++)
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

