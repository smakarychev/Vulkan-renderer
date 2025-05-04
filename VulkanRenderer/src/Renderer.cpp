#include "Renderer.h"

#include <numbers>
#include <tracy/Tracy.hpp>

#include "AssetManager.h"
#include "CameraGPU.h"
#include "Converters.h"
#include "ShadingSettingsGPU.h"
#include "Core/Input.h"
#include "cvars/CVarSystem.h"

#include "GLFW/glfw3.h"
#include "Imgui/ImguiUI.h"
#include "Light/LightFrustumCuller.h"
#include "Light/LightZBinner.h"
#include "Light/SH.h"
#include "RenderGraph/Passes/AA/FxaaPass.h"
#include "RenderGraph/Passes/AO/SsaoBlurPass.h"
#include "RenderGraph/Passes/AO/SsaoPass.h"
#include "RenderGraph/Passes/AO/SsaoVisualizePass.h"
#include "RenderGraph/Passes/Atmosphere/AtmospherePass.h"
#include "RenderGraph/Passes/Atmosphere/SimpleAtmospherePass.h"
#include "RenderGraph/Passes/Atmosphere/Environment/AtmosphereEnvironmentPass.h"
#include "RenderGraph/Passes/Extra/SlimeMold/SlimeMoldPass.h"
#include "RenderGraph/Passes/SceneDraw/PBR/SceneForwardPbrPass.h"
#include "RenderGraph/Passes/Generated/MaterialsBindGroup.generated.h"
#include "RenderGraph/Passes/HiZ/HiZNVPass.h"
#include "RenderGraph/Passes/HiZ/HiZVisualize.h"
#include "RenderGraph/Passes/Lights/LightClustersBinPass.h"
#include "RenderGraph/Passes/Lights/LightClustersCompactPass.h"
#include "RenderGraph/Passes/Lights/LightClustersSetupPass.h"
#include "RenderGraph/Passes/Lights/LightTilesBinPass.h"
#include "RenderGraph/Passes/Lights/LightTilesSetupPass.h"
#include "RenderGraph/Passes/Lights/VisualizeLightClustersDepthLayersPass.h"
#include "RenderGraph/Passes/Lights/VisualizeLightClustersPass.h"
#include "RenderGraph/Passes/Lights/VisualizeLightTiles.h"
#include "RenderGraph/Passes/Scene/Visibility/PrepareVisibleMeshletInfoPass.h"
#include "RenderGraph/Passes/Scene/Visibility/SceneMultiviewMeshletVisibilityPass.h"
#include "RenderGraph/Passes/Scene/Visibility/SceneMultiviewRenderObjectVisibilityPass.h"
#include "RenderGraph/Passes/Scene/Visibility/SceneMultiviewVisibilityHiZPass.h"
#include "RenderGraph/Passes/SceneDraw/SceneMetaDrawPass.h"
#include "RenderGraph/Passes/SceneDraw/General/SceneDepthPrepassPass.h"
#include "RenderGraph/Passes/SceneDraw/Shadow/SceneCsmPass.h"
#include "RenderGraph/Passes/SceneDraw/Shadow/SceneDirectionalShadowPass.h"
#include "RenderGraph/Passes/SceneDraw/VBuffer/SceneVBufferPass.h"
#include "RenderGraph/Passes/SceneDraw/VBuffer/SceneVBufferPbrPass.h"
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
#include "RenderGraph/Passes/Utility/VisualizeDepthPass.h"
#include "Rendering/Shader/ShaderCache.h"
#include "Scene/BindlessTextureDescriptorsRingBuffer.h"
#include "Scene/Scene.h"
#include "String/StringId.h"

Renderer::Renderer() = default;

