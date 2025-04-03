#include "Renderer.h"

#include <numbers>
#include <tracy/Tracy.hpp>

#include "AssetManager.h"
#include "CameraGPU.h"
#include "Converters.h"
#include "Model.h"
#include "ShadingSettingsGPU.h"
#include "Core/Input.h"
#include "cvars/CVarSystem.h"

#include "GLFW/glfw3.h"
#include "Imgui/ImguiUI.h"
#include "Light/LightFrustumCuller.h"
#include "Light/LightZBinner.h"
#include "Light/SH.h"
#include "RenderGraph/Passes/AA/FxaaPass.h"
#include "Scene/ModelCollection.h"
#include "Scene/SceneGeometry.h"
#include "RenderGraph/Passes/AO/SsaoBlurPass.h"
#include "RenderGraph/Passes/AO/SsaoPass.h"
#include "RenderGraph/Passes/AO/SsaoVisualizePass.h"
#include "RenderGraph/Passes/Atmosphere/AtmospherePass.h"
#include "RenderGraph/Passes/Atmosphere/SimpleAtmospherePass.h"
#include "RenderGraph/Passes/Atmosphere/Environment/AtmosphereEnvironmentPass.h"
#include "RenderGraph/Passes/Extra/SlimeMold/SlimeMoldPass.h"
#include "RenderGraph/Passes/Scene/DrawSceneUnifiedBasic.h"
#include "RenderGraph/Passes/General/VisibilityPass.h"
#include "RenderGraph/Passes/Generated/MaterialsBindGroup.generated.h"
#include "RenderGraph/Passes/HiZ/HiZVisualize.h"
#include "RenderGraph/Passes/Lights/LightClustersBinPass.h"
#include "RenderGraph/Passes/Lights/LightClustersCompactPass.h"
#include "RenderGraph/Passes/Lights/LightClustersSetupPass.h"
#include "RenderGraph/Passes/Lights/LightTilesBinPass.h"
#include "RenderGraph/Passes/Lights/LightTilesSetupPass.h"
#include "RenderGraph/Passes/Lights/VisualizeLightClustersDepthLayersPass.h"
#include "RenderGraph/Passes/Lights/VisualizeLightClustersPass.h"
#include "RenderGraph/Passes/Lights/VisualizeLightTiles.h"
#include "RenderGraph/Passes/PBR/PbrVisibilityBufferIBLPass.h"
#include "RenderGraph/Passes/Scene/FillSceneIndirectDrawPass.h"
#include "RenderGraph/Passes/Scene/PrepareVisibleMeshletInfoPass.h"
#include "RenderGraph/Passes/Shadows/CSMVisualizePass.h"
#include "RenderGraph/Passes/Shadows/DepthReductionReadbackPass.h"
#include "RenderGraph/Passes/Shadows/ShadowPassesCommon.h"
#include "RenderGraph/Passes/Skybox/SkyboxPass.h"
#include "RenderGraph/Passes/Utility/BlitPass.h"
#include "RenderGraph/Passes/Utility/BRDFLutPass.h"
#include "RenderGraph/Passes/Utility/CopyTexturePass.h"
#include "RenderGraph/Passes/Utility/DiffuseIrradianceSHPass.h"
#include "RenderGraph/Passes/Utility/EnvironmentPrefilterPass.h"
#include "RenderGraph/Passes/Utility/EquirectangularToCubemapPass.h"
#include "RenderGraph/Passes/Utility/ImGuiTexturePass.h"
#include "RenderGraph/Passes/Utility/UploadPass.h"
#include "Rendering/Shader/ShaderCache.h"
#include "Scene/BindlessTextureDescriptorsRingBuffer.h"
#include "Scene/Scene.h"
#include "Scene/Sorting/DepthGeometrySorter.h"
#include "String/StringId.h"

Renderer::Renderer() = default;

void Renderer::Init()
{
    StringIdRegistry::Init();
    ShaderCache::Init();
    
    InitRenderingStructures();

    Input::s_MainViewportSize = Device::GetSwapchainDescription(m_Swapchain).SwapchainResolution;
    m_Camera = std::make_shared<Camera>(CameraType::Perspective);
    m_CameraController = std::make_unique<CameraController>(m_Camera);
    for (auto& ctx : m_FrameContexts)
        ctx.PrimaryCamera = m_Camera.get();

    m_Graph = std::make_unique<RG::Graph>();

    InitRenderGraph();

    // todo: this is temp (almost the entire file is)
    m_SceneLights = std::make_unique<SceneLight>();
    m_SceneLights->AddPointLight({
            .Position = glm::vec3{-0.8, 0.8, 1.0},
            .Color = glm::vec3{0.8, 0.2, 0.2},
            .Intensity = 8.0f,
            .Radius = 1.0f});
    m_SceneLights->AddPointLight({
            .Position = glm::vec3{0.0, 0.8, 1.0},
            .Color = glm::vec3{0.2, 0.8, 0.2},
            .Intensity = 8.0f,
            .Radius = 1.0f});
    m_SceneLights->AddPointLight({
            .Position = glm::vec3{0.8, 0.8, 1.0},
            .Color = glm::vec3{0.2, 0.2, 0.8},
            .Intensity = 8.0f,
            .Radius = 1.0f});
    constexpr u32 POINT_LIGHT_COUNT = 300;
    for (u32 i = 0; i < POINT_LIGHT_COUNT; i++)
        m_SceneLights->AddPointLight({
            .Position = glm::vec3{Random::Float(-39.0f, 39.0f), Random::Float(0.0f, 4.0f), Random::Float(-19.0f, 19.0f)},
            .Color = Random::Float3(0.0f, 1.0f),
            .Intensity = Random::Float(0.5f, 1.7f),
            .Radius = Random::Float(0.5f, 8.6f)});
}

