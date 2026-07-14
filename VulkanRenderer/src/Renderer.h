#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL

#include <vector>

#include "ResourceUploader.h"
#include "Core/Camera.h"
#include "RenderGraph/RGGraph.h"
#include "FrameContext.h"
#include "Assets/AssetSystem.h"
#include "Assets/Images/ImageAssetManager.h"
#include "Renderer/RGAssets.h"
#include "RenderGraph/Passes/Clouds/CloudCommon.h"
#include "RenderGraph/Passes/Clouds/VerticalProfile/VPCloudPass.h"
#include "RenderGraph/Passes/Scene/SceneGeometryRGResources.h"
#include "RenderGraph/Passes/Scene/Visibility/SceneVisibilityPassesCommon.h"
#include "RenderGraph/Passes/SceneDraw/SceneDrawPassesCommon.h"
#include "RenderGraph/Passes/SceneDraw/PBR/ExposurePass.h"
#include "RenderGraph/Passes/SceneDraw/PBR/TonemappingPass.h"
#include "RenderGraph/Visualization/RGMermaidExporter.h"
#include "Vulkan/Device.h"
#include "Rendering/Swapchain.h"
#include "Scene/BindlessTextureDescriptorsRingBuffer.h"
#include "Scene/Scene.h"
#include "Scene/ScenePass.h"
#include "Scene/SceneRenderObjectSet.h"
#include "Scene/Visibility/SceneMultiviewVisibility.h"
#include <AssetImportLib/Importers/Shaders/ShaderImporter.h>

namespace lux
{
class InputEvent;
class Window;
class SceneAssetManager;
class MaterialAssetManager;
class ImageAssetManager;
}

namespace lux
{
class ShaderAssetManager;
}

namespace Passes::SceneMetaDraw
{
struct PassData;
}

namespace Passes::Atmosphere::LutPasses
{
struct PassData;
}

namespace Passes::SceneCsm
{
struct PassData;
}

class SlimeMoldContext;
class Camera;
class CameraController;

class Renderer
{
public:
    void Init();
    void Shutdown();
    
    static Renderer* Get(); 

    void Run();
    void OnRender();
    void OnUpdate();

    void BeginFrame();
    void EndFrame();

    const lux::Window& GetWindow() const { return *m_Window; }
private:
    Renderer();
    
    void OnInputEvent(const lux::InputEvent& event);
    
    void InitRenderingStructures();
    void InitRenderGraph();
    void ExecuteSingleTimePasses();
    void SetupRenderSlimePasses();
    void SetupRenderGraph();
    void UpdateGlobalRenderGraphResources();

    RG::CsmData RenderGraphShadows(const ScenePass& scenePass, const lux::CommonLight& directionalLight);
    Passes::SceneMetaDraw::PassData& RenderGraphDepthPrepass(RG::ImageResource depth, const ScenePass& scenePass);
    SceneDrawPassDescription RenderGraphDepthPrepassDescription(RG::ImageResource depth, const ScenePass& scenePass);
    SceneDrawPassDescription RenderGraphForwardPbrDescription(RG::ImageResource color, RG::ImageResource depth,
        RG::CsmData csmData, const ScenePass& scenePass);

    SceneDrawPassDescription RenderGraphVBufferDescription(RG::ImageResource vbuffer, RG::ImageResource depth,
        const ScenePass& scenePass);
    RG::ImageResource RenderGraphVBufferPbr(RG::ImageResource vbuffer, RG::BufferResource visibleMeshlets,
        RG::BufferResource viewInfo, RG::CsmData csmData);

    Passes::SceneMetaDraw::PassData& RenderGraphForwardPass(RG::ImageResource& color, RG::ImageResource& depth);
    Passes::SceneMetaDraw::PassData& RenderGraphVBuffer(RG::ImageResource& vbuffer, RG::ImageResource& color, 
        RG::ImageResource& depth);
    
    void RenderGraphOnFrameDepthGenerated(StringId passName, RG::ImageResource depth);

    RG::ImageResource RenderGraphSSAO(StringId baseName, RG::ImageResource depth);
    
    struct TileLightsInfo
    {
        RG::BufferResource Tiles{};
        RG::BufferResource ZBins{};
    };
    TileLightsInfo RenderGraphCullLightsTiled(StringId baseName, RG::ImageResource depth);

