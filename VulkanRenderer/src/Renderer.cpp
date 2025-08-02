#include "Renderer.h"

#include <numbers>
#include <tracy/Tracy.hpp>

#include "AssetManager.h"
#include "ViewInfoGPU.h"
#include "Converters.h"
#include "Core/Input.h"
#include "cvars/CVarSystem.h"

#include "GLFW/glfw3.h"
#include "Imgui/ImguiUI.h"
#include "Light/LightFrustumCuller.h"
#include "Light/LightZBinner.h"
#include "Light/SH.h"
#include "Math/Random.h"
#include "RenderGraph/RGCommon.h"
#include "RenderGraph/Passes/AA/FxaaPass.h"
#include "RenderGraph/Passes/AO/SsaoBlurPass.h"
#include "RenderGraph/Passes/AO/SsaoPass.h"
#include "RenderGraph/Passes/AO/SsaoVisualizePass.h"
#include "RenderGraph/Passes/Atmosphere/AtmosphereAerialPerspectiveLutPass.h"
#include "RenderGraph/Passes/Atmosphere/AtmospherePass.h"
#include "RenderGraph/Passes/Atmosphere/AtmosphereRaymarchPass.h"
#include "RenderGraph/Passes/Atmosphere/SimpleAtmospherePass.h"
#include "RenderGraph/Passes/Atmosphere/Environment/AtmosphereEnvironmentPass.h"
#include "RenderGraph/Passes/Clouds/CloudCurlNoisePass.h"
#include "RenderGraph/Passes/Clouds/CloudReprojectPass.h"
#include "RenderGraph/Passes/Clouds/CloudShapeNoisePass.h"
#include "RenderGraph/Passes/Clouds/VerticalProfile/VPCloudsPass.h"
#include "RenderGraph/Passes/Clouds/VerticalProfile/VPCloudCoveragePass.h"
#include "RenderGraph/Passes/Clouds/VerticalProfile/VPCloudProfileMapPass.h"
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
    
    m_Graph = std::make_unique<RG::Graph>(allocators, m_ShaderCache);
    m_MermaidExporter = std::make_unique<RG::RGMermaidExporter>();
    InitRenderGraph();
}