void Renderer::InitRenderGraph()
{
    ShaderCache::SetAllocators(m_Graph->GetArenaAllocators());
    m_BindlessTextureDescriptorsRingBuffer = std::make_unique<BindlessTextureDescriptorsRingBuffer>(
        1024,
        ShaderCache::Register("Core.Materials"_hsv, "materials.shader", {}));
    m_GraphModelCollection.SetBindlessTextureDescriptorsRingBuffer(*m_BindlessTextureDescriptorsRingBuffer);
    
    Model* helmet = Model::LoadFromAsset("../assets/models/flight_helmet/flightHelmet.model");
    Model* brokenHelmet = Model::LoadFromAsset("../assets/models/broken_helmet/scene.model");
    Model* car = Model::LoadFromAsset("../assets/models/sphere_big/scene.model");
    Model* plane = Model::LoadFromAsset("../assets/models/plane/scene.model");
    m_GraphModelCollection.CreateDefaultTextures();
    m_GraphModelCollection.RegisterModel(helmet, "helmet");
    m_GraphModelCollection.RegisterModel(brokenHelmet, "broken helmet");
    m_GraphModelCollection.RegisterModel(car, "car");
    m_GraphModelCollection.RegisterModel(plane, "plane");

    m_GraphModelCollection.AddModelInstance("helmet", {
        .Transform = {
            .Position = glm::vec3{0.0f, 0.0f, 0.0f},
            .Scale = glm::vec3{0.2f}}});
    
    m_GraphOpaqueGeometry = SceneGeometry::FromModelCollectionFiltered(m_GraphModelCollection,
        *GetFrameContext().ResourceUploader,
        [this](const Mesh&, const Material& material) {
            return material.Type == assetLib::ModelInfo::MaterialType::Opaque;
        });
    m_GraphTranslucentGeometry = SceneGeometry::FromModelCollectionFiltered(m_GraphModelCollection,
        *GetFrameContext().ResourceUploader,
        [this](const Mesh&, const Material& material) {
            return material.Type == assetLib::ModelInfo::MaterialType::Translucent;
        });

    m_Graph->SetBackbuffer(Device::GetSwapchainDescription(m_Swapchain).DrawImage);


    MaterialsShaderBindGroup bindGroup(m_BindlessTextureDescriptorsRingBuffer->GetMaterialsShader());
    bindGroup.SetMaterialsGlobally({.Buffer = m_GraphOpaqueGeometry.GetMaterialsBuffer()});

    ShaderCache::SetAllocators(m_Graph->GetArenaAllocators());
    // todo: this is a little weird
    ShaderCache::AddBindlessDescriptors("main_materials"_hsv,
        ShaderCache::Get("Core.Materials"_hsv).Descriptors(DescriptorsKind::Materials));
    
    // model collection might not have any translucent objects
    if (m_GraphTranslucentGeometry.IsValid())
    {
        /* todo:
         * this is actually unnecessary, there are at least two more options:
         * - have both `u_materials` and `u_translucent_materials` (not so bad)
         * - have two different descriptors: set (2) for materials and set (3) for bindless textures
         */
        // todo: fix me once i fix api
        /*ShaderDescriptors translucentMaterialDescriptors = ShaderDescriptors::Builder()
                .SetTemplate(drawTemplate, DescriptorsKind::Resource)
                // todo: make this (2) an enum
                .ExtractSet(2)
                .BindlessCount(1024)
                .Build();
        translucentMaterialDescriptors.UpdateGlobalBinding(UNIFORM_MATERIALS,
            m_GraphTranslucentGeometry.GetMaterialsBuffer().BindingInfo());*/
        //m_GraphModelCollection.ApplyMaterialTextures(translucentMaterialDescriptors);
    }
    
    // todo: separate geometry for shadow casters

    m_SlimeMoldContext = std::make_shared<SlimeMoldContext>(
        SlimeMoldContext::RandomIn(Device::GetSwapchainDescription(m_Swapchain).SwapchainResolution,
            1, 5000000, *GetFrameContext().ResourceUploader));

    SceneConverter::Convert(
        *CVars::Get().GetStringCVar("Path.Assets"_hsv),
        *CVars::Get().GetStringCVar("Path.Assets"_hsv) + "models/lights_test/scene.gltf");
    
    m_Scene = Scene::CreateEmpty(Device::DeletionQueue());
    m_SceneBucketList.Init(m_Scene);
    m_OpaqueSet.Init("Opaque"_hsv, m_Scene, m_SceneBucketList, {
        ScenePassCreateInfo{
            .Name = "Visibility"_hsv,
            .BucketCreateInfos = {
                {
                    .Name = "Opaque material"_hsv,
                    .Filter = [](const SceneGeometryInfo& geometry, SceneRenderObjectHandle renderObject) {
                        const Material2& material = geometry.MaterialsCpu[
                            geometry.RenderObjects[renderObject.Index].Material];
                        return enumHasAny(material.Flags, MaterialFlags::Opaque);
                    }
                }}
            },
    }, Device::DeletionQueue());
    
    m_TestScene = SceneInfo::LoadFromAsset(
        *CVars::Get().GetStringCVar("Path.Assets"_hsv) + "models/lights_test/scene.scene",
        *m_BindlessTextureDescriptorsRingBuffer, Device::DeletionQueue());
    SceneInstance instance = m_Scene.Instantiate(*m_TestScene, {
        .Transform = {
            .Position = glm::vec3{0.0f, -1.5f, -7.0f},
            .Orientation = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
            .Scale = glm::vec3{1.0f},}},
        GetFrameContext());


    /* initial submit */
    Device::ImmediateSubmit([&](CommandBuffer, RenderCommandList& cmdList)
    {
        FrameContext ctx {.CommandList = cmdList};
        GetFrameContext().ResourceUploader->SubmitUpload(ctx);
    });
}