void Renderer::Init()
{
    StringIdRegistry::Init();
    m_ShaderCache.Init();
    
    InitRenderingStructures();
    Device::BeginFrame(GetFrameContext());

    Input::s_MainViewportSize = Device::GetSwapchainDescription(m_Swapchain).SwapchainResolution;
    m_Camera = std::make_shared<Camera>(CameraType::Perspective);
    m_CameraController = std::make_unique<CameraController>(m_Camera);
    for (auto& ctx : m_FrameContexts)
        ctx.PrimaryCamera = m_Camera.get();

    m_PersistentMaterialAllocator = Device::CreateDescriptorArenaAllocator({
        .Kind = DescriptorsKind::Materials,
        .Residence = DescriptorAllocatorResidence::CPU,
        .UsedTypes = {DescriptorType::UniformBuffer, DescriptorType::StorageBuffer, DescriptorType::Image},
        .DescriptorCount = 8192 * 4});

    std::array<DescriptorArenaAllocators, BUFFERED_FRAMES> allocators;
    for (u32 i = 0; i < BUFFERED_FRAMES; i++)
    {
        DescriptorArenaAllocator samplerAllocator = Device::CreateDescriptorArenaAllocator({
                .Kind = DescriptorsKind::Sampler,
                .Residence = DescriptorAllocatorResidence::CPU,
                .UsedTypes = {DescriptorType::Sampler},
                .DescriptorCount = 256 * 4});
            
        DescriptorArenaAllocator resourceAllocator = Device::CreateDescriptorArenaAllocator({
            .Kind = DescriptorsKind::Resource,
            .Residence = DescriptorAllocatorResidence::CPU,
            .UsedTypes = {DescriptorType::UniformBuffer, DescriptorType::StorageBuffer, DescriptorType::Image},
            .DescriptorCount = 8192 * 4});

        allocators[i] = DescriptorArenaAllocators(samplerAllocator, resourceAllocator, m_PersistentMaterialAllocator);
    }
    
    m_Graph = std::make_unique<RG::Graph>(allocators, &m_ShaderCache);
    InitRenderGraph();
}