void Renderer::InitRenderGraph()
{
    m_BindlessTextureDescriptorsRingBuffer = std::make_unique<BindlessTextureDescriptorsRingBuffer>(
        1024,
        m_ShaderCache.Allocate("materials"_hsv, m_Graph->GetFrameAllocators()).value());
    m_TransmittanceLutBindlessIndex = m_BindlessTextureDescriptorsRingBuffer->AddTexture(
        Images::Default::GetCopy(Images::DefaultKind::White, Device::DeletionQueue()));
    m_SkyViewLutBindlessIndex = m_BindlessTextureDescriptorsRingBuffer->AddTexture(
        Images::Default::GetCopy(Images::DefaultKind::White, Device::DeletionQueue()));

    // todo: this is currently a rgb image, which is wasteful, I need to provide format hints to image converter
    m_BlueNoiseBindlessIndex = m_BindlessTextureDescriptorsRingBuffer->AddTexture(
        Device::CreateImage({
            .DataSource = "../assets/textures/blue_noise_128.tx",
            .Description = {.Usage = ImageUsage::Sampled | ImageUsage::Storage}
        }));

    m_ShaderCache.AddPersistentDescriptors("main_materials"_hsv,
        m_BindlessTextureDescriptorsRingBuffer->GetMaterialsShader().Descriptors(DescriptorsKind::Materials),
        m_BindlessTextureDescriptorsRingBuffer->GetMaterialsShader().DescriptorsLayout(DescriptorsKind::Materials));
    
    /*m_SlimeMoldContext = std::make_shared<SlimeMoldContext>(
        SlimeMoldContext::RandomIn(Device::GetSwapchainDescription(m_Swapchain).SwapchainResolution,
            1, 5000000, *GetFrameContext().ResourceUploader));*/

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

    m_ShadowMultiviewVisibility.Init(m_OpaqueSet);
    m_PrimaryVisibility.Init(m_OpaqueSet);
    
    /* initial submit */
    Device::ImmediateSubmit([&](RenderCommandList& cmdList)
    {
        FrameContext ctx = GetFrameContext();
        ctx.CommandList = cmdList;
        m_TestScene = SceneInfo::LoadFromAsset(
            *CVars::Get().GetStringCVar("Path.Assets"_hsv) + "models/death_valley/scene.scene", 
            *m_BindlessTextureDescriptorsRingBuffer, Device::DeletionQueue());
        SceneInstance instance = m_Scene.Instantiate(*m_TestScene, {
            .Transform = {
                .Position = glm::vec3{0.0f, -1.5f, -7.0f},
                .Orientation = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
                .Scale = glm::vec3{10.0f},}},
            ctx);

        SceneInfo lights = {};
        lights.AddLight({
            .Direction = glm::normalize(glm::vec3(0.3f, -1.0f, 0.1f)),
            .Color = glm::vec3(1.0f, 1.0f, 1.0f),
            .Intensity = 2.5f,
        });
        constexpr u32 POINT_LIGHT_COUNT = 0;
        for (u32 i = 0; i < POINT_LIGHT_COUNT; i++)
            lights.AddLight({
                .Position = glm::vec3{Random::Float(-39.0f, 39.0f), Random::Float(0.0f, 4.0f), Random::Float(-19.0f, 19.0f)},
                .Color = Random::Float3(0.0f, 1.0f),
                .Intensity = Random::Float(0.5f, 1.7f),
                .Radius = Random::Float(0.5f, 8.6f)
            });
        m_Scene.Instantiate(lights, {}, ctx);

        ctx.ResourceUploader->SubmitUpload(ctx);
    });

    m_Graph->SetWatcher(*m_MermaidExporter);
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
        .Description = Passes::EnvironmentPrefilter::getPrefilteredTextureDescription(
            *CVars::Get().GetI32CVar("Renderer.IBL.PrefilterResolution"_hsv)),
        .CalculateMipmaps = false});
    
    m_SkyPrefilterMap = Device::CreateImage({
        .Description = Passes::EnvironmentPrefilter::getPrefilteredTextureDescription(
            *CVars::Get().GetI32CVar("Renderer.IBL.PrefilterResolutionRealtime"_hsv)),
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
    
    m_Graph->Reset();

    RG::Resource cubemap = Passes::EquirectangularToCubemap::addToGraph("Scene.Skybox"_hsv, *m_Graph,
        equirectangular, m_SkyboxTexture).Cubemap;
    Passes::DiffuseIrradianceSH::addToGraph(
        "Scene.DiffuseIrradianceSH"_hsv, *m_Graph, cubemap, m_IrradianceSH, false);
    Passes::EnvironmentPrefilter::addToGraph(
        "Scene.EnvironmentPrefilter"_hsv, *m_Graph, cubemap, m_SkyboxPrefilterMap, false);
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
    
    m_Graph->Reset();
    m_Graph->SetBackbufferImage(Device::GetSwapchainDescription(m_Swapchain).DrawImage);

    UpdateGlobalRenderGraphResources();

    Resource backbuffer = m_Graph->GetBackbufferImage();
    
    MaterialsShaderBindGroup bindGroup(m_BindlessTextureDescriptorsRingBuffer->GetMaterialsShader());
    bindGroup.SetMaterials({.Buffer = m_Scene.Geometry().Materials.Buffer});

    Resource color = m_Graph->Create("Color"_hsv, ResourceCreationFlags::AutoUpdate, RGImageDescription{
        .Inference = RGImageInference::Size2d,
        .Reference = backbuffer,
        .Format = Format::RGBA16_FLOAT});
    Resource vbuffer = m_Graph->Create("VBuffer"_hsv, ResourceCreationFlags::AutoUpdate, RGImageDescription{
        .Inference = RGImageInference::Size2d,
        .Reference = backbuffer,
        .Format = Format::R32_UINT});
    Resource depth = m_Graph->Create("Depth"_hsv, ResourceCreationFlags::AutoUpdate, RGImageDescription{
        .Inference = RGImageInference::Size2d,
        .Reference = backbuffer,
        .Format = Format::D32_FLOAT});
    
    auto& shadowPass = m_OpaqueSet.FindPass("Shadow"_hsv);
    auto* depthPrepass = m_OpaqueSet.TryFindPass("DepthPrepass"_hsv);
    auto& pbrPass = m_OpaqueSet.FindPass("ForwardPbr"_hsv);
    auto& vbufferPass = m_OpaqueSet.FindPass("Vbuffer"_hsv);
    
    std::vector<SceneDrawPassDescription> drawPasses;

    const CommonLight* light = nullptr;
    m_Scene.IterateLights(LightType::Directional, [&light](CommonLight& commonLight, Transform3d& localTransform) {
        light = &commonLight;
        ImGui::Begin("Directional Light");
        glm::vec3 euler = glm::eulerAngles(localTransform.Orientation) * 180.0f / glm::pi<f32>();
        ImGui::DragFloat3("Direction", &euler[0], 1e-1f);
        ImGui::ColorEdit3("Color", &commonLight.Color[0]);
        ImGui::DragFloat("Intensity", &commonLight.Intensity, 1e-2f, 0.0f);
        ImGui::End();
        localTransform.Orientation = glm::quat(euler * glm::pi<f32>() / 180.0f);
        
        return true;
    });

    CsmData csmData = {};
    if (light != nullptr)
        csmData = RenderGraphShadows(shadowPass, *light);

    m_OpaqueSetPrimaryView = {
        .Name = "OpaquePrimary"_hsv,
        .ViewInfo = m_Graph->GetGlobalResources().PrimaryViewInfo};
    
    m_OpaqueSetPrimaryVisibility = m_PrimaryVisibility.AddVisibility(m_OpaqueSetPrimaryView);
    m_PrimaryVisibilityResources = SceneVisibilityPassesResources::FromSceneMultiviewVisibility(
        *m_Graph, m_PrimaryVisibility);

    bool renderAtmosphere = CVars::Get().GetI32CVar("Renderer.Atmosphere"_hsv).value_or(false);
    ImGui::Begin("RenderAtmosphere");
    ImGui::Checkbox("Enabled", &renderAtmosphere);
    CVars::Get().SetI32CVar("Renderer.Atmosphere"_hsv, renderAtmosphere);
    ImGui::End();
    
    Passes::Atmosphere::LutPasses::PassData* atmosphereLuts = nullptr;
    if (renderAtmosphere)
    {
        atmosphereLuts = &RenderGraphAtmosphereLutPasses();
        RenderGraphAtmosphereEnvironment(*atmosphereLuts);
    }

    bool useForwardPass = CVars::Get().GetI32CVar("Renderer.UseForwardShading"_hsv).value_or(false);
    ImGui::Begin("ForwardShading");
    ImGui::Checkbox("Enabled", &useForwardPass);
    CVars::Get().SetI32CVar("Renderer.UseForwardShading"_hsv, useForwardPass);
    ImGui::End();
    
    if (useForwardPass)
    {
        if (CVars::Get().GetI32CVar("Renderer.DepthPrepass"_hsv).value_or(false))
        {
            RenderGraphDepthPrepass(depth, *depthPrepass);
            RenderGraphOnFrameDepthGenerated(depthPrepass->Name(), depth);
        }
        
        drawPasses.push_back(RenderGraphForwardPbrDescription(color, depth, csmData, pbrPass));
    }
    else
    {
        drawPasses.push_back(RenderGraphVBufferDescription(vbuffer, depth, vbufferPass));
    }

    auto& metaUgb = Passes::SceneMetaDraw::addToGraph("MetaUgb"_hsv,
        *m_Graph, {
            .MultiviewVisibility = &m_PrimaryVisibility,
            .Resources = &m_PrimaryVisibilityResources,
            .DrawPasses = drawPasses});

    std::swap(
        m_MinMaxDepthReductionsNextFrame[GetFrameContext().FrameNumber],
        m_MinMaxDepthReductions[GetFrameContext().FrameNumber]);
    m_Graph->MarkBufferForExport(metaUgb.DrawPassViewAttachments.GetMinMaxDepthReduction(m_OpaqueSetPrimaryView.Name),
        BufferUsage::Readback);

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
        color = RenderGraphVBufferPbr(vbuffer, m_Graph->GetGlobalResources().PrimaryViewInfoResource, csmData);
    }

    Resource colorWithSky{};
    CloudMapsInfo cloudMaps = {};
    CloudsInfo clouds = {};
    if (renderAtmosphere)
    {
        auto& aerialPerspective = Passes::Atmosphere::AerialPerspective::addToGraph("AtmosphereAerialPerspective"_hsv,
            *m_Graph, {
            .ViewInfo = m_Graph->GetGlobalResources().PrimaryViewInfoResource,
            .TransmittanceLut = atmosphereLuts->TransmittanceLut,
            .MultiscatteringLut = atmosphereLuts->MultiscatteringLut,
            .Light = &m_Scene.Lights(),
            .CsmData = csmData
        });
        
        cloudMaps = RenderGraphGetCloudMaps();

        clouds = RenderGraphClouds(cloudMaps, color, aerialPerspective.AerialPerspective, depth);
        colorWithSky = RenderGraphAtmosphere(*atmosphereLuts, aerialPerspective.AerialPerspective,
            color, depth, csmData, clouds.Color, clouds.Depth);
    }
    else
    {
        colorWithSky = RenderGraphSkyBox(color, depth);
    }

    auto& fxaa = Passes::Fxaa::addToGraph("FXAA"_hsv, *m_Graph, colorWithSky);

    Passes::CopyTexture::addToGraph("Copy.MainColor"_hsv, *m_Graph, {
        .TextureIn = fxaa.AntiAliased,
        .TextureOut = backbuffer
    });

    ImGui::Begin("Debug");
    if (ImGui::Button("Dump memory stats"))
        Device::DumpMemoryStats("./MemoryStats.json");
    ImGui::End();

    m_Graph->ClaimBuffer(
        metaUgb.DrawPassViewAttachments.GetMinMaxDepthReduction(m_OpaqueSetPrimaryView.Name),
        m_MinMaxDepthReductionsNextFrame[GetFrameContext().FrameNumber],
        Device::DeletionQueue());

    if (atmosphereLuts)
    {
        m_Graph->MarkImageForExport(atmosphereLuts->TransmittanceLut);
        m_Graph->ClaimImage(atmosphereLuts->TransmittanceLut, m_TransmittanceLut, Device::DeletionQueue());

        m_Graph->MarkImageForExport(atmosphereLuts->SkyViewLut);
        m_Graph->ClaimImage(atmosphereLuts->SkyViewLut, m_SkyViewLut, Device::DeletionQueue());

        m_BindlessTextureDescriptorsRingBuffer->SetTexture(m_TransmittanceLutBindlessIndex, m_TransmittanceLut);
        m_BindlessTextureDescriptorsRingBuffer->SetTexture(m_SkyViewLutBindlessIndex, m_SkyViewLut);
    }

    if (renderAtmosphere)
    {
        if (!m_CloudCoverage.HasValue())
            m_Graph->ClaimImage(cloudMaps.Coverage, m_CloudCoverage, Device::DeletionQueue());
        if (!m_CloudProfileMap.HasValue())
            m_Graph->ClaimImage(cloudMaps.Profile, m_CloudProfileMap, Device::DeletionQueue());
        if (!m_CloudShapeLowFrequency.HasValue())
            m_Graph->ClaimImage(cloudMaps.ShapeLowFrequency, m_CloudShapeLowFrequency, Device::DeletionQueue());
        if (!m_CloudShapeHighFrequency.HasValue())
            m_Graph->ClaimImage(cloudMaps.ShapeHighFrequency, m_CloudShapeHighFrequency, Device::DeletionQueue());
        if (!m_CloudCurlNoise.HasValue())
            m_Graph->ClaimImage(cloudMaps.CurlNoise, m_CloudCurlNoise, Device::DeletionQueue());

        if (m_CloudsReprojectionEnabled && !m_CloudColorAccumulation.front().HasValue())
        {
            m_Graph->MarkImageForExport(clouds.ColorPrevious, ImageUsage::Storage | ImageUsage::Sampled | ImageUsage::Source);
            m_Graph->MarkImageForExport(clouds.DepthPrevious, ImageUsage::Storage | ImageUsage::Sampled | ImageUsage::Source);
            m_Graph->MarkImageForExport(clouds.ReprojectionPrevious, ImageUsage::Storage | ImageUsage::Sampled | ImageUsage::Source);
            m_Graph->MarkImageForExport(clouds.Color, ImageUsage::Storage | ImageUsage::Sampled | ImageUsage::Source);
            m_Graph->MarkImageForExport(clouds.Depth, ImageUsage::Storage | ImageUsage::Sampled | ImageUsage::Source);
            m_Graph->MarkImageForExport(clouds.Reprojection, ImageUsage::Storage | ImageUsage::Sampled | ImageUsage::Source);
            m_Graph->ClaimImage(clouds.ColorPrevious, m_CloudColorAccumulation[0], Device::DeletionQueue());
            m_Graph->ClaimImage(clouds.Color, m_CloudColorAccumulation[1], Device::DeletionQueue());
            m_Graph->ClaimImage(clouds.DepthPrevious, m_CloudDepthAccumulation[0], Device::DeletionQueue());
            m_Graph->ClaimImage(clouds.Depth, m_CloudDepthAccumulation[1], Device::DeletionQueue());
            m_Graph->ClaimImage(clouds.ReprojectionPrevious, m_CloudReprojectionFactor[0], Device::DeletionQueue());
            m_Graph->ClaimImage(clouds.Reprojection, m_CloudReprojectionFactor[1], Device::DeletionQueue());
        }
    }

    std::swap(m_CloudsAccumulationIndex, m_CloudsAccumulationIndexPrev);
    
    m_Graph->Compile(GetFrameContext());
}