void Renderer::ExecuteSingleTimePasses()
{
    static constexpr std::string_view SKYBOX_PATH = "../assets/textures/autumn_field_puresky_4k.tx";
    Texture equirectangular = Device::CreateImage({
        .DataSource = SKYBOX_PATH,
        .Description = ImageDescription{
            .Usage = ImageUsage::Sampled},
        .CalculateMipmaps = false},
        GetFrameContext().DeletionQueue);
    const TextureDescription& equirectangularDescription = Device::GetImageDescription(equirectangular);
    m_SkyboxTexture = Device::CreateImage(ImageCreateInfo{
        .Description = ImageDescription{
            .Width = equirectangularDescription.Width / 2,
            .Height = equirectangularDescription.Width / 2,
            .Mipmaps = ImageUtils::mipmapCount(glm::uvec2{equirectangularDescription.Width / 2}),
            .Format = Format::RGBA16_FLOAT,
            .Kind = ImageKind::Cubemap,
            .Usage = ImageUsage::Sampled | ImageUsage::Storage},
        .CalculateMipmaps = false});
    
    m_SkyboxPrefilterMap = Device::CreateImage({
        .Description = Passes::EnvironmentPrefilter::getPrefilteredTextureDescription(),
        .CalculateMipmaps = false});
    
    m_IrradianceSH = Device::CreateBuffer({
        .SizeBytes = sizeof(SH9Irradiance),
        .Usage = BufferUsage::Ordinary | BufferUsage::Storage});
    m_SkyIrradianceSH = Device::CreateBuffer({
        .SizeBytes = sizeof(SH9Irradiance),
        .Usage = BufferUsage::Ordinary | BufferUsage::Storage});

    m_BRDFLut = Device::CreateImage({
        .Description = Passes::BRDFLut::getLutDescription(),
        .CalculateMipmaps = false});
    
    m_Graph->Reset(GetFrameContext());

    Passes::EquirectangularToCubemap::addToGraph("Scene.Skybox"_hsv, *m_Graph,
        equirectangular, m_SkyboxTexture);
    Passes::DiffuseIrradianceSH::addToGraph(
        "Scene.DiffuseIrradianceSH"_hsv, *m_Graph, m_SkyboxTexture, m_IrradianceSH, false);
    Passes::EnvironmentPrefilter::addToGraph(
        "Scene.EnvironmentPrefilter"_hsv, *m_Graph, m_SkyboxTexture, m_SkyboxPrefilterMap);
    Passes::BRDFLut::addToGraph(
        "Scene.BRDFLut"_hsv, *m_Graph, m_BRDFLut);

    m_Graph->Compile(GetFrameContext());
    m_Graph->Execute(GetFrameContext());
}

void Renderer::SetupRenderSlimePasses()
{
    auto& slime = Passes::SlimeMold::addToGraph("Slime"_hsv, *m_Graph, *m_SlimeMoldContext);
    auto& slimeOutput = m_Graph->GetBlackboard().Get<Passes::SlimeMold::PassData>(slime);
    Passes::ImGuiTexture::addToGraph("ImGuiTexture.Mold"_hsv, *m_Graph, slimeOutput.ColorOut);
}

