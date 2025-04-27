#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL

#include <vector>

#include "ResourceUploader.h"
#include "Core/Camera.h"
#include "RenderGraph/RenderGraph.h"
#include "FrameContext.h"
#include "RenderGraph/Passes/Scene/Visibility/SceneVisibilityPassesCommon.h"
#include "RenderGraph/Passes/SceneDraw/SceneDrawPassesCommon.h"
#include "Vulkan/Device.h"
#include "Rendering/Swapchain.h"
#include "Scene/Scene.h"
#include "Scene/ScenePass.h"
#include "Scene/SceneRenderObjectSet.h"
#include "Scene/Visibility/SceneMultiviewVisibility.h"

class SlimeMoldPass;
class SlimeMoldContext;
;
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

    RG::Resource RenderGraphDepthPrepass(const ScenePass& scenePass);
    SceneDrawPassDescription RenderGraphDepthPrepassDescription(RG::Resource& depth, const ScenePass& scenePass);
    SceneDrawPassDescription RenderGraphForwardPbrDescription(RG::Resource& color, RG::Resource& depth,
        const ScenePass& scenePass);

    SceneDrawPassDescription RenderGraphVBufferDescription(RG::Resource& vbuffer, RG::Resource& depth,
        const ScenePass& scenePass);
    RG::Resource RenderGraphVBufferPbr(RG::Resource& vbuffer, RG::Resource camera);
    
    void RenderGraphOnFrameDepthGenerated(StringId passName, RG::Resource depth);

    RG::Resource RenderGraphSSAO(StringId baseName, RG::Resource depth);
    
    struct TileLightsInfo
    {
        RG::Resource Tiles{};
        RG::Resource ZBins{};
    };
    TileLightsInfo RenderGraphCullLightsTiled(StringId baseName, RG::Resource depth);

    struct ClusterLightsInfo
    {
        RG::Resource Clusters{};
    };
    ClusterLightsInfo RenderGraphCullLightsClustered(StringId baseName, RG::Resource depth);

    RG::Resource RenderGraphSkyBox(RG::Resource color, RG::Resource depth);
    
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

    std::unique_ptr<RG::Graph> m_Graph;
    RG::Resource m_Ssao{};
    TileLightsInfo m_TileLightsInfo{};
    ClusterLightsInfo m_ClusterLightsInfo{};
    
    Texture m_SkyboxTexture{};
    Texture m_SkyboxPrefilterMap{};
    Texture m_BRDFLut{};
    Buffer m_IrradianceSH{};
    Buffer m_SkyIrradianceSH{};

    std::shared_ptr<SlimeMoldContext> m_SlimeMoldContext;

    SceneInfo* m_TestScene{nullptr};
    Scene m_Scene;
    SceneBucketList m_SceneBucketList;
    SceneRenderObjectSet m_OpaqueSet;
    SceneVisibilityHandle m_OpaqueSetPrimaryVisibility{};
    SceneView m_OpaqueSetPrimaryView{};

    SceneVisibilityHandle m_OpaqueSetShadowVisibility{};

    SceneMultiviewVisibility m_MultiviewVisibility{};
    SceneVisibilityPassesResources m_SceneVisibilityResources{};
    
    bool m_IsWindowResized{false};
    bool m_FrameEarlyExit{false};

    bool m_HasExecutedSingleTimePasses{false};
};