void Renderer::InitRenderGraph()
{
    m_BindlessTextureDescriptorsRingBuffer = std::make_unique<BindlessTextureDescriptorsRingBuffer>(
        1024,
        m_ShaderCache.Allocate("materials"_hsv, m_Graph->GetFrameAllocators()).value());

    m_Graph->SetBackbuffer(Device::GetSwapchainDescription(m_Swapchain).DrawImage);

    m_ShaderCache.AddPersistentDescriptors("main_materials"_hsv,
        m_BindlessTextureDescriptorsRingBuffer->GetMaterialsShader().Descriptors(DescriptorsKind::Materials));
    
    m_SlimeMoldContext = std::make_shared<SlimeMoldContext>(
        SlimeMoldContext::RandomIn(Device::GetSwapchainDescription(m_Swapchain).SwapchainResolution,
            1, 5000000, *GetFrameContext().ResourceUploader));

    //SceneConverter::Convert(
    //    *CVars::Get().GetStringCVar("Path.Assets"_hsv),
    //    *CVars::Get().GetStringCVar("Path.Assets"_hsv) + "models/lights_test/scene.gltf");
    
    m_Scene = Scene::CreateEmpty(Device::DeletionQueue());
    m_SceneBucketList.Init(m_Scene);
    m_OpaqueSet.Init("Opaque"_hsv, m_Scene, m_SceneBucketList, {
        ScenePassCreateInfo{
            .Name = "DepthPrepass"_hsv,
            .BucketCreateInfos = {
                {
                    .Name = "Opaque material"_hsv,
                    .Filter = [](const SceneGeometryInfo& geometry, SceneRenderObjectHandle renderObject) {
                        const Material& material = geometry.MaterialsCpu[
                            geometry.RenderObjects[renderObject.Index].Material];
                        return enumHasAny(material.Flags, MaterialFlags::Opaque);
                    }
                }
            }
        },
        ScenePassCreateInfo{
            .Name = "Vbuffer"_hsv,
            .BucketCreateInfos = {
                {
                    .Name = "Opaque material"_hsv,
                    .Filter = [](const SceneGeometryInfo& geometry, SceneRenderObjectHandle renderObject) {
                        const Material& material = geometry.MaterialsCpu[
                            geometry.RenderObjects[renderObject.Index].Material];
                        return enumHasAny(material.Flags, MaterialFlags::Opaque);
                    }
                }
            }
        },
        ScenePassCreateInfo{
            .Name = "ForwardPbr"_hsv,
            .BucketCreateInfos = {
                {
                    .Name = "Opaque material"_hsv,
                    .Filter = [](const SceneGeometryInfo& geometry, SceneRenderObjectHandle renderObject) {
                        return renderObject.Index < 1;
                    },
                },
                {
                    .Name = "Opaque material2"_hsv,
                    .Filter = [](const SceneGeometryInfo& geometry, SceneRenderObjectHandle renderObject) {
                        const Material& material = geometry.MaterialsCpu[
                            geometry.RenderObjects[renderObject.Index].Material];
                        return renderObject.Index >= 1;
                    },
                    .ShaderOverrides = ShaderDefines({ShaderDefine("TEST"_hsv)})
                }
            }
        },
        Passes::SceneCsm::getScenePassCreateInfo("Shadow"_hsv),
    }, Device::DeletionQueue());

    m_MultiviewVisibility.Init(m_OpaqueSet);
    m_OpaqueSetPrimaryView = {
        .Name = "OpaquePrimary"_hsv,
        .Camera = GetFrameContext().PrimaryCamera,
        .Resolution = GetFrameContext().Resolution,
        .VisibilityFlags = SceneVisibilityFlags::IsPrimaryView | SceneVisibilityFlags::OcclusionCull};
    
    /* initial submit */
    Device::ImmediateSubmit([&](RenderCommandList& cmdList)
    {
        FrameContext ctx = GetFrameContext();
        ctx.CommandList = cmdList;
        m_TestScene = SceneInfo::LoadFromAsset(
            *CVars::Get().GetStringCVar("Path.Assets"_hsv) + "models/lights_test/scene.scene",
            *m_BindlessTextureDescriptorsRingBuffer, Device::DeletionQueue());
        SceneInstance instance = m_Scene.Instantiate(*m_TestScene, {
            .Transform = {
                .Position = glm::vec3{0.0f, -1.5f, -7.0f},
                .Orientation = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
                .Scale = glm::vec3{1.0f},}},
            ctx);


        ctx.ResourceUploader->SubmitUpload(ctx);
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
            .Mipmaps = Images::mipmapCount(glm::uvec2{equirectangularDescription.Width / 2}),
            .Format = Format::RGBA16_FLOAT,
            .Kind = ImageKind::Cubemap,
            .Usage = ImageUsage::Sampled | ImageUsage::Storage},
        .CalculateMipmaps = false});
    
    m_SkyboxPrefilterMap = Device::CreateImage({
        .Description = Passes::EnvironmentPrefilter::getPrefilteredTextureDescription(),
        .CalculateMipmaps = false});
    
    m_IrradianceSH = Device::CreateBuffer({
        .SizeBytes = sizeof(SH9Irradiance),
        .Usage = BufferUsage::Ordinary | BufferUsage::Storage | BufferUsage::Uniform});
    m_SkyIrradianceSH = Device::CreateBuffer({
        .SizeBytes = sizeof(SH9Irradiance),
        .Usage = BufferUsage::Ordinary | BufferUsage::Storage | BufferUsage::Uniform});

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
    Passes::ImGuiTexture::addToGraph("ImGuiTexture.Mold"_hsv, *m_Graph, slime.ColorOut);
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
    bindGroup.SetMaterials({.Buffer = m_Scene.Geometry().Materials.Buffer});

    Resource color = m_Graph->CreateResource("Color"_hsv, GraphTextureDescription{
        .Width = Device::GetSwapchainDescription(m_Swapchain).DrawResolution.x,
        .Height = Device::GetSwapchainDescription(m_Swapchain).DrawResolution.y,
        .Format = Format::RGBA16_FLOAT});
    Resource vbuffer = m_Graph->CreateResource("VBuffer"_hsv, GraphTextureDescription{
        .Width = Device::GetSwapchainDescription(m_Swapchain).DrawResolution.x,
        .Height = Device::GetSwapchainDescription(m_Swapchain).DrawResolution.y,
        .Format = Format::R32_UINT});
    Resource depth = m_Graph->CreateResource("Depth"_hsv, GraphTextureDescription{
        .Width = Device::GetSwapchainDescription(m_Swapchain).DrawResolution.x,
        .Height = Device::GetSwapchainDescription(m_Swapchain).DrawResolution.y,
        .Format = Format::D32_FLOAT});
    
    auto* depthPrepass = m_OpaqueSet.TryFindPass("DepthPrepass"_hsv);
    auto& pbrPass = m_OpaqueSet.FindPass("ForwardPbr"_hsv);
    auto& vbufferPass = m_OpaqueSet.FindPass("Vbuffer"_hsv);

    m_OpaqueSetPrimaryVisibility = m_MultiviewVisibility.AddVisibility(m_OpaqueSetPrimaryView);
    
    m_SceneVisibilityResources = SceneVisibilityPassesResources::FromSceneMultiviewVisibility(
        *m_Graph, m_MultiviewVisibility);

    std::vector<SceneDrawPassDescription> drawPasses;

    
    bool useForwardPass = CVars::Get().GetI32CVar("Renderer.UseForwardShading"_hsv).value_or(false);
    ImGui::Begin("ForwardShading");
    ImGui::Checkbox("Enabled", &useForwardPass);
    CVars::Get().SetI32CVar("Renderer.UseForwardShading"_hsv, useForwardPass);
    ImGui::End();
    
    if (useForwardPass)
    {
        if (CVars::Get().GetI32CVar("Renderer.DepthPrepass"_hsv).value_or(false))
        {
            depth = RenderGraphDepthPrepass(*depthPrepass);
            RenderGraphOnFrameDepthGenerated(depthPrepass->Name(), depth);
        }
        
        drawPasses.push_back(RenderGraphForwardPbrDescription(color, depth, pbrPass));
    }
    else
    {
        drawPasses.push_back(RenderGraphVBufferDescription(vbuffer, depth, vbufferPass));
    }
    
    m_SceneVisibilityResources.UpdateFromSceneMultiviewVisibility(*m_Graph, m_MultiviewVisibility);
    auto& metaUgb = Passes::SceneMetaDraw::addToGraph("MetaUgb"_hsv,
        *m_Graph, {
            .MultiviewVisibility = &m_MultiviewVisibility,
            .Resources = &m_SceneVisibilityResources,
            .DrawPasses = drawPasses});

    if (useForwardPass)
    {
        color = metaUgb.DrawPassViewAttachments.Get(
            m_OpaqueSetPrimaryView.Name, pbrPass.Name()).Colors[0].Resource;
    }
    else
    {
        depth = metaUgb.DrawPassViewAttachments.Get(
            m_OpaqueSetPrimaryView.Name, vbufferPass.Name()).Depth->Resource;
        vbuffer = metaUgb.DrawPassViewAttachments.Get(
            m_OpaqueSetPrimaryView.Name, vbufferPass.Name()).Colors[0].Resource;

        RenderGraphOnFrameDepthGenerated(depthPrepass->Name(), depth);
        color = RenderGraphVBufferPbr(vbuffer, primaryCamera);
    }

    Resource colorWithSkybox = RenderGraphSkyBox(color, depth);
    auto& fxaa = Passes::Fxaa::addToGraph("FXAA"_hsv, *m_Graph, colorWithSkybox);
    
    Passes::CopyTexture::addToGraph("Copy.MainColor"_hsv, *m_Graph, {
        .TextureIn = fxaa.AntiAliased,
        .TextureOut = backbuffer
    });

    /*auto& csmVisualize = Passes::VisualizeCSM::addToGraph("CsmVisualize"_hsv, *m_Graph, {
        .ShadowMap = metaOutput.DrawPassViewAttachments.Get(
            sceneCsmInitOutput.MetaPassDescriptions.front().View.Name,
            m_OpaqueSet.FindPass("Shadow"_hsv).Name()).Depth->Resource,
        .CSM = sceneCsmInitOutput.CsmInfo,
        .Near = sceneCsmInitOutput.Near,
        .Far = sceneCsmInitOutput.Far}, {});
    auto& csmVisualizeOutput = blackboard.Get<Passes::VisualizeCSM::PassData>(csmVisualize);
    Passes::ImGuiTexture::addToGraph("CSM.Texture"_hsv, *m_Graph, csmVisualizeOutput.ColorOut);*/
 
    
    // todo: move to proper place (this is just testing atm)
    /*
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
    */

    ImGui::Begin("Debug");
    if (ImGui::Button("Dump memory stats"))
        Device::DumpMemoryStats("./MemoryStats.json");
    ImGui::End();
    
    //SetupRenderSlimePasses();
}