void Renderer::SetupRenderGraph()
{
    using namespace RG;
    
    m_Graph->Reset(GetFrameContext());
    auto& blackboard = m_Graph->GetBlackboard();
    Resource backbuffer = m_Graph->GetBackbuffer();

    SwapchainDescription& swapchain = Device::GetSwapchainDescription(m_Swapchain);
    // update camera
    CameraGPU cameraGPU = CameraGPU::FromCamera(*m_Camera, swapchain.SwapchainResolution);
    static ShadingSettingsGPU shadingSettingsGPU = {
        .EnvironmentPower = 1.0f,
        .SoftShadows = false};
    ImGui::Begin("Shading Settings");
    ImGui::DragFloat("Environment power", &shadingSettingsGPU.EnvironmentPower, 1e-2f, 0.0f, 1.0f);
    ImGui::Checkbox("Soft shadows", (bool*)&shadingSettingsGPU.SoftShadows);
    ImGui::End();

    Resource shadingSettings = Passes::Upload::addToGraph("Upload.ShadingSettings"_hsv, *m_Graph, shadingSettingsGPU);
    Resource primaryCamera = Passes::Upload::addToGraph("Upload.PrimaryCamera"_hsv, *m_Graph, cameraGPU);
    
    GlobalResources globalResources = {
        .FrameNumberTick = GetFrameContext().FrameNumberTick,
        .Resolution = GetFrameContext().Resolution,
        .PrimaryCamera = m_Camera.get(),
        .PrimaryCameraGPU = primaryCamera,
        .ShadingSettings = shadingSettings};
    blackboard.Update(globalResources);
    
    MaterialsShaderBindGroup bindGroup(m_BindlessTextureDescriptorsRingBuffer->GetMaterialsShader());
    bindGroup.SetMaterialsGlobally({.Buffer = m_Scene.Geometry().Materials.Buffer});

    auto& prepareMeshlets = Passes::PrepareVisibleMeshletInfo::addToGraph("PrepareVisibleMeshletInfo"_hsv, *m_Graph, {
        .RenderObjectSet = &m_OpaqueSet});
    auto& prepareMeshletsOutput = blackboard.Get<Passes::PrepareVisibleMeshletInfo::PassData>(prepareMeshlets);

    auto& fillIndirectDraws = Passes::FillSceneIndirectDraw::addToGraph("FillSceneIndirectDraw"_hsv, *m_Graph, {
        .Geometry = &m_Scene.Geometry(),
        .RenderObjectSet = &m_OpaqueSet,
        .MeshletInfos = prepareMeshletsOutput.MeshletInfos,
        .MeshletInfoCount = prepareMeshletsOutput.MeshletInfoCount});
    auto& fillIndirectDrawsOutput = blackboard.Get<Passes::FillSceneIndirectDraw::PassData>(fillIndirectDraws);
    
    Resource depth = m_Graph->CreateResource("Depth"_hsv, GraphTextureDescription{
        .Width = Device::GetSwapchainDescription(m_Swapchain).DrawResolution.x,
        .Height = Device::GetSwapchainDescription(m_Swapchain).DrawResolution.y,
        .Format = Format::D32_FLOAT});
    
    auto& ugb = Passes::DrawSceneUnifiedBasic::addToGraph("UGB"_hsv, *m_Graph, {
            .Geometry = &m_Scene.Geometry(),
            .Lights = &m_Scene.Lights(),
            .Draws = fillIndirectDrawsOutput.Draws[0],
            .DrawInfos = fillIndirectDrawsOutput.DrawInfos[0],
            .Resolution = Device::GetSwapchainDescription(m_Swapchain).DrawResolution,
            .Camera = GetFrameContext().PrimaryCamera,
        .Attachments = {
            .Colors = {DrawAttachment{
                .Resource = backbuffer,
                .Description = {
                    .OnLoad = AttachmentLoad::Clear,
                    .ClearColor = {.F = glm::vec4{0.0f, 0.0f, 0.0f, 1.0f}}}}},
            .Depth = DepthStencilAttachment{
                .Resource = depth,
                .Description = {
                    .OnLoad = AttachmentLoad::Clear,
                    .ClearDepthStencil = {.Depth = 0.0f, .Stencil = 0}}}}});
    
    

    // todo: move to proper place (this is just testing atm)
    /*if (m_GraphTranslucentGeometry.IsValid())
    {
        DepthGeometrySorter translucentSorter(m_Camera->GetPosition(), m_Camera->GetForward());
        translucentSorter.Sort(m_GraphTranslucentGeometry, *GetFrameContext().ResourceUploader);
    }

    auto& visibility = Passes::Draw::Visibility::addToGraph("Visibility", *m_Graph, {
        .Geometry = &m_GraphOpaqueGeometry,
        .Resolution = swapchain.SwapchainResolution,
        .Camera = GetFrameContext().PrimaryCamera});
    auto& visibilityOutput = blackboard.Get<Passes::Draw::Visibility::PassData>(visibility);

    bool tileLights = *CVars::Get().GetI32CVar("Lights.Bin.Tiles"_hsv) == 1;
    bool clusterLights = *CVars::Get().GetI32CVar("Lights.Bin.Clusters"_hsv) == 1;

    struct TileLightsInfo
    {
        Resource Tiles{};
        Resource ZBins{};
    };
    if (tileLights)
    {
        auto zbins = LightZBinner::ZBinLights(*m_SceneLights, *GetFrameContext().PrimaryCamera);
        Resource zbinsResource = Passes::Upload::addToGraph("Upload.Light.ZBins", *m_Graph, zbins.Bins);
        auto& tilesSetup = Passes::LightTilesSetup::addToGraph("Tiles.Setup", *m_Graph);
        auto& tilesSetupOutput = blackboard.Get<Passes::LightTilesSetup::PassData>(tilesSetup);
        auto& binLightsTiles = Passes::LightTilesBin::addToGraph("Tiles.Bin", *m_Graph, tilesSetupOutput.Tiles,
            visibilityOutput.DepthOut, *m_SceneLights);
        auto& binLightsTilesOutput = blackboard.Get<Passes::LightTilesBin::PassData>(binLightsTiles);
        auto& visualizeTiles = Passes::LightTilesVisualize::addToGraph("Tiles.Visualize", *m_Graph,
            binLightsTilesOutput.Tiles, visibilityOutput.DepthOut,
            zbinsResource);
        auto& visualizeTilesOutput = blackboard.Get<Passes::LightTilesVisualize::PassData>(
            visualizeTiles);
        Passes::ImGuiTexture::addToGraph("Tiles.Visualize.Texture", *m_Graph, visualizeTilesOutput.ColorOut);

        TileLightsInfo info = {
            .Tiles = binLightsTilesOutput.Tiles,
            .ZBins = zbinsResource};
        blackboard.Update(info);
    }

    struct ClusterLightsInfo
    {
        Resource Clusters{};
    };
    if (clusterLights)
    {
        // light clustering:
        auto& clustersSetup = Passes::LightClustersSetup::addToGraph("Clusters.Setup", *m_Graph);
        auto& clustersSetupOutput = blackboard.Get<Passes::LightClustersSetup::PassData>(clustersSetup);
        auto& compactClusters = Passes::LightClustersCompact::addToGraph("Clusters.Compact", *m_Graph,
            clustersSetupOutput.Clusters, clustersSetupOutput.ClusterVisibility, visibilityOutput.DepthOut);
        auto& compactClustersOutput = blackboard.Get<Passes::LightClustersCompact::PassData>(compactClusters);
        auto& binLightsClusters = Passes::LightClustersBin::addToGraph("Clusters.Bin", *m_Graph,
            compactClustersOutput.DispatchIndirect,
            compactClustersOutput.Clusters, compactClustersOutput.ActiveClusters, compactClustersOutput.ActiveClustersCount,
            *m_SceneLights);
        auto& binLightsClustersOutput = blackboard.Get<Passes::LightClustersBin::PassData>(binLightsClusters);

        auto& visualizeClusters = Passes::LightClustersVisualize::addToGraph("Clusters.Visualize", *m_Graph,
            visibilityOutput.DepthOut, binLightsClustersOutput.Clusters);
        auto& visualizeClustersOutput = blackboard.Get<Passes::LightClustersVisualize::PassData>(
            visualizeClusters);
        Passes::ImGuiTexture::addToGraph("Clusters.Visualize.Texture", *m_Graph, visualizeClustersOutput.ColorOut);

        ClusterLightsInfo info = {
            .Clusters = binLightsClustersOutput.Clusters};
        blackboard.Update(info);
    }

    auto& ssao = Passes::Ssao::addToGraph("SSAO", 32, *m_Graph, visibilityOutput.DepthOut);
    auto& ssaoOutput = blackboard.Get<Passes::Ssao::PassData>(ssao);

    auto& ssaoBlurHorizontal = Passes::SsaoBlur::addToGraph("SSAO.Blur.Horizontal", *m_Graph,
        ssaoOutput.SSAO, {},
        SsaoBlurPassKind::Horizontal);
    auto& ssaoBlurHorizontalOutput = blackboard.Get<Passes::SsaoBlur::PassData>(ssaoBlurHorizontal);
    auto& ssaoBlurVertical = Passes::SsaoBlur::addToGraph("SSAO.Blur.Vertical", *m_Graph,
        ssaoBlurHorizontalOutput.SsaoOut, ssaoOutput.SSAO,
        SsaoBlurPassKind::Vertical);
    auto& ssaoBlurVerticalOutput = blackboard.Get<Passes::SsaoBlur::PassData>(ssaoBlurVertical);

    auto& ssaoVisualize = Passes::SsaoVisualize::addToGraph("SSAO.Visualize", *m_Graph,
        ssaoBlurVerticalOutput.SsaoOut, {});
    auto& ssaoVisualizeOutput = blackboard.Get<Passes::SsaoVisualize::PassData>(ssaoVisualize);

    static bool useDepthReduction = false;
    static bool stabilizeCascades = false;
    ImGui::Begin("Shadow settings");
    ImGui::Checkbox("Use depth reduction", &useDepthReduction);
    ImGui::Checkbox("Stabilize cascades", &stabilizeCascades);
    ImGui::End();
    f32 shadowMin = 0.0f;
    f32 shadowMax = 200.0f;
    if (visibilityOutput.MinMaxDepth.IsValid() && useDepthReduction)
    {
        auto& minMaxDepthReadback = Passes::DepthReductionReadback::addToGraph("Visibility.Readback.Depth", *m_Graph,
            visibilityOutput.PreviousMinMaxDepth, GetFrameContext().PrimaryCamera);
        auto& minMaxDepthReadbackOutput = blackboard.Get<Passes::DepthReductionReadback::PassData>(
            minMaxDepthReadback);
        shadowMin = minMaxDepthReadbackOutput.Min;
        shadowMax = minMaxDepthReadbackOutput.Max;
    }
    auto& csm = Passes::CSM::addToGraph("CSM", *m_Graph, {
        .Geometry = &m_GraphOpaqueGeometry,
        .MainCamera = m_Camera.get(),
        .DirectionalLight = &m_SceneLights->GetDirectionalLight(),
        .ShadowMin = shadowMin,
        .ShadowMax = shadowMax,
        .StabilizeCascades = stabilizeCascades,
        .GeometryBounds = m_GraphOpaqueGeometry.GetBounds()});
    auto& csmOutput = blackboard.Get<Passes::CSM::PassData>(csm);

    static bool useSky = false;
    ImGui::Begin("Use sky as env");
    ImGui::Checkbox("Sky", &useSky);
    ImGui::End();

    auto& pbr = Passes::Pbr::VisibilityIbl::addToGraph("Pbr.Visibility.Ibl", *m_Graph, {
        .VisibilityTexture = visibilityOutput.ColorOut,
        .ColorIn = {},
        .SceneLights = m_SceneLights.get(),
        .Clusters = clusterLights ? blackboard.Get<ClusterLightsInfo>().Clusters : Resource{},
        .Tiles = tileLights ? blackboard.Get<TileLightsInfo>().Tiles : Resource{},
        .ZBins = tileLights ? blackboard.Get<TileLightsInfo>().ZBins : Resource{},
        .IBL = {
            .IrradianceSH = m_Graph->AddExternal("IrradianceSH", useSky ? m_SkyIrradianceSH : m_IrradianceSH),
            .PrefilterEnvironment = m_Graph->AddExternal("PrefilterMap", m_SkyboxPrefilterMap),
            .BRDF = m_Graph->AddExternal("BRDF", m_BRDFLut)},
        .SSAO = {
            .SSAO = ssaoBlurVerticalOutput.SsaoOut},
        .CSMData = {
            .ShadowMap = csmOutput.ShadowMap,
            .CSM = csmOutput.CSM},
        .Geometry = &m_GraphOpaqueGeometry});
    auto& pbrOutput = blackboard.Get<Passes::Pbr::VisibilityIbl::PassData>(pbr);

    Resource renderedColor = {};
    Resource renderedDepth = {};
    
    // todo: this is temp
    {
        if (!blackboard.TryGet<AtmosphereSettings>())
            blackboard.Update(AtmosphereSettings::EarthDefault());

        AtmosphereSettings& settings = blackboard.Get<AtmosphereSettings>();
        ImGui::Begin("Atmosphere settings");
        ImGui::DragFloat3("Rayleigh scattering", &settings.RayleighScattering[0], 1e-2f, 0.0f, 100.0f);
        ImGui::DragFloat3("Rayleigh absorption", &settings.RayleighAbsorption[0], 1e-2f, 0.0f, 100.0f);
        ImGui::DragFloat3("Mie scattering", &settings.MieScattering[0], 1e-2f, 0.0f, 100.0f);
        ImGui::DragFloat3("Mie absorption", &settings.MieAbsorption[0], 1e-2f, 0.0f, 100.0f);
        ImGui::DragFloat3("Ozone absorption", &settings.OzoneAbsorption[0], 1e-2f, 0.0f, 100.0f);
        ImGui::ColorEdit3("Surface albedo", &settings.SurfaceAlbedo[0]);
        ImGui::DragFloat("Surface", &settings.Surface, 1e-2f, 0.0f, 100.0f);
        ImGui::DragFloat("Atmosphere", &settings.Atmosphere, 1e-2f, 0.0f, 100.0f);
        ImGui::DragFloat("Rayleigh density", &settings.RayleighDensity, 1e-2f, 0.0f, 100.0f);
        ImGui::DragFloat("Mie density", &settings.MieDensity, 1e-2f, 0.0f, 100.0f);
        ImGui::DragFloat("Ozone density", &settings.OzoneDensity, 1e-2f, 0.0f, 100.0f);
        ImGui::End();
        
        auto& atmosphere = Passes::Atmosphere::addToGraph("Atmosphere", *m_Graph, settings, *m_SceneLights,
            pbrOutput.ColorOut, visibilityOutput.DepthOut,
            CSMData{
                .ShadowMap = csmOutput.ShadowMap,
                .CSM = csmOutput.CSM});
        auto& atmosphereOutput = blackboard.Get<Passes::Atmosphere::PassData>(atmosphere);
        
        
        // todo: this is a temp place for it obv
        {
            if (useSky)
                Passes::DiffuseIrradianceSH::addToGraph("Sky.DiffuseIrradianceSH", *m_Graph,
                    atmosphereOutput.EnvironmentOut, m_SkyIrradianceSH, false);
        }
        
        Passes::ImGuiTexture::addToGraph("Atmosphere.Transmittance.Lut", *m_Graph, atmosphereOutput.TransmittanceLut);
        Passes::ImGuiTexture::addToGraph("Atmosphere.Multiscattering.Lut", *m_Graph, atmosphereOutput.MultiscatteringLut);
        Passes::ImGuiTexture::addToGraph("Atmosphere.SkyView.Lut", *m_Graph, atmosphereOutput.SkyViewLut);
        Passes::ImGuiTexture::addToGraph("Atmosphere.Atmosphere", *m_Graph, atmosphereOutput.Atmosphere);
        Passes::ImGuiTexture3d::addToGraph("Atmosphere.AerialPerspective.Lut", *m_Graph, atmosphereOutput.AerialPerspectiveLut);
        Passes::ImGuiCubeTexture::addToGraph("Atmosphere.Environment.Lut", *m_Graph, atmosphereOutput.EnvironmentOut);

        renderedColor = atmosphereOutput.Atmosphere;

        if (!useSky)
        {
            auto& skybox = Passes::Skybox::addToGraph("Skybox", *m_Graph,
                atmosphereOutput.EnvironmentOut, renderedColor, visibilityOutput.DepthOut, GetFrameContext().Resolution, 1.2f);
            auto& skyboxOutput = blackboard.Get<Passes::Skybox::PassData>(skybox);
            renderedColor = skyboxOutput.ColorOut;
        }
    }
    
    // model collection might not have any translucent objects
    if (m_GraphTranslucentGeometry.IsValid())
    {
        /*Passes::Pbr::ForwardTranslucentIbl::addToGraph("Pbr.Translucent.Ibl", *m_Graph, {
            .Geometry = m_GraphTranslucentGeometry,
            .Resolution = m_Swapchain.GetResolution(),
            .Camera = GetFrameContext().PrimaryCamera,
            .ColorIn = renderedColor,
            .DepthIn = renderedDepth,
            .SceneLights = &m_SceneLights,
            .IBL = {
                 .Irradiance = m_Graph->AddExternal("IrradianceMap", m_SkyboxIrradianceMap),
                 .PrefilterEnvironment = m_Graph->AddExternal("PrefilterMap", m_SkyboxPrefilterMap),
                 .BRDF = m_Graph->AddExternal("BRDF", *m_BRDFLut)},
            .HiZContext = m_VisibilityPass->GetHiZContext()})
        auto& pbrTranslucentOutput = blackboard.Get<PbrForwardTranslucentIBLPass::PassData>();
        
        renderedColor = pbrTranslucentOutput.ColorOut; #1#
    }

    auto& fxaa = Passes::Fxaa::addToGraph("FXAA", *m_Graph, renderedColor);
    auto& fxaaOutput = blackboard.Get<Passes::Fxaa::PassData>(fxaa);
    //Passes::ImGuiTexture::addToGraph("FXAA.Texture", *m_Graph, fxaaOutput.AntiAliased);
    
    auto& copyRendered = Passes::CopyTexture::addToGraph("CopyRendered", *m_Graph,
        fxaaOutput.AntiAliased, backbuffer, glm::vec3{}, glm::vec3{1.0f});
    backbuffer = blackboard.Get<Passes::CopyTexture::PassData>(copyRendered).TextureOut;

    auto& hizVisualize = Passes::HiZVisualize::addToGraph("HiZ.Visualize", *m_Graph, visibilityOutput.HiZOut);
    auto& hizVisualizePassOutput = blackboard.Get<Passes::HiZVisualize::PassData>(hizVisualize);
    auto& hizMaxVisualize = Passes::HiZVisualize::addToGraph("HiZ.Max.Visualize", *m_Graph, visibilityOutput.HiZMaxOut);
    auto& hizMaxVisualizePassOutput = blackboard.Get<Passes::HiZVisualize::PassData>(hizMaxVisualize);

    auto& csmVisualize = Passes::VisualizeCSM::addToGraph("CSM.Visualize", *m_Graph, csmOutput, {});
    auto& visualizeCSMPassOutput = blackboard.Get<Passes::VisualizeCSM::PassData>(csmVisualize);

    Passes::ImGuiTexture::addToGraph("SSAO.Texture", *m_Graph, ssaoVisualizeOutput.ColorOut);
    Passes::ImGuiTexture::addToGraph("Visibility.HiZ.Texture", *m_Graph, hizVisualizePassOutput.ColorOut);
    Passes::ImGuiTexture::addToGraph("Visibility.HiZ.Max.Texture", *m_Graph, hizMaxVisualizePassOutput.ColorOut);
    Passes::ImGuiTexture::addToGraph("CSM.Texture", *m_Graph, visualizeCSMPassOutput.ColorOut);
    Passes::ImGuiTexture::addToGraph("BRDF.Texture", *m_Graph, m_BRDFLut);*/

    ImGui::Begin("Debug");
    if (ImGui::Button("Dump memory stats"))
        Device::DumpMemoryStats("./MemoryStats.json");
    ImGui::End();
    
    //SetupRenderSlimePasses();
}

