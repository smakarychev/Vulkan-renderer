#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL

#include <vector>

#include "ResourceUploader.h"
#include "Core/Camera.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RenderPassGeometry.h"
#include "FrameContext.h"
#include "RenderGraph/Culling/CullMetaPass.h"
#include "Vulkan/Driver.h"
#include "Rendering/Swapchain.h"

class SsaoVisualizePass;
class SsaoPSPass;
class PbrVisibilityBuffer;
class CullMetaPass;
class CopyTexturePass;
class SlimeMoldPass;
class SlimeMoldContext;
class HiZPassContext;
class SkyGradientPass;
class BlitPass;
class HiZVisualize;
class HiZPass;
class CrtPass;
class Camera;
class CameraController;

class Renderer
{
public:
    void Init();
    static Renderer* Get(); 
    ~Renderer();

    void Run();
    void OnRender();
    void OnUpdate();

    void BeginFrame();
    void EndFrame();

    template <typename Fn>
    void ImmediateUpload(Fn&& uploadFunction) const;

    GLFWwindow* GetWindow() { return m_Window; }
private:
    Renderer();
    void InitRenderingStructures();
    void InitRenderGraph();
    void SetupRenderSlimePasses();
    CullMetaPass::PassData SetupVisibilityBufferPass();
    void SetupRenderGraph();

    void Shutdown();

    RenderingInfo GetImGuiUIRenderingInfo();

    void OnWindowResize();
    void RecreateSwapchain();
    
    const FrameContext& GetFrameContext() const;
    FrameContext& GetFrameContext();
private:
    GLFWwindow* m_Window;
    std::unique_ptr<CameraController> m_CameraController;
    std::shared_ptr<Camera> m_Camera;

    Device m_Device;
    Swapchain m_Swapchain;
    
    u64 m_FrameNumber{0};
    u32 m_SwapchainImageIndex{0};

    std::vector<FrameContext> m_FrameContexts;
    FrameContext* m_CurrentFrameContext{nullptr};
    
    ResourceUploader m_ResourceUploader;

    ModelCollection m_GraphModelCollection;
    RenderPassGeometry m_GraphOpaqueGeometry;
    
    std::unique_ptr<RenderGraph::Graph> m_Graph;
    std::shared_ptr<SkyGradientPass> m_SkyGradientPass;
    std::shared_ptr<CrtPass> m_CrtPass;
    
    std::shared_ptr<HiZVisualize> m_HiZVisualizePass;
    std::shared_ptr<CopyTexturePass> m_CopyTexturePass;
    std::shared_ptr<BlitPass> m_BlitPartialDraw;
    std::shared_ptr<BlitPass> m_BlitHiZ;

    std::shared_ptr<CullMetaPass> m_TriangleCull;
    // todo: rename once working
    std::shared_ptr<CullMetaPass> m_VisibilityBufferPass;
    std::shared_ptr<PbrVisibilityBuffer> m_PbrVisibilityBufferPass;
    std::shared_ptr<SsaoPSPass> m_SsaoPsPass;
    std::shared_ptr<SsaoVisualizePass> m_SsaoVisualizePass;

    std::shared_ptr<SlimeMoldContext> m_SlimeMoldContext;
    std::shared_ptr<SlimeMoldPass> m_SlimeMoldPass;

    bool m_IsWindowResized{false};
    bool m_FrameEarlyExit{false};
};

template <typename Fn>
void Renderer::ImmediateUpload(Fn&& uploadFunction) const
{
    Driver::ImmediateUpload(uploadFunction);
}