    struct ClusterLightsInfo
    {
        RG::BufferResource Clusters{};
    };
    ClusterLightsInfo RenderGraphCullLightsClustered(StringId baseName, RG::ImageResource depth);

    struct CloudMapsInfo
    {
        RG::ImageResource Coverage{};
        RG::ImageResource Profile{};
        RG::ImageResource ShapeLowFrequency{};
        RG::ImageResource ShapeHighFrequency{};
        RG::ImageResource CurlNoise{};
    };
    CloudMapsInfo RenderGraphGetCloudMaps();
    RG::ImageResource RenderGraphSkyBox(RG::ImageResource color, RG::ImageResource depth);
    Passes::Atmosphere::LutPasses::PassData& RenderGraphAtmosphereLutPasses();
    struct AtmosphereEnvironmentInfo
    {
        RG::ImageResource AtmosphereWithClouds{};
        RG::ImageResource CloudsEnvironment{};
    };
    AtmosphereEnvironmentInfo RenderGraphAtmosphereEnvironment(Passes::Atmosphere::LutPasses::PassData& lut,
        const CloudMapsInfo& cloudMaps);

    RG::ImageResource RenderGraphAtmosphere(Passes::Atmosphere::LutPasses::PassData& lut,
        RG::ImageResource aerialPerspective, RG::ImageResource color, RG::ImageResource depth, RG::CsmData csmData,
        RG::ImageResource clouds, RG::ImageResource cloudsDepth, RG::ImageResource cloudsEnvironment);

    struct CloudsInfo
    {
        RG::ImageResource ColorPrevious{};
        RG::ImageResource DepthPrevious{};
        RG::ImageResource ReprojectionPrevious{};
        RG::ImageResource Color{};
        RG::ImageResource Depth{};
        RG::ImageResource Reprojection{};
    };
    CloudsInfo RenderGraphClouds(const CloudMapsInfo& cloudMaps, RG::ImageResource color, 
        RG::ImageResource aerialPerspective, RG::ImageResource minMaxDepth, RG::ImageResource sceneDepth);
    void RenderGraphCloudShadows(const CloudMapsInfo& cloudMaps);
    
    void OnImageAssetReloaded(lux::ImageHandle image);
    
    RenderingInfo GetImGuiUIRenderingInfo();

    void OnWindowResize();
    void RecreateSwapchain();
    
    const FrameContext& GetFrameContext() const;
    FrameContext& GetFrameContext();

    u32 GetPreviousFrameNumber() const;
private:
    std::unique_ptr<lux::Window> m_Window;
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
    Descriptors m_TextureHeap{};
    PipelineLayout m_TextureHeapPipelineLayout{};

    std::shared_ptr<lux::assetlib::io::AssetIoInterface> m_AssetIoInterface{};
    std::shared_ptr<lux::assetlib::io::AssetCompressor> m_AssetCompressor{};
    std::shared_ptr<lux::import::Context> m_ImportCtx{};
    lux::import::ShaderImportSettings m_ShaderImportSettings{};

    lux::AssetSystem m_AssetSystem;
    std::unique_ptr<lux::ShaderAssetManager> m_ShaderAssetManager;
    std::unique_ptr<lux::ImageAssetManager> m_ImageAssetManager;
    std::unique_ptr<lux::MaterialAssetManager> m_MaterialAssetManager;
    std::unique_ptr<lux::SceneAssetManager> m_SceneAssetManager;
    
    std::unique_ptr<RG::Graph> m_Graph;
    std::unique_ptr<RG::RGMermaidExporter> m_MermaidExporter;
    
    RG::ImageResource m_Ssao{};
    TileLightsInfo m_TileLightsInfo{};
    ClusterLightsInfo m_ClusterLightsInfo{};

    RG::BufferResource m_DepthMinMaxCurrentFrame{};
    RG::CsmData m_CsmData{};
    
    RG::PersistentImageResource m_SkyboxTexture{};
    RG::PersistentImageResource m_SkyboxPrefilterMap{};
    RG::PersistentImageResource m_BRDFLut{};
    RG::PersistentBufferResource m_IrradianceSH{};
    RG::PersistentBufferResource m_SkyIrradianceSH{};
    RG::BufferResource m_SkyIrradianceSHResource{};
    RG::PersistentImageResource m_SkyPrefilterMap{};
    RG::ImageResource m_SkyPrefilterMapResource{};