void Renderer::UpdateLights()
{
    // todo: should not be here obv
    DirectionalLight directionalLight = m_SceneLights->GetDirectionalLight();
    ImGui::Begin("Directional Light");
    ImGui::DragFloat3("Direction", &directionalLight.Direction[0], 1e-2f, -1.0f, 1.0f);
    ImGui::ColorPicker3("Color", &directionalLight.Color[0]);
    ImGui::DragFloat("Intensity", &directionalLight.Intensity, 1e-1f, 0.0f, 100.0f);
    ImGui::DragFloat("Size", &directionalLight.Size, 1e-1f, 0.0f, 100.0f);
    ImGui::End();
    directionalLight.Direction = glm::normalize(directionalLight.Direction);
    m_SceneLights->SetDirectionalLight(directionalLight);

    LightFrustumCuller::CullDepthSort(*m_SceneLights, *GetFrameContext().PrimaryCamera);
    
    m_SceneLights->UpdateBuffers(GetFrameContext());    
}

Renderer* Renderer::Get()
{
    static Renderer renderer = {};
    return &renderer;
}

Renderer::~Renderer()
{
    Shutdown();
}

void Renderer::Run()
{
    while(!glfwWindowShouldClose(m_Window))
    {
        glfwPollEvents();

        OnUpdate();
        
        OnRender();
    }
}