RG::Resource Renderer::RenderGraphDepthPrepass(const ScenePass& scenePass)
{
    using namespace RG;
    
    Resource depth = m_Graph->CreateResource("Depth"_hsv, GraphTextureDescription{
        .Width = Device::GetSwapchainDescription(m_Swapchain).DrawResolution.x,
        .Height = Device::GetSwapchainDescription(m_Swapchain).DrawResolution.y,
        .Format = Format::D32_FLOAT});
    
    auto& metaPass = Passes::SceneMetaDraw::addToGraph("MetaDepthPrepass"_hsv,
        *m_Graph, {
            .MultiviewVisibility = &m_MultiviewVisibility,
            .Resources = &m_SceneVisibilityResources,
            .DrawPasses = {RenderGraphDepthPrepassDescription(depth, scenePass)}
        });

    return metaPass.DrawPassViewAttachments.Get(m_OpaqueSetPrimaryView.Name, scenePass.Name()).Depth->Resource;
}

SceneDrawPassDescription Renderer::RenderGraphDepthPrepassDescription(RG::Resource& depth, const ScenePass& scenePass)
{
    using namespace RG;
    
    auto initDepthPrepass = [&](StringId name, Graph& graph, const SceneDrawPassExecutionInfo& info)
    {
        auto& pass = Passes::SceneDepthPrepass::addToGraph(
            name.Concatenate(".DepthPrepass"), graph, {
                .DrawInfo = info,
                .Geometry = &m_Scene.Geometry()});

        depth = *pass.Resources.Attachments.Depth;

        return pass.Resources.Attachments;
    };
    
    DrawAttachments attachments = {
        .Depth = DepthStencilAttachment{
            .Resource = depth,
            .Description = {
                .OnLoad = AttachmentLoad::Clear,
                .ClearDepthStencil = {.Depth = 0.0f, .Stencil = 0}
            }
        }
    };
    
    return {
        .Pass = &scenePass,
        .DrawPassInit = initDepthPrepass, 
        .View = m_OpaqueSetPrimaryView,
        .Visibility = m_OpaqueSetPrimaryVisibility,
        .Attachments = attachments
    };
}