void Renderer::UpdateGlobalRenderGraphResources() const
{
    using namespace RG;
    
    auto& blackboard = m_Graph->GetBlackboard();

    SwapchainDescription& swapchain = Device::GetSwapchainDescription(m_Swapchain);

    if (!blackboard.TryGet<GlobalResources>())
    {
        ViewInfoGPU primaryView = {};
        primaryView.Atmosphere = AtmosphereSettings::EarthDefault();
        blackboard.Update<GlobalResources>({.PrimaryViewInfo = primaryView});
    }

    GlobalResources& globalResources = blackboard.Get<GlobalResources>();
    ViewInfoGPU& primaryView = globalResources.PrimaryViewInfo;

    if (m_FrameNumber > 0)
        primaryView.PreviousCamera = primaryView.Camera;
    primaryView.Camera = CameraGPU::FromCamera(*m_Camera, swapchain.SwapchainResolution,
        VisibilityFlags::IsPrimaryView | VisibilityFlags::OcclusionCull);
    
    primaryView.ShadingSettings.TransmittanceLut = m_TransmittanceLutBindlessIndex;
    primaryView.ShadingSettings.SkyViewLut = m_SkyViewLutBindlessIndex;
    ImGui::Begin("Shading Settings");
    ImGui::DragFloat("Environment power", &primaryView.ShadingSettings.EnvironmentPower, 1e-2f, 0.0f, 1.0f);
    ImGui::Checkbox("Soft shadows", (bool*)&primaryView.ShadingSettings.SoftShadows);
    ImGui::End();

    ImGui::Begin("Atmosphere settings");
    ImGui::DragFloat3("Rayleigh scattering", &primaryView.Atmosphere.RayleighScattering[0], 1e-2f, 0.0f, 100.0f);
    ImGui::DragFloat3("Rayleigh absorption", &primaryView.Atmosphere.RayleighAbsorption[0], 1e-2f, 0.0f, 100.0f);
    ImGui::DragFloat3("Mie scattering", &primaryView.Atmosphere.MieScattering[0], 1e-2f, 0.0f, 100.0f);
    ImGui::DragFloat3("Mie absorption", &primaryView.Atmosphere.MieAbsorption[0], 1e-2f, 0.0f, 100.0f);
    ImGui::DragFloat3("Ozone absorption", &primaryView.Atmosphere.OzoneAbsorption[0], 1e-2f, 0.0f, 100.0f);
    ImGui::ColorEdit3("Surface albedo", &primaryView.Atmosphere.SurfaceAlbedo[0]);
    ImGui::DragFloat("Surface", &primaryView.Atmosphere.Surface, 1e-2f, 0.0f, 100.0f);
    ImGui::DragFloat("Atmosphere", &primaryView.Atmosphere.Atmosphere, 1e-2f, 0.0f, 100.0f);
    ImGui::DragFloat("Rayleigh density", &primaryView.Atmosphere.RayleighDensity, 1e-2f, 0.0f, 100.0f);
    ImGui::DragFloat("Mie density", &primaryView.Atmosphere.MieDensity, 1e-2f, 0.0f, 100.0f);
    ImGui::DragFloat("Ozone density", &primaryView.Atmosphere.OzoneDensity, 1e-2f, 0.0f, 100.0f);
    ImGui::End();

    primaryView.FrameNumber = (f32)GetFrameContext().FrameNumberTick;
    primaryView.FrameNumberU32 = (u32)GetFrameContext().FrameNumberTick;

    globalResources.FrameNumberTick = GetFrameContext().FrameNumberTick;
    globalResources.Resolution = GetFrameContext().Resolution;
    globalResources.PrimaryCamera = m_Camera.get();
    globalResources.PrimaryViewInfoResource = Passes::Upload::addToGraph(
        "Upload.GlobalGraphData"_hsv, *m_Graph, globalResources.PrimaryViewInfo);
}