void Renderer::OnRender()
{
    CPU_PROFILE_FRAME("On render")

    BeginFrame();
    /* light update requires cmd in recording state */
    UpdateLights();
    LightFrustumCuller::CullDepthSort(m_Scene.Lights(), *GetFrameContext().PrimaryCamera);
    m_Scene.Hierarchy().OnUpdate(m_Scene, GetFrameContext());
    m_Scene.Lights().OnUpdate(GetFrameContext());
    m_OpaqueSet.OnUpdate(GetFrameContext());

    if (Input::GetKey(Key::Space))
    {
        glm::vec3 position = m_Camera->GetPosition() + m_Camera->GetForward() * 4.0f;
        SceneInstance instance = m_Scene.Instantiate(*m_TestScene, {
            .Transform = {
                .Position = position,
                .Orientation = glm::angleAxis(
                    Random::Float(0.0f, (f32)std::numbers::pi), glm::normalize(Random::Float3(0.0f, 1.0f))),
                .Scale = glm::vec3{0.5f},}},
            GetFrameContext());
        LOG("{}", m_Scene.Geometry().CommandCount);
    }
    
    {
        // todo: as always everything in this file is somewhat temporary
        // todo: what I really want here is a single-time render graph, that can be queried to check if it has pending passes
        if (!m_HasExecutedSingleTimePasses)
        {
            ExecuteSingleTimePasses();
            m_HasExecutedSingleTimePasses = true;   
        }
    }

    {
        CPU_PROFILE_FRAME("Setup Render Graph")
        SetupRenderGraph();
        static u32 twice = 2;
        if (twice)
        {
            if (twice == 1)
                m_Graph->MermaidDumpHTML("../assets/render graph/graph.html");
            twice--;
        }
    }
    

    if (!m_FrameEarlyExit)
    {
        m_Graph->Compile(GetFrameContext());
        m_Graph->Execute(GetFrameContext());
        
        EndFrame();
        
        FrameMark; // tracy
    }
    else
    {
        m_FrameEarlyExit = false;
    }
}

