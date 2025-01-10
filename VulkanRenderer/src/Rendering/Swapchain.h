#pragma once

#include <vector>
#include <glm/vec2.hpp>

#include "Image/Image.h"
#include "Synchronization.h"
#include "types.h"

class QueueInfo;
class ShaderPipelineTemplate;
struct DeviceQueues;

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
            bool UseDefaultHint{true};
            glm::uvec2 DrawResolution;
            Format DrawFormat;
            Format DepthStencilFormat;
            std::vector<SwapchainFrameSync> FrameSyncs;
        };
    public:
        Swapchain Build();
        Swapchain BuildManualLifetime();
        Builder& SetDrawResolution(const glm::uvec2& resolution);
        Builder& BufferedFrames(u32 count);
        Builder& SetSyncStructures(const std::vector<SwapchainFrameSync>& syncs);
    private:
        void PreBuild();
        static Format ChooseDepthFormat();
        std::vector<SwapchainFrameSync> CreateSynchronizationStructures();
    private:
        CreateInfo m_CreateInfo;
        u32 m_BufferedFrames;
    };
public:
    static Swapchain Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const Swapchain& swapchain);
    static void DestroyImages(const Swapchain& swapchain);
    
    u32 AcquireImage(u32 frameNumber);
    bool PresentImage(QueueKind queueKind, u32 imageIndex, u32 frameNumber);

    void PreparePresent(const CommandBuffer& cmd, u32 imageIndex);

    const SwapchainFrameSync& GetFrameSync(u32 frameNumber) const;
    const std::vector<SwapchainFrameSync>& GetFrameSync() const;

    glm::uvec2 GetResolution() const { return m_SwapchainResolution; }
    glm::uvec2 GetDrawResolution() const { return m_DrawResolution; }

    const Image& GetDrawImage() const { return m_DrawImage; }
    const Image& GetDepthImage() const { return m_DepthImage; }
private:
    std::vector<Image> CreateColorImages() const;
    Image CreateDrawImage();
    Image CreateDepthImage();
    ResourceHandleType<Swapchain> Handle() const { return m_ResourceHandle; }
private:
    glm::uvec2 m_SwapchainResolution;
    glm::uvec2 m_DrawResolution;
    Format m_DrawFormat{};
    Format m_DepthFormat{};
    std::vector<Image> m_ColorImages;
    Image m_DrawImage;
    Image m_DepthImage;
    u32 m_ColorImageCount{};
    std::vector<SwapchainFrameSync> m_SwapchainFrameSync;
    GLFWwindow* m_Window{nullptr};
    
    ResourceHandleType<Swapchain> m_ResourceHandle{};
};
