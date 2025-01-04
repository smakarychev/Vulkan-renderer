#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL

#include <vector>

#include "ResourceUploader.h"
#include "Core/Camera.h"
#include "RenderGraph/RenderGraph.h"
#include "Scene/SceneGeometry.h"
#include "FrameContext.h"
#include "Light/SceneLight.h"
#include "Vulkan/Driver.h"
#include "Rendering/Swapchain.h"

class SlimeMoldPass;
class SlimeMoldContext;
class HiZPassContext;
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
    void ExecuteSingleTimePasses();
    void SetupRenderSlimePasses();
    void SetupRenderGraph();

    void UpdateLights();

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

    Swapchain m_Swapchain;
    
    u64 m_FrameNumber{0};
    u32 m_SwapchainImageIndex{0};

    std::vector<FrameContext> m_FrameContexts;
    FrameContext* m_CurrentFrameContext{nullptr};
    
    ResourceUploader m_ResourceUploader;

    std::unique_ptr<BindlessTextureDescriptorsRingBuffer> m_BindlessTextureDescriptorsRingBuffer;
    ModelCollection m_GraphModelCollection;
    SceneGeometry m_GraphOpaqueGeometry;
    SceneGeometry m_GraphTranslucentGeometry;

    std::unique_ptr<SceneLight> m_SceneLights{};
    
    std::unique_ptr<RG::Graph> m_Graph;
    
    Texture m_SkyboxTexture{};
    Texture m_SkyboxPrefilterMap{};
    Texture m_BRDFLut{};
    Buffer m_IrradianceSH{};
    Buffer m_SkyIrradianceSH{};

    std::shared_ptr<SlimeMoldContext> m_SlimeMoldContext;
    std::shared_ptr<SlimeMoldPass> m_SlimeMoldPass;

    bool m_IsWindowResized{false};
    bool m_FrameEarlyExit{false};

    bool m_HasExecutedSingleTimePasses{false};
};