void Renderer::OnUpdate()
{
    CPU_PROFILE_FRAME("On update")

    m_CameraController->OnUpdate(1.0f / 60.0f);
}

void Renderer::BeginFrame()
{
    CPU_PROFILE_FRAME("Begin frame")

    Device::BeginFrame(GetFrameContext());
    u32 frameNumber = GetFrameContext().FrameNumber;
    m_SwapchainImageIndex = Device::AcquireNextImage(m_Swapchain, frameNumber);
    if (m_SwapchainImageIndex == INVALID_SWAPCHAIN_IMAGE)
    {
        m_FrameEarlyExit = true;
        RecreateSwapchain();
        return;
    }
    
    m_CurrentFrameContext->DeletionQueue.Flush();

    CommandBuffer cmd = GetFrameContext().Cmd;
    Device::ResetCommandBuffer(cmd);
    Device::BeginCommandBuffer(cmd);

    GetFrameContext().CommandList.WaitOnBarrier({
        .DependencyInfo = Device::CreateDependencyInfo({
            .MemoryDependencyInfo = MemoryDependencyInfo{
                .SourceStage = PipelineStage::AllCommands,
                .DestinationStage = PipelineStage::AllCommands,
                .SourceAccess = PipelineAccess::WriteAll | PipelineAccess::WriteHost,
                .DestinationAccess = PipelineAccess::ReadAll}},
            GetFrameContext().DeletionQueue)});
    
    m_ResourceUploader.BeginFrame(GetFrameContext());
    ShaderCache::OnFrameBegin(GetFrameContext());
    ImGuiUI::BeginFrame(GetFrameContext().CommandList, GetFrameContext().FrameNumber);
}

RenderingInfo Renderer::GetImGuiUIRenderingInfo()
{
    const SwapchainDescription& swapchain = Device::GetSwapchainDescription(m_Swapchain);

    return Device::CreateRenderingInfo({
        .RenderArea = swapchain.SwapchainResolution,
        .ColorAttachments = {Device::CreateRenderingAttachment({
            .Description = ColorAttachmentDescription{
                .OnLoad = AttachmentLoad::Load,
                .OnStore = AttachmentStore::Store},
            .Image = swapchain.DrawImage,
            .Layout = ImageLayout::General},
            GetFrameContext().DeletionQueue)}},
        GetFrameContext().DeletionQueue);
}