SceneDrawPassDescription Renderer::RenderGraphForwardPbrDescription(RG::Resource& color, RG::Resource& depth,
    const ScenePass& scenePass)
{
    using namespace RG;

    auto initForwardPbr = [&](StringId name, Graph& graph, const SceneDrawPassExecutionInfo& info)
    {
        const bool useSky = false;
        
        Passes::SceneForwardPbr::ExecutionInfo executionInfo = {
            .DrawInfo = info,
            .Geometry = &m_Scene.Geometry(),
            .Lights = &m_Scene.Lights(),
            .SSAO = {.SSAO = m_Ssao},
            .IBL = {
                .IrradianceSH = m_Graph->AddExternal("IrradianceSH"_hsv, useSky ? m_SkyIrradianceSH : m_IrradianceSH),
                .PrefilterEnvironment = m_Graph->AddExternal("PrefilterMap"_hsv, m_SkyboxPrefilterMap),
                .BRDF = m_Graph->AddExternal("BRDF"_hsv, m_BRDFLut)
            },
            .Clusters = m_ClusterLightsInfo.Clusters,
            .Tiles = m_TileLightsInfo.Tiles,
            .ZBins = m_TileLightsInfo.ZBins,
        };
        if (CVars::Get().GetI32CVar("Renderer.DepthPrepass"_hsv).value_or(false))
            executionInfo.CommonOverrides = ShaderPipelineOverrides({.DepthTest = DepthTest::Equal});
        
        auto& pass = Passes::SceneForwardPbr::addToGraph(name.Concatenate(".UGB"), graph, executionInfo);

        return pass.Resources.Attachments;
    };

    AttachmentLoad depthOnLoad = AttachmentLoad::Clear;
    AttachmentStore depthOnStore = AttachmentStore::Store;

    if (CVars::Get().GetI32CVar("Renderer.DepthPrepass"_hsv).value_or(false))
    {
        depthOnLoad = AttachmentLoad::Load;
        depthOnStore = AttachmentStore::Unspecified;
    }
    
    DrawAttachments attachments = {
        .Colors = {
            DrawAttachment{
                .Resource = color,
                .Description = {
                    .OnLoad = AttachmentLoad::Clear,
                    .ClearColor = {.F = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)}
                }
            }
        },
        .Depth = DepthStencilAttachment{
            .Resource = depth,
            .Description = {
                .OnLoad = depthOnLoad,
                .OnStore = depthOnStore,
                .ClearDepthStencil = {.Depth = 0.0f, .Stencil = 0}
            }
        }
    };

    return {
        .Pass = &scenePass,
        .DrawPassInit = initForwardPbr, 
        .View = m_OpaqueSetPrimaryView,
        .Visibility = m_OpaqueSetPrimaryVisibility,
        .Attachments = attachments
    };
}