RG::CsmData Renderer::RenderGraphShadows(const ScenePass& scenePass,
    const CommonLight& directionalLight)
{
    using namespace RG;

    f32 shadowMin = 0.01f;
    f32 shadowMax = 100.0f;
    if (m_MinMaxDepthReductions[GetFrameContext().FrameNumber].HasValue())
    {
        auto& readBackDepthMinMax = Passes::DepthReductionReadback::addToGraph("DepthReductionReadback"_hsv,
            *m_Graph, {
                .MinMaxDepthReduction = m_Graph->Import("DepthReduction"_hsv,
                    m_MinMaxDepthReductions[GetFrameContext().FrameNumber]),
                .PrimaryCamera = m_Camera.get()
            });
        shadowMin = readBackDepthMinMax.Min;
        shadowMax = readBackDepthMinMax.Max;
    }
    
    
    auto& csmInit = Passes::SceneCsm::addToGraph("CSM"_hsv,
        *m_Graph, {
            .Pass = &scenePass,
            .Geometry = &m_Scene.Geometry(),
            .MultiviewVisibility = &m_ShadowMultiviewVisibility,
            .MainCamera = m_Camera.get(),
            .DirectionalLight = DirectionalLight{
                .Direction = directionalLight.PositionDirection,
                .Color = directionalLight.Color,
                .Intensity = directionalLight.Intensity,
                .Size = directionalLight.Radius},
            .ShadowMin = shadowMin,
            .ShadowMax = shadowMax,
            .StabilizeCascades = false
        });

    m_ShadowMultiviewVisibilityResources = SceneVisibilityPassesResources::FromSceneMultiviewVisibility(
        *m_Graph, m_ShadowMultiviewVisibility);
    
    auto& meta = Passes::SceneMetaDraw::addToGraph("MetaCsmPass"_hsv,
        *m_Graph, {
            .MultiviewVisibility = &m_ShadowMultiviewVisibility,
            .Resources = &m_ShadowMultiviewVisibilityResources,
            .DrawPasses = csmInit.MetaPassDescriptions
        });

    Passes::SceneCsm::mergeCsm(*m_Graph, csmInit, scenePass, meta.DrawPassViewAttachments);

    return csmInit.CsmData;
}

void Renderer::RenderGraphDepthPrepass(RG::Resource depth, const ScenePass& scenePass)
{
    using namespace RG;
    
    Passes::SceneMetaDraw::addToGraph("MetaDepthPrepass"_hsv,
        *m_Graph, {
            .MultiviewVisibility = &m_PrimaryVisibility,
            .Resources = &m_PrimaryVisibilityResources,
            .DrawPasses = {RenderGraphDepthPrepassDescription(depth, scenePass)}
        });
}

SceneDrawPassDescription Renderer::RenderGraphDepthPrepassDescription(RG::Resource depth, const ScenePass& scenePass)
{
    using namespace RG;
    
    auto initDepthPrepass = [&](StringId name, Graph& graph, const SceneDrawPassExecutionInfo& info)
    {
        auto& pass = Passes::SceneDepthPrepass::addToGraph(
            name.Concatenate(".DepthPrepass"), graph, {
                .DrawInfo = info,
                .Geometry = &m_Scene.Geometry()});

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
        .SceneView = m_OpaqueSetPrimaryView,
        .Visibility = m_OpaqueSetPrimaryVisibility,
        .Attachments = attachments
    };
}

