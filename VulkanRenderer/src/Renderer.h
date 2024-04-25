#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL

#include <vector>

#include "ResourceUploader.h"
#include "Core/Camera.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGGeometry.h"
#include "FrameContext.h"
#include "Light/SceneLight.h"
#include "Vulkan/Driver.h"
#include "Rendering/Swapchain.h"

class CSMVisualizePass;
class CSMPass;
class VisualizeDepthPass;
class DirectionalShadowPass;
class PbrForwardTranslucentIBLPass;
class PbrTCForwardIBLPass;
class VisibilityPass;
class VisualizeBRDFPass;
class SkyboxPass;
class SsaoPass;
class SsaoBlurPass;
class SsaoVisualizePass;
class PbrVisibilityBufferIBL;
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

    GLFWwindow* GetWindow() { return m_Window; }
private:
    Renderer();
    void InitRenderingStructures();
    void InitRenderGraph();
    void SetupRenderSlimePasses();
    void SetupRenderGraph();

    void Shutdown();

    RenderingInfo GetImGuiUIRenderingInfo();

    void ProcessPendingCubemaps();
    void ProcessPendingPBRTextures();

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
    RG::Geometry m_GraphOpaqueGeometry;
    RG::Geometry m_GraphTranslucentGeometry;

    SceneLight m_SceneLights{};
    
    std::unique_ptr<RG::Graph> m_Graph;
    std::shared_ptr<SkyGradientPass> m_SkyGradientPass;
    std::shared_ptr<CrtPass> m_CrtPass;
    
    std::shared_ptr<HiZVisualize> m_HiZVisualizePass;
    std::shared_ptr<CopyTexturePass> m_CopyTexturePass;
    std::shared_ptr<BlitPass> m_BlitPartialDraw;
    std::shared_ptr<BlitPass> m_BlitHiZ;

    std::shared_ptr<VisibilityPass> m_VisibilityPass;
    std::shared_ptr<PbrVisibilityBufferIBL> m_PbrVisibilityBufferIBLPass;
    std::shared_ptr<PbrForwardTranslucentIBLPass> m_PbrForwardIBLTranslucentPass;
    std::shared_ptr<SsaoPass> m_SsaoPass;
    std::shared_ptr<SsaoBlurPass> m_SsaoBlurHorizontalPass;
    std::shared_ptr<SsaoBlurPass> m_SsaoBlurVerticalPass;
    std::shared_ptr<SsaoVisualizePass> m_SsaoVisualizePass;
    
    std::shared_ptr<VisualizeBRDFPass> m_VisualizeBRDFPass;
    

    Texture m_SkyboxTexture{};
    Texture m_SkyboxIrradianceMap{};
    Texture m_SkyboxPrefilterMap{};
    std::shared_ptr<Texture> m_BRDF{};
    std::shared_ptr<SkyboxPass> m_SkyboxPass;

    std::shared_ptr<CSMPass> m_CSMPass;
    std::shared_ptr<CSMVisualizePass> m_CSMVisualizePass;
    std::shared_ptr<BlitPass> m_BlitCSM;

    
    std::shared_ptr<SlimeMoldContext> m_SlimeMoldContext;
    std::shared_ptr<SlimeMoldPass> m_SlimeMoldPass;

    bool m_IsWindowResized{false};
    bool m_FrameEarlyExit{false};
};