    lux::PersistentImageAsset m_MipsTest{};
    
    RG::PersistentImageResource m_SkyAtmosphereWithCloudsEnvironment{};
    RG::PersistentImageResource m_CloudsEnvironment{};

    lux::PersistentImageAsset m_CloudCoverage{};
    lux::PersistentImageAsset m_CloudProfileMap{};
    RG::PersistentImageResource m_CloudShapeLowFrequency{};
    RG::PersistentImageResource m_CloudShapeHighFrequency{};
    RG::PersistentImageResource m_CloudCurlNoise{};
    std::array<RG::PersistentImageResource, 2> m_CloudColorAccumulation{};
    std::array<RG::PersistentImageResource, 2> m_CloudDepthAccumulation{};
    std::array<RG::PersistentImageResource, 2> m_CloudReprojectionFactor{};
    u32 m_CloudsAccumulationIndex{0};
    u32 m_CloudsAccumulationIndexPrev{1};
    Passes::PbrCameraExposure::ExposureSettings m_ExposureSettings{};
    Passes::PbrTonemapping::TonemappingType m_TonemappingType{Passes::PbrTonemapping::TonemappingType::GT7};
    Passes::Clouds::CloudsNoiseParameters m_CloudCoverageNoiseParameters{};
    Passes::Clouds::CloudsNoiseParameters m_CloudShapeLowFrequencyNoiseParameters{};
    Passes::Clouds::CloudsNoiseParameters m_CloudShapeHighFrequencyNoiseParameters{};
    Passes::Clouds::VP::CloudParameters m_CloudParameters{};
    RG::BufferResource m_CloudParametersResource{};
    bool m_CloudsReprojectionEnabled{true};
    lux::CommonLight* m_SunLight{nullptr};
    
    std::shared_ptr<SlimeMoldContext> m_SlimeMoldContext;

    std::vector<lux::SceneHandle> m_Scenes;
    lux::SceneHandle m_Lights{};
    std::unique_ptr<Scene> m_Scene;
    SceneGeometryRGResources m_SceneGeometryRGResources;
    SceneBucketList m_SceneBucketList;
    SceneRenderObjectSet m_OpaqueSet;
    SceneVisibilityHandle m_OpaqueSetPrimaryVisibility{};
    SceneView m_OpaqueSetPrimaryView{};
    
    std::array<RG::PersistentBufferResource, BUFFERED_FRAMES> m_MinMaxDepthReductions{};
    std::array<RG::PersistentBufferResource, BUFFERED_FRAMES> m_MinMaxDepthReductionsNextFrame{};
    SceneMultiviewVisibility m_ShadowMultiviewVisibility{};
    SceneMultiviewVisibility m_PrimaryVisibility{};
    SceneVisibilityPassesResources m_ShadowMultiviewVisibilityResources{};
    SceneVisibilityPassesResources m_PrimaryVisibilityResources{};

    std::array<RG::PersistentImageResource, BUFFERED_FRAMES> m_PrimaryHizPrevious{};

    RG::PersistentImageResource m_TransmittanceLut{};
    RG::PersistentImageResource m_SkyViewLut{};
    /* environment capture is updated across 6 frames, it needs sky view lut to not change to avoid flickering 
     * for that purpose, copy of the sky view is used, which is updated once every 6 frames
     */
    RG::PersistentImageResource m_SkyViewEnvironmentCaptureLut{};
    RG::PersistentImageResource m_VolumetricCloudShadow{};
    TextureHandle m_TransmittanceLutBindlessIndex{};
    TextureHandle m_SkyViewLutBindlessIndex{};
    TextureHandle m_BlueNoiseBindlessIndex{};
    TextureHandle m_VolumetricShadowBindlessIndex{};

    lux::AssetUpdatedHandler m_ImageAssetReloadedHandler;
    lux::PersistentImageAssetMap m_PersistentImageAssetMap;
    
    bool m_IsWindowResized{false};
    bool m_FrameEarlyExit{false};

    bool m_HasExecutedSingleTimePasses{false};
};