SceneDrawPassDescription Renderer::RenderGraphForwardPbrDescription(RG::Resource color, RG::Resource depth,
    RG::CsmData csmData, const ScenePass& scenePass)
{
    using namespace RG;

    auto initForwardPbr = [&](StringId name, Graph& graph, const SceneDrawPassExecutionInfo& info)
    {
        const bool renderAtmosphere = CVars::Get().GetI32CVar("Renderer.Atmosphere"_hsv).value_or(false);
        
        Passes::SceneForwardPbr::ExecutionInfo executionInfo = {
            .DrawInfo = info,
            .Geometry = &m_Scene.Geometry(),
            .Light = &m_Scene.Lights(),
            .SSAO = {.SSAO = m_Ssao},
            .IBL = {
                .IrradianceSH = renderAtmosphere ? m_SkyIrradianceSHResource :
                    m_Graph->Import("IrradianceSH"_hsv, m_IrradianceSH),
                .PrefilterEnvironment = renderAtmosphere ? m_SkyPrefilterMapResource :
                    m_Graph->Import("PrefilterMap"_hsv, m_SkyboxPrefilterMap, ImageLayout::Readonly),
                .BRDF = m_Graph->Import("BRDF"_hsv, m_BRDFLut,
                    ImageLayout::Readonly)
            },
            .Clusters = m_ClusterLightsInfo.Clusters,
            .Tiles = m_TileLightsInfo.Tiles,
            .ZBins = m_TileLightsInfo.ZBins,
            .CsmData = csmData
        };
        if (CVars::Get().GetI32CVar("Renderer.DepthPrepass"_hsv).value_or(false))
            executionInfo.CommonOverrides = ShaderPipelineOverrides({.DepthTest = DepthTest::Equal});
        
        auto& pass = Passes::SceneForwardPbr::addToGraph(name.Concatenate(".UGB"), graph, executionInfo);

        return pass.Resources.Attachments;
    };

    AttachmentLoad depthOnLoad = AttachmentLoad::Clear;
    AttachmentStore depthOnStore = AttachmentStore::Store;

    if (CVars::Get().GetI32CVar("Renderer.DepthPrepass"_hsv).value_or(false))
        depthOnLoad = AttachmentLoad::Load;
    
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
        .SceneView = m_OpaqueSetPrimaryView,
        .Visibility = m_OpaqueSetPrimaryVisibility,
        .Attachments = attachments
    };
}

SceneDrawPassDescription Renderer::RenderGraphVBufferDescription(RG::Resource vbuffer, RG::Resource depth,
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
        .SceneView = m_OpaqueSetPrimaryView,
        .Visibility = m_OpaqueSetPrimaryVisibility,
        .Attachments = attachments
    };
}

RG::Resource Renderer::RenderGraphVBufferPbr(RG::Resource vbuffer, RG::Resource viewInfo, RG::CsmData csmData)
{
    const bool renderAtmosphere = CVars::Get().GetI32CVar("Renderer.Atmosphere"_hsv).value_or(false);
    
    auto& pbr = Passes::SceneVBufferPbr::addToGraph("VBufferPbr"_hsv, *m_Graph, {
        .Geometry = &m_Scene.Geometry(),
        .VisibilityTexture = vbuffer,
        .ViewInfo = viewInfo,
        .Light = &m_Scene.Lights(),
        .SSAO = {.SSAO = m_Ssao},
        .IBL = {
            .IrradianceSH = renderAtmosphere ? m_SkyIrradianceSHResource :
                m_Graph->Import("IrradianceSH"_hsv, m_IrradianceSH),
            .PrefilterEnvironment = renderAtmosphere ? m_SkyPrefilterMapResource :
                m_Graph->Import("PrefilterMap"_hsv, m_SkyboxPrefilterMap, ImageLayout::Readonly),
            .BRDF = m_Graph->Import("BRDF"_hsv, m_BRDFLut, ImageLayout::Readonly)
        },
        .Clusters = m_ClusterLightsInfo.Clusters,
        .Tiles = m_TileLightsInfo.Tiles,
        .ZBins = m_TileLightsInfo.ZBins,
        .CsmData = csmData,
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
        .Tiles = binLightsTiles.Tiles,
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
        .ViewInfo = m_Graph->GetGlobalResources().PrimaryViewInfoResource,
        .SkyboxTexture = m_SkyboxTexture,
        .Color = color,
        .Depth = depth,
        .Resolution = GetFrameContext().Resolution});

    return skybox.Color;
}

Passes::Atmosphere::LutPasses::PassData& Renderer::RenderGraphAtmosphereLutPasses()
{
    auto& luts = Passes::Atmosphere::LutPasses::addToGraph("AtmosphereLutPasses"_hsv, *m_Graph, {
        .ViewInfo = m_Graph->GetGlobalResources().PrimaryViewInfoResource,
        .Light = &m_Scene.Lights() 
    });
    
    Passes::ImGuiTexture::addToGraph("Atmosphere.Transmittance.Lut"_hsv, *m_Graph, luts.TransmittanceLut);
    Passes::ImGuiTexture::addToGraph("Atmosphere.Multiscattering.Lut"_hsv, *m_Graph, luts.MultiscatteringLut);
    Passes::ImGuiTexture::addToGraph("Atmosphere.SkyView.Lut"_hsv, *m_Graph, luts.SkyViewLut);

    return luts;
}

void Renderer::RenderGraphAtmosphereEnvironment(Passes::Atmosphere::LutPasses::PassData& lut)
{
    auto& environment = Passes::Atmosphere::Environment::addToGraph("Atmosphere.Environment"_hsv, *m_Graph, {
        .PrimaryView = &m_Graph->GetGlobalResources().PrimaryViewInfo,
        .Light = &m_Scene.Lights(),
        .SkyViewLut = lut.SkyViewLut
    });

    m_SkyIrradianceSHResource = Passes::DiffuseIrradianceSH::addToGraph("Sky.DiffuseIrradianceSH"_hsv, *m_Graph,
        environment.ColorOut, m_SkyIrradianceSH, true).DiffuseIrradiance;

    m_SkyPrefilterMapResource = Passes::EnvironmentPrefilter::addToGraph(
        "Scene.EnvironmentPrefilter"_hsv, *m_Graph, environment.ColorOut, m_SkyPrefilterMap, true).PrefilteredTexture;

    Passes::ImGuiCubeTexture::addToGraph("Atmosphere.Environment.Lut"_hsv, *m_Graph, environment.ColorOut);
}

RG::Resource Renderer::RenderGraphAtmosphere(Passes::Atmosphere::LutPasses::PassData& lut,
    RG::Resource aerialPerspective, RG::Resource color, RG::Resource depth, RG::CsmData csmData,
    RG::Resource clouds, RG::Resource cloudsDepth)
{
    static constexpr bool USE_SUN_LUMINANCE = true;
    auto& atmosphere = Passes::Atmosphere::Raymarch::addToGraph("AtmosphereRaymarch"_hsv, *m_Graph, {
        .ViewInfo = m_Graph->GetGlobalResources().PrimaryViewInfoResource,
        .Light = &m_Scene.Lights(),
        .SkyViewLut = lut.SkyViewLut,
        .TransmittanceLut = lut.TransmittanceLut,
        .AerialPerspective = aerialPerspective,
        .Clouds = clouds,
        .CloudsDepth = cloudsDepth,
        .ColorIn = color,
        .DepthIn = depth,
        .UseSunLuminance = USE_SUN_LUMINANCE 
    });
    
    Passes::ImGuiTexture::addToGraph("Atmosphere.Atmosphere"_hsv, *m_Graph, atmosphere.ColorOut);
    Passes::ImGuiTexture3d::addToGraph("Atmosphere.AerialPerspective"_hsv, *m_Graph,
        aerialPerspective);

    return atmosphere.ColorOut;
}