void Renderer::EndFrame()
{
    ImGuiUI::EndFrame(GetFrameContext().CommandList, GetImGuiUIRenderingInfo());

    CommandBuffer cmd = GetFrameContext().Cmd;
    GetFrameContext().CommandList.PrepareSwapchainPresent({
        .Swapchain = m_Swapchain,
        .ImageIndex = m_SwapchainImageIndex});
    
    u32 frameNumber = GetFrameContext().FrameNumber;
    SwapchainFrameSync& sync = GetFrameContext().FrameSync;

    TracyVkCollect(ProfilerContext::Get()->GraphicsContext(), Device::GetProfilerCommandBuffer(ProfilerContext::Get()))

    m_ResourceUploader.SubmitUpload(GetFrameContext());

    Device::EndCommandBuffer(cmd);
    
    Device::SubmitCommandBuffer(cmd, QueueKind::Graphics, BufferSubmitSyncInfo{
        .WaitStages = {PipelineStage::ColorOutput},
        .WaitSemaphores = {sync.PresentSemaphore},
        .SignalSemaphores = {sync.RenderSemaphore},
        .Fence = sync.RenderFence});
    
    bool isFramePresentSuccessful = Device::Present(m_Swapchain, QueueKind::Presentation,
        frameNumber, m_SwapchainImageIndex); 
    bool shouldRecreateSwapchain = m_IsWindowResized || !isFramePresentSuccessful;
    if (shouldRecreateSwapchain)
        RecreateSwapchain();
    
    m_FrameNumber++;
    m_CurrentFrameContext = &m_FrameContexts[(u32)m_FrameNumber % BUFFERED_FRAMES];
    m_CurrentFrameContext->FrameNumberTick = m_FrameNumber;
    
    ProfilerContext::Get()->NextFrame();
}

void Renderer::InitRenderingStructures()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // do not create opengl context
    m_Window = glfwCreateWindow(1600, 900, "My window", nullptr, nullptr);
    glfwSetWindowUserPointer(m_Window, this);
    glfwSetFramebufferSizeCallback(m_Window, [](GLFWwindow* window, i32 width, i32 height)
    {
        Renderer* renderer = (Renderer*)glfwGetWindowUserPointer(window);
        renderer->OnWindowResize();
    });

    static constexpr bool ASYNC_COMPUTE = true;
    Device::Init(DeviceCreateInfo::Default(m_Window, ASYNC_COMPUTE));
    ImageUtils::DefaultTextures::Init();

    m_ResourceUploader.Init();
    
    m_Swapchain = Device::CreateSwapchain({}, Device::DummyDeletionQueue());
    const SwapchainDescription& swapchain = Device::GetSwapchainDescription(m_Swapchain);

    m_FrameContexts.resize(BUFFERED_FRAMES);
    for (u32 i = 0; i < BUFFERED_FRAMES; i++)
    {
        CommandPool pool = Device::CreateCommandPool({
            .QueueKind = QueueKind::Graphics,
            .PerBufferReset = true});

        m_FrameContexts[i].FrameSync = swapchain.Sync[i];
        m_FrameContexts[i].FrameNumber = i;
        m_FrameContexts[i].Resolution = swapchain.SwapchainResolution;

        m_FrameContexts[i].Cmd =  Device::CreateCommandBuffer({
            .Pool = pool,
            .Kind = CommandBufferKind::Primary});
        m_FrameContexts[i].CommandList.SetCommandBuffer(m_FrameContexts[i].Cmd);
        
        m_FrameContexts[i].ResourceUploader = &m_ResourceUploader;
    }

    std::array<CommandBuffer, BUFFERED_FRAMES> cmds;
    for (u32 i = 0; i < BUFFERED_FRAMES; i++)
        cmds[i] = m_FrameContexts[i].Cmd;
    ProfilerContext::Get()->Init(cmds);

    m_CurrentFrameContext = &m_FrameContexts.front();
}

void Renderer::Shutdown()
{
    Device::WaitIdle();

    Device::Destroy(m_Swapchain);

    m_SceneLights.reset();
    m_Graph.reset();
    m_ResourceUploader.Shutdown();
    ShaderCache::Shutdown();
    for (auto& ctx : m_FrameContexts)
        ctx.DeletionQueue.Flush();
    ProfilerContext::Get()->Shutdown();

    AssetManager::Shutdown();
    Device::Shutdown();
    glfwDestroyWindow(m_Window); // optional (glfwTerminate does same thing)
    glfwTerminate();
}

void Renderer::OnWindowResize()
{
    m_IsWindowResized = true;
}

void Renderer::RecreateSwapchain()
{
    m_IsWindowResized = false;    
    i32 width = 0, height = 0;
    glfwGetFramebufferSize(m_Window, &width, &height);
    while (width == 0 || height == 0)
    {
        glfwGetFramebufferSize(m_Window, &width, &height);
        glfwWaitEvents();
    }
    
    Device::WaitIdle();


    const SwapchainDescription& oldSwapchain = Device::GetSwapchainDescription(m_Swapchain);
    auto frameSync = oldSwapchain.Sync;
    Device::Destroy(m_Swapchain);
    
    m_Swapchain = Device::CreateSwapchain({
        .FrameSyncs = frameSync},
        Device::DummyDeletionQueue());

    const SwapchainDescription& swapchain = Device::GetSwapchainDescription(m_Swapchain);
    m_Graph->SetBackbuffer(swapchain.DrawImage);
    // todo: to multicast delegate
    m_Graph->OnResolutionChange();

    Input::s_MainViewportSize = swapchain.SwapchainResolution;
    m_Camera->SetViewport(swapchain.SwapchainResolution.x, swapchain.SwapchainResolution.y);
    for (auto& frameContext : m_FrameContexts)
        frameContext.Resolution = swapchain.SwapchainResolution;
}

const FrameContext& Renderer::GetFrameContext() const
{
    return *m_CurrentFrameContext;
}

FrameContext& Renderer::GetFrameContext()
{
    return *m_CurrentFrameContext;
}
