#pragma once
#include <vector>
#include <glm/vec2.hpp>
#include <vulkan/vulkan_core.h>

#include "Attachment.h"
#include "Framebuffer.h"
#include "Syncronization.h"
#include "types.h"
#include "VulkanCommon.h"

class Device;
class RenderPass;
struct DeviceQueues;
struct SurfaceDetails;

struct GLFWwindow;

struct SwapchainFrameSync
{
    Fence RenderFence;
    Semaphore RenderSemaphore;
    Semaphore PresentSemaphore;
};

class Swapchain
{
    FRIEND_INTERNAL
public:
    class Builder
    {
        friend class Swapchain;
        FRIEND_INTERNAL
        struct CreateInfo
        {
            VkSurfaceCapabilitiesKHR Capabilities;
            VkSurfaceFormatKHR Format;
            VkPresentModeKHR PresentMode;
            VkExtent2D Extent;
            u32 ImageCount;
            std::vector<SwapchainFrameSync> FrameSyncs;
            GLFWwindow* Window{nullptr};
            VkDevice Device{VK_NULL_HANDLE};
            VkSurfaceKHR Surface{VK_NULL_HANDLE};
            const DeviceQueues* Queues{nullptr};
        };
        struct CreateInfoHint
        {
            std::vector<VkSurfaceFormatKHR> DesiredFormats;
            std::vector<VkPresentModeKHR> DesiredPresentModes;
        };
    public:
        Swapchain Build();
        Builder& DefaultHints();
        Builder& FromDetails(const SurfaceDetails& details);
        Builder& SetDevice(const Device& device);
        Builder& BufferedFrames(u32 count);
    private:
        static VkExtent2D ChooseExtent(const VkSurfaceCapabilitiesKHR& capabilities);
        static u32 ChooseImageCount(const VkSurfaceCapabilitiesKHR& capabilities);
        std::vector<SwapchainFrameSync> CreateSynchronizationStructures();
    private:
        CreateInfo m_CreateInfo;
        CreateInfoHint m_CreateInfoHint;
        u32 m_BufferedFrames;
        const Device* m_Device;
    };
public:
    static Swapchain Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const Swapchain& swapchain);

    u32 AcquireImage();
    void PresentImage(const QueueInfo& queueInfo, u32 imageIndex);

    const SwapchainFrameSync& GetFrameSync() const;

    std::vector<AttachmentTemplate> GetAttachmentTemplates() const;
    std::vector<Attachment> GetAttachments(u32 imageIndex) const;
    std::vector<Framebuffer> GetFramebuffers(const RenderPass& renderPass) const;

    glm::uvec2 GetSize() const { return glm::uvec2{m_Extent.width, m_Extent.height}; }
private:
    using CreateInfo = Builder::CreateInfo;
    std::vector<ImageData> CreateColorImages(const CreateInfo& createInfo) const;
    static VkExtent2D GetValidExtent(const CreateInfo& createInfo);
private:
    VkSwapchainKHR m_Swapchain{VK_NULL_HANDLE};
    VkExtent2D m_Extent{};
    VkFormat m_ColorFormat{};
    std::vector<ImageData> m_ColorImages;
    u32 m_ImageCount{};
    std::vector<SwapchainFrameSync> m_SwapchainFrameSync;
    VkDevice m_Device{VK_NULL_HANDLE};
};