Renderer::CloudMapsInfo Renderer::RenderGraphGetCloudMaps()
{
    using namespace RG;

    static bool isInitialized = false;
    if (!isInitialized)
    {
        m_CloudShapeLowFrequencyNoiseParameters.PerlinCoverageMin = 0.56f;
        m_CloudShapeLowFrequencyNoiseParameters.PerlinCoverageMax = 0.836f;
        m_CloudShapeLowFrequencyNoiseParameters.WorleyCoverageMin = 0.25f;
        m_CloudShapeLowFrequencyNoiseParameters.WorleyCoverageMax = 0.52f;
        m_CloudShapeLowFrequencyNoiseParameters.PerlinWorleyFraction = 0.75f;
        isInitialized = true;
    }
    
    ImGui::Begin("Cloud Maps");

    auto imguiNoiseParametersControls = [](const std::string& name,
        Passes::Clouds::CloudsNoiseParameters& parameters) -> bool {
        bool isDirty = false;
        ImGui::Begin(name.c_str());
        isDirty |= ImGui::DragFloat("Perlin coverage min", &parameters.PerlinCoverageMin, 1e-3f,
            0.0f, parameters.PerlinCoverageMax);
        isDirty |= ImGui::DragFloat("Perlin coverage max", &parameters.PerlinCoverageMax, 1e-3f,
            parameters.PerlinCoverageMin, 2.0f);
        isDirty |= ImGui::DragFloat("Worley coverage min", &parameters.WorleyCoverageMin, 1e-3f,
            0.0f, parameters.WorleyCoverageMax);
        isDirty |= ImGui::DragFloat("Worley coverage max", &parameters.WorleyCoverageMax, 1e-3f,
            parameters.WorleyCoverageMin, 2.0f);
        isDirty |= ImGui::DragFloat("Perlin-Worley fraction", &parameters.PerlinWorleyFraction, 1e-3f,
            0.0f, 1.0f);
        isDirty |= ImGui::DragFloat("Coverage bias", &parameters.NoiseDensityBias, 1e-3f,
            0.0f, 1.0f);
        ImGui::End();

        return isDirty;
    };
    
    ImGui::Begin("CloudCoverage");
    bool isCloudCoverageDirty = imguiNoiseParametersControls("Cloud coverage noise", m_CloudCoverageNoiseParameters);
    if (ImGui::Button("Regenerate"))
        isCloudCoverageDirty = true;
    ImGui::End();

    Resource cloudCoverageResource = {};
    Resource cloudProfileMapResource = {};

    const bool loadCoverage = (bool)CVars::Get().GetI32CVar("Clouds.LoadCoverage"_hsv, bool(1));
    const bool loadProfile = (bool)CVars::Get().GetI32CVar("Clouds.LoadProfile"_hsv, bool(1));
    if (loadCoverage)
    {
        if (!m_CloudCoverage.HasValue())
            m_CloudCoverage = Device::CreateImage({
               .DataSource = "../assets/textures/clouds/coverage.tx",
               .Description = {.Usage = ImageUsage::Sampled},
               .CalculateMipmaps = false
           });
       cloudCoverageResource = m_Graph->Import("CloudCoverage.Loaded"_hsv, m_CloudCoverage, ImageLayout::Readonly);
    }
    else
    {
        if (!isCloudCoverageDirty && m_CloudCoverage.HasValue())
        {
            cloudCoverageResource = m_Graph->Import("CloudCoverage.Import"_hsv, m_CloudCoverage, ImageLayout::Readonly);
        }
        else
        {
            auto& cloudCoverage = Passes::Clouds::VP::Coverage::addToGraph("CoverageMapGen"_hsv, *m_Graph, {
                .CoverageMap = m_CloudCoverage,
                .NoiseParameters = &m_CloudCoverageNoiseParameters,
            });
            cloudCoverageResource = cloudCoverage.CoverageMap;
            if (!m_CloudCoverage.HasValue())
                m_Graph->MarkImageForExport(cloudCoverageResource);
        }
    }
    if (loadProfile)
    {
        if (!m_CloudProfileMap.HasValue())
            m_CloudProfileMap = Device::CreateImage({
                .DataSource = "../assets/textures/clouds/profile.tx",
                .Description = {.Usage = ImageUsage::Sampled},
                .CalculateMipmaps = false
            });
        cloudProfileMapResource = m_Graph->Import("CloudProfileMap.Loaded"_hsv,
            m_CloudProfileMap, ImageLayout::Readonly);
    }
    else
    {
        if (!isCloudCoverageDirty && m_CloudProfileMap.HasValue())
        {
            cloudProfileMapResource = m_Graph->Import("CloudProfileMap.Import"_hsv, 
                m_CloudProfileMap, ImageLayout::Readonly);
        }
        else
        {
            auto& cloudProfileMap = Passes::Clouds::VP::ProfileMap::addToGraph("ProfileMapGen"_hsv, *m_Graph, {
                .ProfileMap = m_CloudProfileMap,
            });
            cloudProfileMapResource = cloudProfileMap.ProfileMap;
            if (!m_CloudProfileMap.HasValue())
                m_Graph->MarkImageForExport(cloudProfileMapResource);
        }    
    }
    
    Passes::ImGuiTexture::addToGraph("CloudCoverage.Tex"_hsv, *m_Graph, cloudCoverageResource);
    Passes::ImGuiTexture::addToGraph("CloudProfileMap.Tex"_hsv, *m_Graph, cloudProfileMapResource);

    bool isShapeDirty = false;
    isShapeDirty |= imguiNoiseParametersControls("Low frequency shape noise", m_CloudShapeLowFrequencyNoiseParameters);
    isShapeDirty |= imguiNoiseParametersControls("High frequency shape noise",
        m_CloudShapeHighFrequencyNoiseParameters);
    if (ImGui::Button("Regenerate"))
        isShapeDirty = true;
    
    Resource lowFrequencyNoiseResource = {};
    Resource highFrequencyNoiseResource = {};
    if (!isShapeDirty && m_CloudShapeLowFrequency.HasValue() && m_CloudShapeHighFrequency.HasValue())
    {
        lowFrequencyNoiseResource = m_Graph->Import("LowFrequency.Import"_hsv, m_CloudShapeLowFrequency,
            ImageLayout::Readonly);
        highFrequencyNoiseResource = m_Graph->Import("HighFrequency.Import"_hsv, m_CloudShapeHighFrequency,
            ImageLayout::Readonly);
    }
    else
    {
        auto& cloudShape = Passes::CloudShapeNoise::addToGraph("CloudShapeNoise"_hsv, *m_Graph, {
            .LowFrequencyTextureSize = 128.0f,
            .HighFrequencyTextureSize = 32.0f,
            .LowFrequencyTexture = m_CloudShapeLowFrequency,
            .HighFrequencyTexture = m_CloudShapeHighFrequency,
            .LowFrequencyNoiseParameters = &m_CloudShapeLowFrequencyNoiseParameters,
            .HighFrequencyNoiseParameters = &m_CloudShapeHighFrequencyNoiseParameters
        });
        lowFrequencyNoiseResource = cloudShape.LowFrequencyTexture;
        highFrequencyNoiseResource = cloudShape.HighFrequencyTexture;

        if (!m_CloudShapeLowFrequency.HasValue())
            m_Graph->MarkImageForExport(cloudShape.LowFrequencyTexture);
        if (!m_CloudShapeHighFrequency.HasValue())
            m_Graph->MarkImageForExport(cloudShape.HighFrequencyTexture);
    }

    Resource curlNoiseResource = {};
    if (m_CloudCurlNoise.HasValue())
    {
        curlNoiseResource = m_Graph->Import("CloudsCurlNoise.Import"_hsv, m_CloudCurlNoise,
            ImageLayout::Readonly);
    }
    else
    {
        auto& curlNoise = Passes::CloudCurlNoise::addToGraph("CloudsCurlNoise"_hsv, *m_Graph, {
            .CloudCurlNoise = m_CloudCurlNoise
        });
        curlNoiseResource = curlNoise.CloudCurlNoise;
        m_Graph->MarkImageForExport(curlNoiseResource);
    }
    
    Passes::ImGuiTexture3d::addToGraph("LowFreq.Tex"_hsv, *m_Graph, lowFrequencyNoiseResource);
    Passes::ImGuiTexture3d::addToGraph("HighFreq.Tex"_hsv, *m_Graph, highFrequencyNoiseResource);
    Passes::ImGuiTexture::addToGraph("CurlNoise.Tex"_hsv, *m_Graph, curlNoiseResource);
    
    ImGui::End();
    
    return {
        .Coverage = cloudCoverageResource,
        .Profile = cloudProfileMapResource,
        .ShapeLowFrequency = lowFrequencyNoiseResource,
        .ShapeHighFrequency = highFrequencyNoiseResource,
        .CurlNoise = curlNoiseResource,
    };
}

