#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL

#include <vector>

#include "ResourceUploader.h"
#include "Core/Camera.h"
#include "RenderGraph/RGGraph.h"
#include "FrameContext.h"
#include "RenderGraph/Passes/Clouds/CloudsCommon.h"
#include "RenderGraph/Passes/Clouds/CloudsPass.h"
#include "RenderGraph/Passes/Scene/Visibility/SceneVisibilityPassesCommon.h"
#include "RenderGraph/Passes/SceneDraw/SceneDrawPassesCommon.h"
#include "RenderGraph/Visualization/RGMermaidExporter.h"
#include "Vulkan/Device.h"
#include "Rendering/Swapchain.h"
#include "Rendering/Shader/ShaderCache.h"
#include "Scene/BindlessTextureDescriptorsRingBuffer.h"
#include "Scene/Scene.h"
#include "Scene/ScenePass.h"
#include "Scene/SceneRenderObjectSet.h"
#include "Scene/Visibility/SceneMultiviewVisibility.h"

namespace Passes::Atmosphere::LutPasses
{
    struct PassData;
}

namespace Passes::SceneCsm
{
    struct PassData;
}

class SlimeMoldPass;
class SlimeMoldContext;
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
    void UpdateGlobalRenderGraphResources() const;

    RG::CsmData RenderGraphShadows(const ScenePass& scenePass,
        const CommonLight& directionalLight);
    void RenderGraphDepthPrepass(RG::Resource depth, const ScenePass& scenePass);
    SceneDrawPassDescription RenderGraphDepthPrepassDescription(RG::Resource depth, const ScenePass& scenePass);
    SceneDrawPassDescription RenderGraphForwardPbrDescription(RG::Resource color, RG::Resource depth,
        RG::CsmData csmData, const ScenePass& scenePass);

    SceneDrawPassDescription RenderGraphVBufferDescription(RG::Resource vbuffer, RG::Resource depth,
        const ScenePass& scenePass);
    RG::Resource RenderGraphVBufferPbr(RG::Resource vbuffer, RG::Resource viewInfo, RG::CsmData csmData);
    
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
    Passes::Atmosphere::LutPasses::PassData& RenderGraphAtmosphereLutPasses();
    void RenderGraphAtmosphereEnvironment(Passes::Atmosphere::LutPasses::PassData& lut);

    RG::Resource RenderGraphAtmosphere(Passes::Atmosphere::LutPasses::PassData& lut, RG::Resource aerialPerspective,
        RG::Resource color, RG::Resource depth, RG::CsmData csmData, RG::Resource clouds);

    struct CloudMapsInfo
    {
        RG::Resource CloudMap{};
        RG::Resource CloudShapeLowFrequency{};
        RG::Resource CloudShapeHighFrequency{};
        RG::Resource CloudCurlNoise{};
    };
    CloudMapsInfo RenderGraphGetCloudMaps();
    struct CloudsInfo
    {
        RG::Resource Color{};
        RG::Resource Depth{};
    };
    CloudsInfo RenderGraphClouds(const CloudMapsInfo& cloudMaps, RG::Resource color, RG::Resource aerialPerspective,
        RG::Resource depth);
    
    void Shutdown();

    RenderingInfo GetImGuiUIRenderingInfo();

    void OnWindowResize();
    void RecreateSwapchain();
    
    const FrameContext& GetFrameContext() const;
    FrameContext& GetFrameContext();

    u32 GetPreviousFrameNumber() const;
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

    DescriptorArenaAllocator m_PersistentMaterialAllocator;

    ShaderCache m_ShaderCache;
    std::unique_ptr<RG::Graph> m_Graph;
    std::unique_ptr<RG::RGMermaidExporter> m_MermaidExporter;
    RG::Resource m_Ssao{};
    TileLightsInfo m_TileLightsInfo{};
    ClusterLightsInfo m_ClusterLightsInfo{};
    
    Texture m_SkyboxTexture{};
    Texture m_SkyboxPrefilterMap{};
    Texture m_BRDFLut{};
    Buffer m_IrradianceSH{};
    Buffer m_SkyIrradianceSH{};
    RG::Resource m_SkyIrradianceSHResource{};
    Texture m_SkyPrefilterMap{};
    RG::Resource m_SkyPrefilterMapResource{};

    Texture m_CloudMap{};
    Texture m_CloudShapeLowFrequency{};
    Texture m_CloudShapeHighFrequency{};
    Texture m_CloudCurlNoise{};
    Passes::Clouds::CloudsNoiseParameters m_CloudMapNoiseParameters{};
    Passes::Clouds::CloudsNoiseParameters m_CloudShapeLowFrequencyNoiseParameters{};
    Passes::Clouds::CloudsNoiseParameters m_CloudShapeHighFrequencyNoiseParameters{};
    Passes::Clouds::CloudParameters m_CloudParameters{};
    
    std::shared_ptr<SlimeMoldContext> m_SlimeMoldContext;

    SceneInfo* m_TestScene{nullptr};
    Scene m_Scene;
    SceneBucketList m_SceneBucketList;
    SceneRenderObjectSet m_OpaqueSet;
    SceneVisibilityHandle m_OpaqueSetPrimaryVisibility{};
    SceneView m_OpaqueSetPrimaryView{};

    std::array<Buffer, BUFFERED_FRAMES> m_MinMaxDepthReductions{};
    std::array<Buffer, BUFFERED_FRAMES> m_MinMaxDepthReductionsNextFrame{};
    SceneMultiviewVisibility m_ShadowMultiviewVisibility{};
    SceneMultiviewVisibility m_PrimaryVisibility{};
    SceneVisibilityPassesResources m_ShadowMultiviewVisibilityResources{};
    SceneVisibilityPassesResources m_PrimaryVisibilityResources{};

    Texture m_TransmittanceLut{};
    Texture m_SkyViewLut{};
    u32 m_TransmittanceLutBindlessIndex{};
    u32 m_SkyViewLutBindlessIndex{};

    u32 m_BlueNoiseBindlessIndex{};
    
    bool m_IsWindowResized{false};
    bool m_FrameEarlyExit{false};

    bool m_HasExecutedSingleTimePasses{false};
};