SceneDrawPassDescription Renderer::RenderGraphVBufferDescription(RG::Resource& vbuffer, RG::Resource& depth,
    const ScenePass& scenePass)
{
    using namespace RG;

    auto initVbuffer = [&](StringId name, Graph& graph, const SceneDrawPassExecutionInfo& info)
    {
        Passes::SceneVBuffer::ExecutionInfo executionInfo = {
            .DrawInfo = info,
            .Geometry = &m_Scene.Geometry()
        };
        
        auto& pass = Passes::SceneVBuffer::addToGraph(name.Concatenate(".VBuffer"), graph, executionInfo);

        vbuffer = pass.Resources.Attachments.Colors[0];
        depth = *pass.Resources.Attachments.Depth;

        return pass.Resources.Attachments;
    };

    DrawAttachments attachments = {
        .Colors = {
            DrawAttachment{
                .Resource = vbuffer,
                .Description = {
                    .OnLoad = AttachmentLoad::Clear,
                    .ClearColor = {.U = glm::uvec4(std::numeric_limits<u32>::max(), 0, 0, 0)},
                }
            }
        },
        .Depth = DepthStencilAttachment{
            .Resource = depth,
            .Description = {
                .OnLoad = AttachmentLoad::Clear,
                .ClearDepthStencil = {.Depth = 0.0f, .Stencil = 0}
            }
        }
    };

    return {
        .Pass = &scenePass,
        .DrawPassInit = initVbuffer, 
        .View = m_OpaqueSetPrimaryView,
        .Visibility = m_OpaqueSetPrimaryVisibility,
        .Attachments = attachments
    };
}