Renderer::CloudsInfo Renderer::RenderGraphClouds(const CloudMapsInfo& cloudMaps, RG::Resource color,
    RG::Resource aerialPerspective, RG::Resource depth)
{
    ImGui::Begin("Clouds Parameters");
    ImGui::DragFloat("Meters per texel", &m_CloudParameters.CloudMapMetersPerTexel, 1e-2f, 0.0f);
    f32 shapeNoiseScale = 1.0f / m_CloudParameters.ShapeNoiseScale;
    ImGui::DragFloat("Shape noise scale", &shapeNoiseScale, 1e-1f, 0.0f);
    m_CloudParameters.ShapeNoiseScale = 1.0f / shapeNoiseScale;
    ImGui::DragFloat("Detail noise scale multiplier", &m_CloudParameters.DetailNoiseScaleMultiplier, 1e-2f, 0.0f);
    f32 windAngle = glm::degrees(m_CloudParameters.WindAngle);
    ImGui::DragFloat("Detail noise contribution", &m_CloudParameters.DetailNoiseContribution, 1e-2f, 0.0f);
    ImGui::DragFloat("Detail noise height modifier", &m_CloudParameters.DetailNoiseHeightModifier, 1e-1f, 0.0f);
    ImGui::DragFloat("Wind angle", &windAngle, 1e-1f);
    m_CloudParameters.WindAngle = glm::radians(windAngle);
    ImGui::DragFloat("Wind speed", &m_CloudParameters.WindSpeed, 1e-2f);
    ImGui::DragFloat("Wind upright amount", &m_CloudParameters.WindUprightAmount, 1e-2f);
    ImGui::DragFloat("Wind horizontal skew", &m_CloudParameters.WindHorizontalSkew, 1e+1f);
    ImGui::DragFloat4("Anvil stratus", &m_CloudParameters.AnvilStratus[0], 1e-2f);
    ImGui::DragFloat4("Anvil stratocumulus", &m_CloudParameters.AnvilStratocumulus[0], 1e-2f);
    ImGui::DragFloat4("Anvil cumulus", &m_CloudParameters.AnvilCumulus[0], 1e-2f);
    ImGui::DragFloat("Curl noise scale multiplier", &m_CloudParameters.CurlNoiseScaleMultiplier, 1e-3f);
    ImGui::DragFloat("Curl noise height", &m_CloudParameters.CurlNoiseHeight, 1e-3f);
    ImGui::DragFloat("Curl noise contribution", &m_CloudParameters.CurlNoiseContribution, 1e-3f);

    ImGui::DragFloat("HG eccentricity", &m_CloudParameters.HGEccentricity, 1e-2f, 0.0f, 1.0f);
    ImGui::DragFloat("HG backward eccentricity", &m_CloudParameters.HGBackwardEccentricity, 1e-2f, -1.0f, 0.0f);
    
    ImGui::Checkbox("Reprojection", &m_CloudsReprojectionEnabled);
    
    ImGui::End();

    m_CloudParameters.BlueNoiseBindlessIndex = m_BlueNoiseBindlessIndex;

    const bool renderAtmosphere = CVars::Get().GetI32CVar("Renderer.Atmosphere"_hsv).value_or(false);
    
    auto& clouds = Passes::Clouds::VP::addToGraph("Clouds"_hsv, *m_Graph, {
        .ViewInfo = m_Graph->GetGlobalResources().PrimaryViewInfoResource,
        .CloudCoverage = cloudMaps.Coverage,
        .CloudProfile = cloudMaps.Profile,
        .CloudShapeLowFrequencyMap = cloudMaps.ShapeLowFrequency,
        .CloudShapeHighFrequencyMap = cloudMaps.ShapeHighFrequency, 
        .CloudCurlNoise = cloudMaps.CurlNoise, 
        .DepthIn = depth,
        .AerialPerspectiveLut = aerialPerspective,
        .IrradianceSH = renderAtmosphere ? m_SkyIrradianceSHResource :
            m_Graph->Import("IrradianceSH"_hsv, m_IrradianceSH),
        .Light = &m_Scene.Lights(),
        .CloudParameters = &m_CloudParameters,
        .CloudsRenderingMode = m_CloudsReprojectionEnabled ?
            Passes::Clouds::VP::CloudsRenderingMode::Reprojection :
            Passes::Clouds::VP::CloudsRenderingMode::FullResolution,
    });

    Passes::ImGuiTexture::addToGraph("Clouds.Color"_hsv, *m_Graph, clouds.ColorOut);
    Passes::ImGuiTexture::addToGraph("Clouds.Depth"_hsv, *m_Graph, clouds.DepthOut);

    if (m_CloudsReprojectionEnabled)
    {
        auto& reprojection = Passes::CloudReproject::addToGraph("Clouds.Reproject"_hsv, *m_Graph, {
            .ViewInfo = m_Graph->GetGlobalResources().PrimaryViewInfoResource,
            .Color = clouds.ColorOut,
            .Depth = clouds.DepthOut,
            .ColorAccumulationIn = m_FrameNumber > 1 ?
                m_Graph->Import("Clouds.Color.Accumulation.In"_hsv,
                    m_CloudColorAccumulation[m_CloudsAccumulationIndexPrev], ImageLayout::Readonly) :
                RG::Resource{},
            .DepthAccumulationIn = m_FrameNumber > 1 ?
                m_Graph->Import("Clouds.Depth.Accumulation.In"_hsv,
                    m_CloudDepthAccumulation[m_CloudsAccumulationIndexPrev], ImageLayout::Source) :
                RG::Resource{},
            .ReprojectionFactorIn = m_FrameNumber > 1 ?
                m_Graph->Import("Clouds.ReprojectionFactor.In"_hsv,
                    m_CloudReprojectionFactor[m_CloudsAccumulationIndexPrev], ImageLayout::Source) :
                RG::Resource{},
            .ColorAccumulationOut = m_FrameNumber > 1 ?
                m_Graph->Import("Clouds.Color.Accumulation"_hsv,
                    m_CloudColorAccumulation[m_CloudsAccumulationIndex]) :
                RG::Resource{},
            .DepthAccumulationOut = m_FrameNumber > 1 ?
                m_Graph->Import("Clouds.Depth.Accumulation"_hsv,
                    m_CloudDepthAccumulation[m_CloudsAccumulationIndex]) :
                RG::Resource{},
            .ReprojectionFactorOut = m_FrameNumber > 1 ?
                m_Graph->Import("Clouds.ReprojectionFactor"_hsv,
                    m_CloudReprojectionFactor[m_CloudsAccumulationIndex]) :
                RG::Resource{},
            .CloudParameters = &m_CloudParameters,
        });

        Passes::ImGuiTexture::addToGraph("Clouds.Reprojection.Color"_hsv, *m_Graph, reprojection.ColorAccumulationOut);
        Passes::ImGuiTexture::addToGraph("Clouds.Reprojection.Depth"_hsv, *m_Graph, reprojection.DepthAccumulationOut);
        Passes::ImGuiTexture::addToGraph("Clouds.Reprojection.Factor"_hsv, *m_Graph, reprojection.ReprojectionFactorOut);

        return {
            .ColorPrevious = reprojection.ColorAccumulationIn,
            .DepthPrevious = reprojection.DepthAccumulationIn,
            .ReprojectionPrevious = reprojection.ReprojectionFactorIn,
            .Color = reprojection.ColorAccumulationOut,
            .Depth = reprojection.DepthAccumulationOut,
            .Reprojection = reprojection.ReprojectionFactorOut
        };
    }
    
    return {
        .Color = clouds.ColorOut,
        .Depth = clouds.DepthOut
    };
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
        BeginFrame();


        OnUpdate();
        
        OnRender();


        if (!m_FrameEarlyExit)
        {
            EndFrame();
            FrameMark; // tracy
        }
        else
        {
            m_FrameEarlyExit = false;
        }
    }
}

