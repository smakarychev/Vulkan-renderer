#pragma once
#include <array>
#include <vector>
#include <glm/vec2.hpp>
#include <vulkan/vulkan_core.h>

#include "DepthPyramid.h"
#include "Image.h"
#include "Syncronization.h"
#include "types.h"
#include "VulkanCommon.h"

class ShaderPipelineTemplate;
class Device;
struct DeviceQueues;
struct SurfaceDetails;

struct GLFWwindow;

struct SwapchainFrameSync
{
    Fence RenderFence;
    Semaphore RenderSemaphore;
    Semaphore PresentSemaphore;
};

static constexpr u32 INVALID_SWAPCHAIN_IMAGE = std::numeric_limits<u32>::max();

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
            VkSurfaceFormatKHR ColorFormat;
            VkFormat DepthStencilFormat;
            VkPresentModeKHR PresentMode;
            VkExtent2D Extent;
            u32 ImageCount;
            std::vector<SwapchainFrameSync> FrameSyncs;
            GLFWwindow* Window{nullptr};
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
        Swapchain BuildManualLifetime();
        Builder& DefaultHints();
        Builder& FromDetails(const SurfaceDetails& details);
        Builder& SetDevice(const Device& device);
        Builder& BufferedFrames(u32 count);
        Builder& SetSyncStructures(const std::vector<SwapchainFrameSync>& syncs);
    private:
        void PreBuild();
        static VkExtent2D ChooseExtent(const VkSurfaceCapabilitiesKHR& capabilities);
        static u32 ChooseImageCount(const VkSurfaceCapabilitiesKHR& capabilities);
        static VkFormat ChooseDepthFormat();
        std::vector<SwapchainFrameSync> CreateSynchronizationStructures();
    private:
        CreateInfo m_CreateInfo;
        CreateInfoHint m_CreateInfoHint;
        u32 m_BufferedFrames;
    };
public:
    static Swapchain Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const Swapchain& swapchain);
    
    u32 AcquireImage(u32 frameNumber);
    bool PresentImage(const QueueInfo& queueInfo, u32 imageIndex, u32 frameNumber);

    void PrepareRendering(const CommandBuffer& cmd, u32 imageIndex);
    void PreparePresent(const CommandBuffer& cmd, u32 imageIndex);

    const SwapchainFrameSync& GetFrameSync(u32 frameNumber) const;
    const std::vector<SwapchainFrameSync>& GetFrameSync() const;

    glm::uvec2 GetSize() const { return glm::uvec2{m_Extent.width, m_Extent.height}; }

    RenderingDetails GetRenderingDetails() const;
    
    const Image& GetColorImage(u32 index) const { return m_ColorImages[index]; }
    const Image& GetDepthImage() const { return m_DepthImage; }

private:
    std::vector<Image> CreateColorImages() const;
    Image CreateDepthImage();
    VkExtent2D GetValidExtent(const VkSurfaceCapabilitiesKHR& capabilities);
private:
    VkSwapchainKHR m_Swapchain{VK_NULL_HANDLE};
    VkExtent2D m_Extent{};
    VkFormat m_ColorFormat{};
    VkFormat m_DepthFormat{};
    std::vector<Image> m_ColorImages;
    Image m_DepthImage;
    u32 m_ColorImageCount{};
    std::vector<SwapchainFrameSync> m_SwapchainFrameSync;
    GLFWwindow* m_Window{nullptr};
};