RG::Resource Renderer::RenderGraphVBufferPbr(RG::Resource& vbuffer, RG::Resource camera)
{
    const bool useSky = false;
    
    auto& pbr = Passes::SceneVBufferPbr::addToGraph("VBufferPbr"_hsv, *m_Graph, {
        .Geometry = &m_Scene.Geometry(),
        .VisibilityTexture = vbuffer,
        .Camera = camera,
        .Lights = &m_Scene.Lights(),
        .SSAO = {.SSAO = m_Ssao},
        .IBL = {
            .IrradianceSH = m_Graph->AddExternal("IrradianceSH"_hsv, useSky ? m_SkyIrradianceSH : m_IrradianceSH),
            .PrefilterEnvironment = m_Graph->AddExternal("PrefilterMap"_hsv, m_SkyboxPrefilterMap),
            .BRDF = m_Graph->AddExternal("BRDF"_hsv, m_BRDFLut)
        },
        .Clusters = m_ClusterLightsInfo.Clusters,
        .Tiles = m_TileLightsInfo.Tiles,
        .ZBins = m_TileLightsInfo.ZBins,
    });

    return pbr.Color;
}

void Renderer::RenderGraphOnFrameDepthGenerated(StringId passName, RG::Resource depth)
{
    const bool tileLights = *CVars::Get().GetI32CVar("Lights.Bin.Tiles"_hsv) == 1;
    const bool clusterLights = *CVars::Get().GetI32CVar("Lights.Bin.Clusters"_hsv) == 1;
    
    m_Ssao = RenderGraphSSAO(passName, depth);
    m_TileLightsInfo = {};
    m_ClusterLightsInfo = {};
    if (tileLights)
        m_TileLightsInfo = RenderGraphCullLightsTiled(passName, depth);
    if (clusterLights)
        m_ClusterLightsInfo = RenderGraphCullLightsClustered(passName, depth);
}

RG::Resource Renderer::RenderGraphSSAO(StringId baseName, RG::Resource depth)
{
    using namespace RG;

    auto& blackboard = m_Graph->GetBlackboard();
    
    auto& ssao = Passes::Ssao::addToGraph(baseName.Concatenate("SSAO"), *m_Graph, {
        .Depth = depth,
        .MaxSampleCount = 32});

    auto& ssaoBlurHorizontal = Passes::SsaoBlur::addToGraph(baseName.Concatenate("SSAO.Blur.Horizontal"), *m_Graph, {
        .SsaoIn = ssao.SSAO,
        .SsaoOut = {},
        .BlurKind = SsaoBlurPassKind::Horizontal});
    auto& ssaoBlurVertical = Passes::SsaoBlur::addToGraph(baseName.Concatenate("SSAO.Blur.Vertical"), *m_Graph, {
        .SsaoIn = ssaoBlurHorizontal.SsaoOut,
        .SsaoOut = ssao.SSAO,
        .BlurKind = SsaoBlurPassKind::Vertical});

    auto& ssaoVisualize = Passes::SsaoVisualize::addToGraph(baseName.Concatenate("SSAO.Visualize"), *m_Graph,
        ssaoBlurVertical.SsaoOut);

    Passes::ImGuiTexture::addToGraph(baseName.Concatenate("SSAO.Texture"), *m_Graph, ssaoVisualize.Color);

    return ssaoBlurVertical.SsaoOut;
}

Renderer::TileLightsInfo Renderer::RenderGraphCullLightsTiled(StringId baseName, RG::Resource depth)
{
    using namespace RG;
    
    auto& blackboard = m_Graph->GetBlackboard();
    
    struct TileLightsInfo
    {
        Resource Tiles{};
        Resource ZBins{};
    };

    auto zbins = LightZBinner::ZBinLights(m_Scene.Lights(), *GetFrameContext().PrimaryCamera);
    Resource zbinsResource = Passes::Upload::addToGraph(baseName.Concatenate("Upload.Light.ZBins"), *m_Graph,
        zbins.Bins);
    auto& tilesSetup = Passes::LightTilesSetup::addToGraph(baseName.Concatenate("Tiles.Setup"), *m_Graph);
    auto& binLightsTiles = Passes::LightTilesBin::addToGraph(baseName.Concatenate("Tiles.Bin"), *m_Graph, {
        .Tiles = tilesSetup.Tiles,
        .Depth = depth,
        .Light = &m_Scene.Lights()});
    auto& visualizeTiles = Passes::LightTilesVisualize::addToGraph(baseName.Concatenate("Tiles.Visualize"), *m_Graph, {
        .Tiles = tilesSetup.Tiles,
        .Bins = zbinsResource,
        .Depth = depth});
    Passes::ImGuiTexture::addToGraph(baseName.Concatenate("Tiles.Visualize.Texture"), *m_Graph,
        visualizeTiles.Color);

    return {
        .Tiles = binLightsTiles.Tiles,
        .ZBins = zbinsResource};
}