void Renderer::OnUpdate()
{
    CPU_PROFILE_FRAME("On update")

    m_CameraController->OnUpdate(1.0f / 60.0f);

    LightFrustumCuller::CullDepthSort(m_Scene.Lights(), *GetFrameContext().PrimaryCamera);
    m_Scene.Hierarchy().OnUpdate(m_Scene, GetFrameContext());
    m_Scene.Lights().OnUpdate(GetFrameContext());
    m_OpaqueSet.OnUpdate(GetFrameContext());

    m_ShadowMultiviewVisibility.OnUpdate(GetFrameContext());
    m_PrimaryVisibility.OnUpdate(GetFrameContext());

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
}

void Renderer::OnRender()
{
    CPU_PROFILE_FRAME("On render")

    {
        // todo: is this even necessary?
        CPU_PROFILE_FRAME("Initial submit")
        GPU_PROFILE_FRAME("Initial submit")
        GetFrameContext().ResourceUploader->SubmitUpload(GetFrameContext());
        GetFrameContext().CommandList.WaitOnBarrier({.DependencyInfo = Device::CreateDependencyInfo({
            .MemoryDependencyInfo = MemoryDependencyInfo{
                .SourceStage = PipelineStage::AllTransfer,
                .DestinationStage = PipelineStage::AllCommands,
                .SourceAccess = PipelineAccess::WriteAll,
                .DestinationAccess = PipelineAccess::ReadAll}},
            GetFrameContext().DeletionQueue)});
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
                m_MermaidExporter->ExportToHtml("../assets/render graph/graph.html");
            twice--;
        }
        else
        {
            m_Graph->RemoveWatcher();
        }
    }

    if (!m_FrameEarlyExit)
    {
        m_Graph->Execute(GetFrameContext());
    }
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

    GPU_COLLECT_PROFILE_FRAMES()
    
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
    
    m_ShadowMultiviewVisibility.Shutdown();
    m_PrimaryVisibility.Shutdown();
    
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
    m_Graph->SetBackbufferImage(swapchain.DrawImage);

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

u32 Renderer::GetPreviousFrameNumber() const
{
    return (m_FrameNumber + BUFFERED_FRAMES - 1) % BUFFERED_FRAMES;
}