Renderer::ClusterLightsInfo Renderer::RenderGraphCullLightsClustered(StringId baseName, RG::Resource depth)
{
    using namespace RG;
    
    auto& blackboard = m_Graph->GetBlackboard();
    
    struct ClusterLightsInfo
    {
        Resource Clusters{};
    };
    
    auto& clustersSetup = Passes::LightClustersSetup::addToGraph(baseName.Concatenate("Clusters.Setup"), *m_Graph);
    auto& compactClusters = Passes::LightClustersCompact::addToGraph(baseName.Concatenate("Clusters.Compact"),
        *m_Graph, {
        .Clusters = clustersSetup.Clusters,
        .ClusterVisibility = clustersSetup.ClusterVisibility,
        .Depth = depth});
    auto& binLightsClusters = Passes::LightClustersBin::addToGraph(baseName.Concatenate("Clusters.Bin"), *m_Graph, {
        .DispatchIndirect = compactClusters.DispatchIndirect,
        .Clusters = compactClusters.Clusters,
        .ActiveClusters = compactClusters.ActiveClusters,
        .ClustersCount = compactClusters.ActiveClustersCount,
        .Light = &m_Scene.Lights()});

    auto& visualizeClusters = Passes::LightClustersVisualize::addToGraph(baseName.Concatenate("Clusters.Visualize"),
        *m_Graph, {
        .Clusters = binLightsClusters.Clusters,
        .Depth = depth});
    Passes::ImGuiTexture::addToGraph(baseName.Concatenate("Clusters.Visualize.Texture"), *m_Graph,
        visualizeClusters.Color);

    return {
        .Clusters = binLightsClusters.Clusters};
}

RG::Resource Renderer::RenderGraphSkyBox(RG::Resource color, RG::Resource depth)
{
    auto& skybox = Passes::Skybox::addToGraph("Skybox"_hsv, *m_Graph, {
        .SkyboxTexture = m_SkyboxTexture,
        .Color = color,
        .Depth = depth,
        .Resolution = GetFrameContext().Resolution});

    return skybox.Color;
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
    LightFrustumCuller::CullDepthSort(m_Scene.Lights(), *GetFrameContext().PrimaryCamera);
    m_Scene.Hierarchy().OnUpdate(m_Scene, GetFrameContext());
    m_Scene.Lights().OnUpdate(GetFrameContext());
    m_OpaqueSet.OnUpdate(GetFrameContext());
    m_MultiviewVisibility.OnUpdate(GetFrameContext());

    if (Input::GetKey(Key::Space))
    {
        glm::vec3 position = m_Camera->GetPosition() + m_Camera->GetForward() * 15.0f;
        SceneInstance instance = m_Scene.Instantiate(*m_TestScene, {
            .Transform = {
                .Position = position,
                .Orientation = glm::angleAxis(
                    Random::Float(0.0f, (f32)std::numbers::pi), glm::normalize(Random::Float3(0.0f, 1.0f))),
                .Scale = glm::vec3{0.5f},}},
            GetFrameContext());
        LOG("Meshes: {}\tMeshlets: {}\tTriangles: {}",
            m_OpaqueSet.RenderObjectCount(), m_OpaqueSet.MeshletCount(), m_OpaqueSet.TriangleCount());
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
    m_Graph->OnFrameBegin(GetFrameContext());
    m_ShaderCache.OnFrameBegin(GetFrameContext());
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
    Images::Default::Init();

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

    m_Graph.reset();
    m_MultiviewVisibility.Shutdown();
    m_ResourceUploader.Shutdown();
    m_ShaderCache.Shutdown();
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
