#include "rendererpch.h"

#include "Renderer.h"

#include <tracy/Tracy.hpp>

#include "ViewInfoGPU.h"
#include "Assets/AssetSystem.h"
#include "Assets/Images/ImageAssetManager.h"
#include "Assets/Materials/MaterialAsset.h"
#include "Assets/Materials/MaterialAssetManager.h"
#include "Assets/Scenes/SceneAssetManager.h"
#include "Assets/Shaders/ShaderAssetManager.h"
#include "Core/Input.h"
#include "Core/InputEvents/InputEventDispatcher.h"
#include "Core/InputEvents/KeyboardInputEvent.h"
#include "Core/InputEvents/MouseInputEvent.h"
#include "Core/InputEvents/WindowInputEvent.h"
#include "Core/Window/Window.h"
#include "cvars/CVarSystem.h"

#include "Imgui/ImguiUI.h"
#include "Light/LightFrustumCuller.h"
#include "Light/LightZBinner.h"
#include "Light/SH.h"
#include "RenderGraph/RGCommon.h"
#include "RenderGraph/Passes/AA/FxaaPass.h"
#include "RenderGraph/Passes/AO/SsaoBlurPass.h"
#include "RenderGraph/Passes/AO/SsaoPass.h"
#include "RenderGraph/Passes/Atmosphere/AtmosphereAerialPerspectiveLutPass.h"
#include "RenderGraph/Passes/Atmosphere/AtmospherePass.h"
#include "RenderGraph/Passes/Atmosphere/AtmosphereRenderPass.h"
#include "RenderGraph/Passes/Atmosphere/AtmosphereTransmittanceAtViewPass.h"
#include "RenderGraph/Passes/Atmosphere/Environment/AtmosphereEnvironmentPass.h"
#include "RenderGraph/Passes/Clouds/CloudComposePass.h"
#include "RenderGraph/Passes/Clouds/CloudCurlNoisePass.h"
#include "RenderGraph/Passes/Clouds/CloudReprojectPass.h"
#include "RenderGraph/Passes/Clouds/CloudShapeNoisePass.h"
#include "RenderGraph/Passes/Clouds/VerticalProfile/VPCloudPass.h"
#include "RenderGraph/Passes/Clouds/VerticalProfile/VPCloudCoveragePass.h"
#include "RenderGraph/Passes/Clouds/VerticalProfile/VPCloudProfileMapPass.h"
#include "RenderGraph/Passes/Clouds/VerticalProfile/VPCloudEnvironmentPass.h"
#include "RenderGraph/Passes/Clouds/VerticalProfile/VPCloudShadowPass.h"
#include "RenderGraph/Passes/Extra/SlimeMold/SlimeMoldPass.h"
#include "RenderGraph/Passes/SceneDraw/PBR/SceneForwardPbrPass.h"
#include "RenderGraph/Passes/HiZ/HiZVisualize.h"
#include "RenderGraph/Passes/Lights/LightClustersBinPass.h"
#include "RenderGraph/Passes/Lights/LightClustersSetupPass.h"
#include "RenderGraph/Passes/Lights/LightTilesBinPass.h"
#include "RenderGraph/Passes/Lights/LightTilesSetupPass.h"
#include "RenderGraph/Passes/Lights/VisualizeLightClustersPass.h"
#include "RenderGraph/Passes/Lights/VisualizeLightTiles.h"
#include "RenderGraph/Passes/PostProcessing/CRT/CrtPass.h"
#include "RenderGraph/Passes/Scene/SceneGeometryRGResources.h"
#include "RenderGraph/Passes/Scene/Visibility/SceneMultiviewMeshletVisibilityPass.h"
#include "RenderGraph/Passes/Scene/Visibility/SceneMultiviewRenderObjectVisibilityPass.h"
#include "RenderGraph/Passes/Scene/Visibility/SceneMultiviewVisibilityHiZPass.h"
#include "RenderGraph/Passes/SceneDraw/SceneMetaDrawPass.h"
#include "RenderGraph/Passes/SceneDraw/General/SceneDepthPrepassPass.h"
#include "RenderGraph/Passes/SceneDraw/Shadow/SceneCsmPass.h"
#include "RenderGraph/Passes/SceneDraw/Shadow/SceneDirectionalShadowPass.h"
#include "RenderGraph/Passes/SceneDraw/VBuffer/SceneVBufferPass.h"
#include "RenderGraph/Passes/SceneDraw/VBuffer/SceneVBufferPbrPass.h"
#include "RenderGraph/Passes/Shadows/DepthReductionReadbackPass.h"
#include "RenderGraph/Passes/Shadows/ShadowPassesCommon.h"
#include "RenderGraph/Passes/Skybox/SkyboxPass.h"
#include "RenderGraph/Passes/Utility/BlitPass.h"
#include "RenderGraph/Passes/SceneDraw/PBR/BRDFLutPass.h"
#include "RenderGraph/Passes/Utility/CopyTexturePass.h"
#include "RenderGraph/Passes/SceneDraw/PBR/DiffuseIrradianceSHPass.h"
#include "RenderGraph/Passes/SceneDraw/PBR/EnvironmentPrefilterPass.h"
#include "RenderGraph/Passes/SceneDraw/PBR/ExposurePass.h"
#include "RenderGraph/Passes/SceneDraw/PBR/TonemappingPass.h"
#include "RenderGraph/Passes/Shadows/ShadowCamerasGpuPass.h"
#include "RenderGraph/Passes/Skinning/ComputeSkinningPass.h"
#include "RenderGraph/Passes/Utility/CopyBufferPass.h"
#include "RenderGraph/Passes/Utility/CopyToBufferPass.h"
#include "RenderGraph/Passes/Utility/EquirectangularToCubemapPass.h"
#include "RenderGraph/Passes/Utility/ImGuiTexturePass.h"
#include "RenderGraph/Passes/Utility/MipMapPass.h"
#include "RenderGraph/Passes/Utility/UploadPass.h"
#include "Scene/BindlessTextureDescriptorsRingBuffer.h"
#include "Scene/Scene.h"

#include <AssetLib/Images/ImageMeta.h>
#include <AssetLib/Materials/MaterialMeta.h>
#include <AssetLib/Scenes/Scene/SceneMeta.h>
#include <AssetLib/Io/Compression/Lz4AssetCompressor.h>
#include <AssetLib/Io/Compression/RawAssetCompressor.h>
#include <AssetLib/Io/IoInterface/CombinedAssetIoInterface.h>
#include <AssetLib/Io/IoInterface/SeparateAssetIoInterface.h>
#include <CoreLib/Math/Random.h>
#include <CoreLib/String/StringId.h>

Renderer::Renderer() = default;

void Renderer::OnInputEvent(const lux::InputEvent& event)
{
    Input::OnInputEvent(event);
    m_CameraController->OnInputEvent(event);
    lux::InputEventDispatcher dispatcher(event);
    dispatcher.Dispatch<lux::WindowResizedEvent>([&](const lux::WindowResizedEvent&)
    {
        OnWindowResize();
    });
}

void Renderer::Init()
{
    StringIdRegistry::Init();

    if (*CVars::Get().GetI32CVar("Assets.IoType"_hsv) == 0)
        m_AssetIoInterface = std::make_shared<lux::assetlib::io::SeparateAssetIoInterface>();
    else
        m_AssetIoInterface = std::make_shared<lux::assetlib::io::CombinedAssetIoInterface>();
    if (*CVars::Get().GetI32CVar("Assets.IoCompression"_hsv) == 0)
        m_AssetCompressor = std::make_shared<lux::assetlib::io::Lz4AssetCompressor>();
    else
        m_AssetCompressor = std::make_shared<lux::assetlib::io::RawAssetCompressor>();

    m_ImportCtx = std::make_shared<lux::import::Context>(lux::import::Context{
        .InitialDirectory = *CVars::Get().GetStringCVar("Path.Assets"_hsv),
        .BakedDirectory = *CVars::Get().GetStringCVar("Path.AssetsBaked"_hsv),
        .Io = m_AssetIoInterface.get(),
        .Compressor = m_AssetCompressor.get()
    });
    m_ShaderImportSettings = {
        .IncludePaths = {"../assets/shaders/slang/raw"},
        .UniformReflectionDirectoryName = "uniform_types",
        .EnableHotReloading = true,
    };

    m_AssetSystem.Init(m_ImportCtx);
    m_AssetSystem.SetAssetsDirectory(*CVars::Get().GetStringCVar("Path.Assets"_hsv));

    m_ShaderAssetManager = std::make_unique<lux::ShaderAssetManager>(m_AssetSystem);
    m_AssetSystem.RegisterAssetManager(lux::assetlib::shader::ASSET_TYPE, *m_ShaderAssetManager);
    m_ShaderAssetManager->Init(m_ShaderImportSettings);

    m_ImageAssetManager = std::make_unique<lux::ImageAssetManager>(m_AssetSystem);
    m_AssetSystem.RegisterAssetManager(lux::assetlib::image::ASSET_TYPE, *m_ImageAssetManager);

    m_MaterialAssetManager = std::make_unique<lux::MaterialAssetManager>(m_AssetSystem);
    m_AssetSystem.RegisterAssetManager(lux::assetlib::material::ASSET_TYPE, *m_MaterialAssetManager);

    m_SceneAssetManager = std::make_unique<lux::SceneAssetManager>(m_AssetSystem);
    m_AssetSystem.RegisterAssetManager(lux::assetlib::scene::ASSET_TYPE, *m_SceneAssetManager);

    m_AssetSystem.ScanAssetsDirectory();
    
    InitRenderingStructures();
    Device::BeginFrame(GetFrameContext());

    Input::OnWindowResized(
        Device::GetSwapchainDescription(m_Swapchain).SwapchainResolution.x,
        Device::GetSwapchainDescription(m_Swapchain).SwapchainResolution.y
    );
    m_Camera = std::make_shared<Camera>(CameraType::Perspective);
    m_CameraController = std::make_unique<CameraController>(m_Camera);
    for (auto& ctx : m_FrameContexts)
        ctx.PrimaryCamera = m_Camera.get();

    m_PersistentMaterialAllocator = Device::CreateDescriptorArenaAllocator({
        .DescriptorSet = BINDLESS_DESCRIPTORS_INDEX,
        .Residence = DescriptorAllocatorResidence::CPU,
        .UsedTypes = {DescriptorType::UniformBuffer, DescriptorType::StorageBuffer, DescriptorType::Image},
        .DescriptorCount = 8192 * 4});

    std::array<DescriptorArenaAllocators, BUFFERED_FRAMES> allocators;
    for (u32 i = 0; i < BUFFERED_FRAMES; i++)
    {
        DescriptorArenaAllocator samplerAllocator = Device::CreateDescriptorArenaAllocator({
                .DescriptorSet = 0,
                .Residence = DescriptorAllocatorResidence::CPU,
                .UsedTypes = {DescriptorType::Sampler},
                .DescriptorCount = 256 * 4});
            
        DescriptorArenaAllocator resourceAllocator = Device::CreateDescriptorArenaAllocator({
            .DescriptorSet = 1,
            .Residence = DescriptorAllocatorResidence::CPU,
            .UsedTypes = {DescriptorType::UniformBuffer, DescriptorType::StorageBuffer, DescriptorType::Image},
            .DescriptorCount = 8192 * 4});

        allocators[i] = DescriptorArenaAllocators({samplerAllocator, resourceAllocator}, m_PersistentMaterialAllocator);
    }
    
    m_Graph = std::make_unique<RG::Graph>(allocators, *m_ShaderAssetManager);
    m_MermaidExporter = std::make_unique<RG::RGMermaidExporter>();
    InitRenderGraph();
}

void Renderer::InitRenderGraph()
{
    static constexpr u32 TEXTURE_HEAP_SIZE = 1024;
    auto textureHeap = m_ShaderAssetManager->AllocateTextureHeap(
        m_Graph->GetFrameAllocators().GetPersistent(),
        TEXTURE_HEAP_SIZE);
    ASSERT(textureHeap.has_value())
    m_TextureHeap = textureHeap->Descriptors;
    m_TextureHeapPipelineLayout = textureHeap->PipelineLayout;
    
    m_BindlessTextureDescriptorsRingBuffer = std::make_unique<BindlessTextureDescriptorsRingBuffer>(
        TEXTURE_HEAP_SIZE, m_TextureHeap);
    m_SceneAssetManager->SetTextureRingBuffer(*m_BindlessTextureDescriptorsRingBuffer);
    m_TransmittanceLutBindlessIndex = m_BindlessTextureDescriptorsRingBuffer->AddTexture(
        Images::Default::GetCopy(Images::DefaultKind::White, Device::DeletionQueue()));
    m_SkyViewLutBindlessIndex = m_BindlessTextureDescriptorsRingBuffer->AddTexture(
        Images::Default::GetCopy(Images::DefaultKind::White, Device::DeletionQueue()));
    m_VolumetricShadowBindlessIndex = m_BindlessTextureDescriptorsRingBuffer->AddTexture(
        Images::Default::GetCopy(Images::DefaultKind::White, Device::DeletionQueue()));

    // todo: this is currently a rgb image, which is wasteful, I need to provide format hints to image converter
    m_BlueNoiseBindlessIndex = m_BindlessTextureDescriptorsRingBuffer->AddTexture(
        m_ImageAssetManager->Get(m_ImageAssetManager->LoadResource(
            {.Path = "../assets/textures/blue_noise_128.png"})
    ));
    
    /*m_SlimeMoldContext = std::make_shared<SlimeMoldContext>(
        SlimeMoldContext::RandomIn(Device::GetSwapchainDescription(m_Swapchain).SwapchainResolution,
            1, 5000000, *GetFrameContext().ResourceUploader));*/

    //SceneConverter::Convert(
    //    *CVars::Get().GetStringCVar("Path.Assets"_hsv),
    //    *CVars::Get().GetStringCVar("Path.Assets"_hsv) + "models/lights_test/scene.gltf");
    
    m_Scene = std::make_unique<Scene>(Device::DeletionQueue(), *m_SceneAssetManager);
    m_SceneBucketList.Init(*m_Scene);
    m_OpaqueSet.Init("Opaque"_hsv, *m_Scene, m_SceneBucketList, {
        ScenePassCreateInfo{
            .Name = "DepthPrepass"_hsv,
            .BucketCreateInfos = {
                {
                    .Name = "Opaque material"_hsv,
                    .Filter = [this](const lux::SceneGeometryInfo& geometry, SceneRenderObjectHandle renderObject) {
                        const u32 material = geometry.RenderObjects[renderObject.Index].Material;
                        if (material == lux::SceneRenderObject::INVALID)
                            return false;
                        const lux::MaterialAsset* materialAsset = m_MaterialAssetManager->Get(
                            geometry.MaterialsCpu[material].Handle);
                        if (materialAsset == nullptr)
                            return false;
                        
                        return materialAsset->AlphaMode == lux::MaterialAlphaMode::Opaque;
                    }
                }
            }
        },
        ScenePassCreateInfo{
            .Name = "Vbuffer"_hsv,
            .BucketCreateInfos = {
                {
                    .Name = "Opaque material"_hsv,
                    .Filter = [this](const lux::SceneGeometryInfo& geometry, SceneRenderObjectHandle renderObject) {
                        const u32 material = geometry.RenderObjects[renderObject.Index].Material;
                        if (material == lux::SceneRenderObject::INVALID)
                            return false;
                        const lux::MaterialAsset* materialAsset = m_MaterialAssetManager->Get(
                            geometry.MaterialsCpu[material].Handle);
                        if (materialAsset == nullptr)
                            return false;
                        
                        return materialAsset->AlphaMode == lux::MaterialAlphaMode::Opaque;
                    }
                }
            }
        },
        ScenePassCreateInfo{
            .Name = "ForwardPbr"_hsv,
            .BucketCreateInfos = {
                {
                    .Name = "Opaque material"_hsv,
                    .Filter = [](const lux::SceneGeometryInfo& geometry, SceneRenderObjectHandle renderObject) {
                        return renderObject.Index < 1;
                    },
                },
                {
                    .Name = "Opaque material2"_hsv,
                    .Filter = [this](const lux::SceneGeometryInfo& geometry, SceneRenderObjectHandle renderObject) {
                        const u32 material = geometry.RenderObjects[renderObject.Index].Material;
                        if (material == lux::SceneRenderObject::INVALID)
                            return false;
                        const lux::MaterialAsset* materialAsset = m_MaterialAssetManager->Get(
                            geometry.MaterialsCpu[material].Handle);
                        if (materialAsset == nullptr)
                            return false;
                        
                        return materialAsset->AlphaMode == lux::MaterialAlphaMode::Opaque && renderObject.Index > 1;
                    },
                    .ShaderOverrides = ShaderDefines({ShaderDefine("TEST"_hsv)})
                }
            }
        },
        Passes::SceneCsm::getScenePassCreateInfo("Shadow"_hsv, *m_MaterialAssetManager),
    }, Device::DeletionQueue());

    m_ShadowMultiviewVisibility.Init(m_OpaqueSet);
    m_PrimaryVisibility.Init(m_OpaqueSet);
    
    /* initial submit */
    Device::ImmediateSubmit([&](RenderCommandList& cmdList)
    {
        FrameContext ctx = GetFrameContext();
        ctx.CommandList = cmdList;
        m_Scenes.push_back(
            m_SceneAssetManager->LoadResource(
                {.Path = *CVars::Get().GetStringCVar("Path.Assets"_hsv) + "models/hotReloadTest/scene.gltf"}));
        m_Scenes.push_back(
            m_SceneAssetManager->LoadResource(
                {.Path = *CVars::Get().GetStringCVar("Path.Assets"_hsv) + "models/flight_helmet/scene.gltf"}));
        
        lux::SceneInstanceHandle instance = m_Scene->Instantiate(m_Scenes.front(), {
            .Transform = {
                //.Position = glm::vec3{1500.0f, -500.0f, -7.0f},
                .Orientation = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
                //.Scale = glm::vec3{750.0f},
                //.Scale = glm::vec3{1.0f},
                }});

        lux::SceneAsset lights = {};
        lights.SetSunLight({{
            .Direction = glm::normalize(glm::vec3(-0.5f, -0.6f, 0.74f)),
            .Color = glm::vec3(1.0f, 1.0f, 1.0f),
            .Intensity = 120000.0f,
        }});
        constexpr u32 POINT_LIGHT_COUNT = 32;
        for (u32 i = 0; i < POINT_LIGHT_COUNT; i++)
        {
            const auto pos =
                glm::vec3{Random::Float(-5.0f, 5.0f), Random::Float(0.0f, 2.0f), Random::Float(-5.0f, 5.0f)};
            const float rad = Random::Float(0.5f, 8.6f);
            lights.AddLight({{
                //.Position = glm::vec3{Random::Float(-39.0f, 39.0f), Random::Float(0.0f, 4.0f), Random::Float(-19.0f, 19.0f)},
                .Position = pos,
                .Color = Random::Float3(0.0f, 1.0f),
                .Intensity = Random::Float(1500.0f, 3700.0f),
                .Radius = rad
            }});
        }
        m_Lights = m_SceneAssetManager->AddExternalScene(std::move(lights));
        m_Scene->Instantiate(m_Lights, {});

        ctx.ResourceUploader->SubmitUpload(ctx);
    });

    m_Graph->SetWatcher(*m_MermaidExporter);
}

void Renderer::ExecuteSingleTimePasses()
{
    static constexpr std::string_view SKYBOX_PATH = "../assets/textures/forest.hdr";
    const lux::ImageHandle equirectangular = m_ImageAssetManager->LoadResource({.Path = SKYBOX_PATH});

    const TextureDescription& equirectangularDescription =
        Device::GetImageDescription(m_ImageAssetManager->Get(equirectangular));
    m_SkyboxTexture = m_Graph->AddPersistent(Device::CreateImage({
        .Description = ImageDescription{
            .Width = equirectangularDescription.Width / 2,
            .Height = equirectangularDescription.Width / 2,
            .Mipmaps = Images::mipmapCount(glm::uvec2{equirectangularDescription.Width / 2}),
            .Format = Format::RGBA16_FLOAT,
            .Kind = ImageKind::ImageCubemap,
            .Usage = ImageUsage::Sampled | ImageUsage::Storage
        },
        .CalculateMipmaps = false
    }));

    m_MipsTest.Load(*m_Graph, *m_ImageAssetManager, "../assets/textures/texture.png");

    m_SkyboxPrefilterMap = m_Graph->AddPersistent(Device::CreateImage({
        .Description = Passes::EnvironmentPrefilter::getPrefilteredTextureDescription(
            *CVars::Get().GetI32CVar("Renderer.IBL.PrefilterResolution"_hsv)),
        .CalculateMipmaps = false
    }));

    m_SkyPrefilterMap = m_Graph->AddPersistent(Device::CreateImage({
        .Description = Passes::EnvironmentPrefilter::getPrefilteredTextureDescription(
            *CVars::Get().GetI32CVar("Renderer.IBL.PrefilterResolutionRealtime"_hsv)),
        .CalculateMipmaps = false
    }));

    m_IrradianceSH = m_Graph->AddPersistent(Device::CreateBuffer({
        .Description = {
            .SizeBytes = sizeof(SH2Irradiance),
            .Usage = BufferUsage::Ordinary | BufferUsage::Storage | BufferUsage::Uniform,
        }
    }));
    m_SkyIrradianceSH = m_Graph->AddPersistent(Device::CreateBuffer({
        .Description = {
            .SizeBytes = sizeof(SH2Irradiance),
            .Usage = BufferUsage::Ordinary | BufferUsage::Storage | BufferUsage::Uniform
        }
    }));

    m_BRDFLut = m_Graph->AddPersistent(Device::CreateImage({
        .Description = Passes::BRDFLut::getLutDescription(),
        .CalculateMipmaps = false
    }));
    
    m_Graph->Reset();

    const RG::ImageResource cubemap = Passes::EquirectangularToCubemap::addToGraph("Scene.Skybox"_hsv, *m_Graph, {
        .Equirectangular = m_Graph->Import(
            "Equirectangular"_hsv, m_ImageAssetManager->Get(equirectangular), ImageLayout::Readonly),
        .Cubemap = m_Graph->ImportPersistent("Cubemap"_hsv, m_SkyboxTexture),
        .Exposure = Passes::PbrCameraExposure::convertEV100ToExposure(
            *CVars::Get().GetF32CVar("Renderer.FixedExposure"_hsv))
    }).Cubemap;
    
    Passes::DiffuseIrradianceSH::addToGraph(
        "Scene.DiffuseIrradianceSH"_hsv, *m_Graph, cubemap, 
        m_Graph->ImportPersistent("IrradianceSH"_hsv, m_IrradianceSH), false);
    Passes::EnvironmentPrefilter::addToGraph(
        "Scene.EnvironmentPrefilter"_hsv, *m_Graph, cubemap,
        m_Graph->ImportPersistent("SkyboxPrefilterMap"_hsv, m_SkyboxPrefilterMap), false);
    Passes::BRDFLut::addToGraph(
        "Scene.BRDFLut"_hsv, *m_Graph, {.Lut = m_Graph->ImportPersistent("BRDFLut"_hsv, m_BRDFLut)});

    m_Graph->Compile(GetFrameContext());
    m_Graph->Execute(GetFrameContext());

    m_ImageAssetManager->UnloadResource(equirectangular);
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

    ImageResource backbuffer = m_Graph->GetBackbufferImage();
    
    m_SceneGeometryRGResources = SceneGeometryRGResources::ForGeometry(m_Scene->Geometry(), 
        *m_Graph);

    ImageResource color = m_Graph->Create("Color"_hsv, ResourceCreationFlags::AutoUpdate, RGImageDescription{
        .Inference = RGImageInference::Size2d,
        .Reference = backbuffer,
        .Format = Format::RGBA16_FLOAT});
    ImageResource vbuffer = m_Graph->Create("VBuffer"_hsv, ResourceCreationFlags::AutoUpdate, RGImageDescription{
        .Inference = RGImageInference::Size2d,
        .Reference = backbuffer,
        .Format = Format::R32_UINT});
    ImageResource depth = m_Graph->Create("Depth"_hsv, ResourceCreationFlags::AutoUpdate, RGImageDescription{
        .Inference = RGImageInference::Size2d,
        .Reference = backbuffer,
        .Format = Format::D32_FLOAT});
    
    auto& shadowPass = m_OpaqueSet.FindPass("Shadow"_hsv);
    auto* depthPrepass = m_OpaqueSet.TryFindPass("DepthPrepass"_hsv);
    auto& pbrPass = m_OpaqueSet.FindPass("ForwardPbr"_hsv);
    auto& vbufferPass = m_OpaqueSet.FindPass("Vbuffer"_hsv);
    
    std::vector<SceneDrawPassDescription> drawPasses;
    
    m_Scene->IterateLights(lux::LightType::Directional, 
        [this](lux::CommonLight& commonLight, Transform3d& localTransform) {
            if (!commonLight.IsSun)
                return false;
            
            m_SunLight = &commonLight;
            
            ImGui::Begin("Directional Light");
            glm::vec3 euler = glm::eulerAngles(localTransform.Orientation) * 180.0f / glm::pi<f32>();
            ImGui::DragFloat3("Direction", &euler[0], 1e-1f);
            ImGui::ColorEdit3("Color", &commonLight.Color[0]);
            ImGui::DragFloat("Intensity", &commonLight.Intensity, 10.0f, 0.0f);
            ImGui::End();
            localTransform.Orientation = glm::quat(euler * glm::pi<f32>() / 180.0f);
            
            return true;
    });
    ASSERT(m_SunLight)

    bool renderAtmosphere = CVars::Get().GetI32CVar("Renderer.Atmosphere"_hsv).value_or(false);
    ImGui::Begin("RenderAtmosphere");
    ImGui::Checkbox("Enabled", &renderAtmosphere);
    CVars::Get().SetI32CVar("Renderer.Atmosphere"_hsv, renderAtmosphere);
    ImGui::End();
    
    Passes::Atmosphere::LutPasses::PassData* atmosphereLuts = nullptr;
    CloudMapsInfo cloudMaps = {};
    AtmosphereEnvironmentInfo skyAtmosphereWithCloudsEnvironment = {};
    if (renderAtmosphere)
    {
        atmosphereLuts = &RenderGraphAtmosphereLutPasses();
        cloudMaps = RenderGraphGetCloudMaps();
        skyAtmosphereWithCloudsEnvironment = RenderGraphAtmosphereEnvironment(*atmosphereLuts, cloudMaps);
    }
    
    if (m_Scene->Geometry().SkinnedRenderObjectCount > 0)
    {
        auto& skinning = Passes::ComputeSkinning::addToGraph("Skinning"_hsv, *m_Graph, {
            .RenderObjects = m_SceneGeometryRGResources.RenderObjects,
            .Meshlets = m_SceneGeometryRGResources.Meshlets,
            .Skins = m_SceneGeometryRGResources.Skins,
            .RenderObjectSkinnedInfos = m_SceneGeometryRGResources.RenderObjectSkinnedInfos,
            .RenderObjectSkinnedInfoIndices = m_SceneGeometryRGResources.RenderObjectSkinnedInfoIndices,
            .Ugb = m_SceneGeometryRGResources.Attributes,
            .JointMatrices = m_SceneGeometryRGResources.JointMatrices,
            .BlendShapes = m_SceneGeometryRGResources.BlendShapes,
            .SkinnedRenderObjectCount = m_Scene->Geometry().SkinnedRenderObjectCount,
            .SkinnedMeshletCount = m_Scene->Geometry().SkinnedMeshletCount
        });
        
        m_SceneGeometryRGResources.RenderObjects = skinning.RenderObjects;
        m_SceneGeometryRGResources.Meshlets = skinning.Meshlets;
        m_SceneGeometryRGResources.Attributes = skinning.Ugb;
    }

    m_OpaqueSetPrimaryView = {
        .Name = "OpaquePrimary"_hsv,
        .ViewInfo = SceneViewInfo(
            m_Graph->GetGlobalResources().PrimaryViewInfoResource,
            (VisibilityFlags)m_Graph->GetGlobalResources().PrimaryViewInfo.Camera.VisibilityFlags)
    };
    
    m_OpaqueSetPrimaryVisibility = m_PrimaryVisibility.AddVisibility(m_OpaqueSetPrimaryView);
    const u32 primaryVisibilityIndex = m_PrimaryVisibility.VisibilityHandleToIndex(m_OpaqueSetPrimaryVisibility);
    m_PrimaryVisibilityResources = SceneVisibilityPassesResources::FromSceneMultiviewVisibility(
        *m_Graph, m_SceneGeometryRGResources, m_PrimaryVisibility);
    m_PrimaryVisibilityResources.HizPrevious[primaryVisibilityIndex] =
        m_PrimaryHizPrevious[GetPreviousFrameNumber()].HasValue() ?
        m_Graph->ImportPersistent("PrimaryHiz.Previous"_hsv, m_PrimaryHizPrevious[GetPreviousFrameNumber()]) :
        m_Graph->Import("PrimaryHiz.Dummy"_hsv,
            Images::Default::GetCopy(Images::DefaultKind::White, GetFrameContext().DeletionQueue),
            ImageLayout::Readonly
        );
    
    bool useForwardPass = CVars::Get().GetI32CVar("Renderer.UseForwardShading"_hsv).value_or(false);
    ImGui::Begin("ForwardShading");
    ImGui::Checkbox("Enabled", &useForwardPass);
    CVars::Get().SetI32CVar("Renderer.UseForwardShading"_hsv, useForwardPass);
    ImGui::End();

    Passes::SceneMetaDraw::PassData* metaUgb = nullptr;
    if (useForwardPass)
        metaUgb = &RenderGraphForwardPass(color, depth);
    else
        metaUgb = &RenderGraphVBuffer(vbuffer, color, depth);

    std::swap(
        m_MinMaxDepthReductionsNextFrame[GetFrameContext().FrameNumber],
        m_MinMaxDepthReductions[GetFrameContext().FrameNumber]);

    ImageResource minMaxDepth =
        m_PrimaryVisibilityResources.Hiz[primaryVisibilityIndex];

    CloudShadowInfo cloudShadow = {};
    ImageResource colorWithSky{};
    CloudsInfo clouds = {};
    if (renderAtmosphere)
    {
        cloudShadow = RenderGraphCloudShadows(cloudMaps);
        
        auto& aerialPerspective = Passes::Atmosphere::AerialPerspective::addToGraph("AtmosphereAerialPerspective"_hsv,
            *m_Graph, {
            .ViewInfo = m_Graph->GetGlobalResources().PrimaryViewInfoResource,
            .TransmittanceLut = atmosphereLuts->TransmittanceLut,
            .MultiscatteringLut = atmosphereLuts->MultiscatteringLut,
            .CsmData = m_CsmData
        });
        
        clouds = RenderGraphClouds(cloudMaps, color, aerialPerspective.Lut, minMaxDepth, depth);
        colorWithSky = RenderGraphAtmosphere(*atmosphereLuts, aerialPerspective.Lut,
            color, depth, m_CsmData, clouds.Color, clouds.Depth, skyAtmosphereWithCloudsEnvironment.CloudsEnvironment);
    }
    else
    {
        colorWithSky = RenderGraphSkyBox(color, depth);
    }
    
    ImageResource colorPreTonemapping = colorWithSky;
    auto& exposure = Passes::PbrCameraExposure::addToGraph("CameraExposure"_hsv,
        *m_Graph, {
            .ViewInfo = m_Graph->Import(
                "PrimaryView"_hsv, m_Graph->GetGlobalResources().PrimaryViewInfoBuffer),
            .Color = colorPreTonemapping,
            .ExposureSettings = &m_ExposureSettings,
        });
    
    if (exposure.HistogramVisualization.IsValid())
    {
        if (m_ExposureSettings.VisualizationInfo.AsOverlay)
            colorPreTonemapping = exposure.HistogramVisualization;
        else 
            Passes::ImGuiTexture::addToGraph("LuminanceHistogram"_hsv, *m_Graph, exposure.HistogramVisualization);
    }
    
    ImageResource colorTonemapped = colorPreTonemapping;
    {
        namespace Tonemapping = Passes::PbrTonemapping;
        ImGui::Begin("Tonemapper");
        u32 currentTonemapper = (u32)m_TonemappingType;
        if (ImGui::BeginCombo("Tonemappers", Tonemapping::tonemappingTypeToString(m_TonemappingType).data(), 0))
        {
            for (u32 i = 0; i < (u32)Tonemapping::TonemappingType::MaxValue; i++)
            {
                const bool isSelected = (currentTonemapper == i);
                if (ImGui::Selectable(Tonemapping::tonemappingTypeToString((Tonemapping::TonemappingType)i).data(), 
                    isSelected))
                    currentTonemapper = i;

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::End();
        
        m_TonemappingType = (Tonemapping::TonemappingType)currentTonemapper;
        auto& tonemapping = Tonemapping::addToGraph("Tonemapping"_hsv, *m_Graph, {
            .Color = colorPreTonemapping,
            .Type = m_TonemappingType
        });
        colorTonemapped = tonemapping.Color;
    }
    
    auto& fxaa = Passes::Fxaa::addToGraph("FXAA"_hsv, *m_Graph, {.Color = colorTonemapped});
    
    ImageResource finalColor = fxaa.AntiAliased;

    if (CVars::Get().GetI32CVar("Postprocessing.CRT"_hsv).value_or(false))
        finalColor = Passes::Crt::addToGraph("CRT"_hsv, *m_Graph, {.Color = finalColor}).Color;

    Passes::CopyTexture::addToGraph("Copy.MainColor"_hsv, *m_Graph, {
        .TextureIn = finalColor,
        .TextureOut = backbuffer
    });

    ImGui::Begin("Debug");
    if (ImGui::Button("Dump memory stats"))
        Device::DumpMemoryStats("./MemoryStats.json");
    ImGui::End();
    
    m_Graph->Export(
        metaUgb->DrawPassViewAttachments.GetMinMaxDepthReduction(m_OpaqueSetPrimaryView.Name),
        m_MinMaxDepthReductionsNextFrame[GetFrameContext().FrameNumber],
        Device::DeletionQueue());

    Passes::ImGuiTexture::addToGraph("TTTTT"_hsv, *m_Graph, 
        m_Graph->ImportPersistent("T"_hsv, m_MipsTest.Update(*m_Graph, *m_ImageAssetManager).Image));

    if (renderAtmosphere)
    {
        m_Graph->Export(atmosphereLuts->TransmittanceLut, m_TransmittanceLut, Device::DeletionQueue(), 
            ImageUsage::Sampled);
        m_Graph->Export(atmosphereLuts->SkyViewLut, m_SkyViewLut, Device::DeletionQueue(), ImageUsage::Sampled);
        m_Graph->Export(cloudShadow.Shadow, m_VolumetricCloudShadow, Device::DeletionQueue(), ImageUsage::Sampled);
    }

    if (renderAtmosphere)
    {
        if (m_CloudsReprojectionEnabled && !m_CloudColorAccumulation.front().HasValue())
        {
            constexpr ImageUsage extraUsage = ImageUsage::Storage | ImageUsage::Sampled | ImageUsage::Source;
            m_Graph->Export(clouds.ColorPrevious, m_CloudColorAccumulation[0], Device::DeletionQueue(), extraUsage);
            m_Graph->Export(clouds.DepthPrevious, m_CloudDepthAccumulation[0], Device::DeletionQueue(), extraUsage);
            m_Graph->Export(clouds.ReprojectionPrevious, m_CloudReprojectionFactor[0], Device::DeletionQueue(),
                extraUsage);
            m_Graph->Export(clouds.Color, m_CloudColorAccumulation[1], Device::DeletionQueue(), extraUsage);
            m_Graph->Export(clouds.Depth, m_CloudDepthAccumulation[1], Device::DeletionQueue(), extraUsage);
            m_Graph->Export(clouds.Reprojection, m_CloudReprojectionFactor[1], Device::DeletionQueue(), extraUsage);
        }

        if (!m_SkyAtmosphereWithCloudsEnvironment.HasValue())
            m_Graph->Export(skyAtmosphereWithCloudsEnvironment.AtmosphereWithClouds, 
                m_SkyAtmosphereWithCloudsEnvironment, Device::DeletionQueue());
        if (!m_CloudsEnvironment.HasValue())
            m_Graph->Export(skyAtmosphereWithCloudsEnvironment.CloudsEnvironment, 
                m_CloudsEnvironment, Device::DeletionQueue());
    }

    m_Graph->Export(m_PrimaryVisibilityResources.Hiz[primaryVisibilityIndex],
        m_PrimaryHizPrevious[GetFrameContext().FrameNumber], Device::DeletionQueue());

    std::swap(m_CloudsAccumulationIndex, m_CloudsAccumulationIndexPrev);
    
    m_Graph->Compile(GetFrameContext());
    
    if (renderAtmosphere)
    {
        m_BindlessTextureDescriptorsRingBuffer->SetTexture(m_TransmittanceLutBindlessIndex, 
            m_Graph->GetImage(m_TransmittanceLut));
        m_BindlessTextureDescriptorsRingBuffer->SetTexture(m_SkyViewLutBindlessIndex, 
            m_Graph->GetImage(m_SkyViewLut));
        m_BindlessTextureDescriptorsRingBuffer->SetTexture(m_VolumetricShadowBindlessIndex,
            m_Graph->GetImage(m_VolumetricCloudShadow));
    }
}

void Renderer::UpdateGlobalRenderGraphResources()
{
    using namespace RG;
    
    auto& blackboard = m_Graph->GetBlackboard();

    SwapchainDescription& swapchain = Device::GetSwapchainDescription(m_Swapchain);

    if (!blackboard.TryGet<GlobalResources>())
    {
        ViewInfoGPU primaryView = ViewInfoGPU::Default();
        Buffer primaryViewBuffer = Device::CreateBuffer({
            .Description = {
                .SizeBytes = sizeof(ViewInfoGPU),
                .Usage = BufferUsage::Ordinary | BufferUsage::Source | BufferUsage::Uniform | BufferUsage::Storage
            },
        });
        Buffer primaryViewEnvironmentCaptureBuffer = Device::CreateBuffer({
            .Description = {
                .SizeBytes = sizeof(ViewInfoGPU),
                .Usage = BufferUsage::Ordinary | BufferUsage::Source | BufferUsage::Uniform | BufferUsage::Storage
            },
        });
        
        BufferResource primaryViewResource = Passes::CopyToBuffer::addToGraph("CopyInitialPrimaryView"_hsv, *m_Graph, {
            .Source = {primaryView},
            .Destination = m_Graph->Import("InitialPrimaryView"_hsv, primaryViewBuffer)
        }).Destination;

        BufferResource primaryViewEnvironmentCaptureResource = Passes::CopyBuffer::addToGraph("CopyPrimaryViewEnv"_hsv,
            *m_Graph, {
                .Source = primaryViewResource,
                .Destination = m_Graph->Import("InitialPrimaryEnvironmentView"_hsv, primaryViewEnvironmentCaptureBuffer),
                .SizeBytes = sizeof(ViewInfoGPU),
            }).Destination;
        
        blackboard.Update<GlobalResources>({
            .PrimaryViewInfo = primaryView, 
            .PrimaryViewInfoBuffer = primaryViewBuffer,
            .PrimaryViewInfoEnvironmentCaptureBuffer = primaryViewEnvironmentCaptureBuffer,
            .PrimaryViewInfoResource = primaryViewResource,
            .PrimaryViewInfoEnvironmentCaptureResource = primaryViewEnvironmentCaptureResource,
        });
    }

    GlobalResources& globalResources = blackboard.Get<GlobalResources>();
    ViewInfoGPU& primaryView = globalResources.PrimaryViewInfo;

    if (m_FrameNumber > 0)
        primaryView.PreviousCamera = primaryView.Camera;
    primaryView.Camera = CameraGPU::FromCamera(*m_Camera, swapchain.SwapchainResolution,
        VisibilityFlags::IsPrimaryView | VisibilityFlags::OcclusionCull);
    
    primaryView.Shading.TransmittanceLut = m_TransmittanceLutBindlessIndex;
    primaryView.Shading.SkyViewLut = m_SkyViewLutBindlessIndex;
    primaryView.Shading.VolumetricCloudShadow = m_VolumetricShadowBindlessIndex;
    primaryView.Shading.BlueNoise128 = m_BlueNoiseBindlessIndex;
    primaryView.Shading.MaxLightCullDistance =
        *CVars::Get().GetF32CVar("Renderer.Limits.MaxLightCullDistance"_hsv);
    primaryView.Shading.DirectionalLightCount = m_Scene->Lights().DirectionalLightCount();
    primaryView.Shading.PointLightCount = m_Scene->Lights().PointLightCount();
    // todo: toC VAR
    primaryView.Shading.LightCullingUseZBins = true;
    primaryView.Shading.LightCullTileCount =
        (swapchain.SwapchainResolution + glm::uvec2(LIGHT_TILE_SIZE_X, LIGHT_TILE_SIZE_Y) - glm::uvec2(1)) /
        glm::uvec2(LIGHT_TILE_SIZE_X, LIGHT_TILE_SIZE_Y);

    if (m_SunLight)
    {
        primaryView.Shading.PrimaryDirectionalLightDirection = m_SunLight->PositionDirection;
        primaryView.Shading.PrimaryDirectionalLightColor = glm::vec4(m_SunLight->Color, 1.0f);
        primaryView.Shading.PrimaryDirectionalLightIntensity = m_SunLight->Intensity;
        primaryView.Shading.OuterSpacePrimaryDirectionalLightIlluminance = 
            glm::vec4(m_SunLight->Color, 1.0f) * m_SunLight->Intensity;
        primaryView.Shading.CameraPrimaryDirectionalLightIlluminance = 
            primaryView.Shading.OuterSpacePrimaryDirectionalLightIlluminance;
    }

    const bool renderAtmosphere = CVars::Get().GetI32CVar("Renderer.Atmosphere"_hsv).value_or(false);
    if (m_SunLight && renderAtmosphere)
    {
        const CameraGPU cloudShadowCamera = Passes::Clouds::VP::Shadow::createShadowCamera(
            *globalResources.PrimaryCamera, primaryView, m_SunLight->PositionDirection);
        primaryView.Shading.VolumetricCloudViewProjection = cloudShadowCamera.ViewProjection;
        primaryView.Shading.VolumetricCloudView = cloudShadowCamera.View;
    }
    
    ImGui::Begin("Shading Settings");
    ImGui::DragFloat("Environment power", &primaryView.Shading.EnvironmentPower, 1e-2f, 0.0f, 1.0f);
    ImGui::Checkbox("Soft shadows", (bool*)&primaryView.Shading.SoftShadows);
    ImGui::DragFloat("Volumetric cloud shadows strength", &primaryView.Shading.VolumetricCloudShadowStrength,
        1e-2f, 0.0f, 1.0f);
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

    {
        f32 shutterTimeInverse = 1.0f / m_ExposureSettings.ShutterTime;
        ImGui::Begin("Camera exposure settings");
        ImGui::DragFloat("Aperture f stops", &m_ExposureSettings.Aperture, 1e-2f, 0.1f, 30.0f);
        ImGui::DragFloat("ShutterTime inverse", &shutterTimeInverse, 1e-1f, 0.0f, 200.0f);
        ImGui::DragFloat("ISO", &m_ExposureSettings.ISO, 1e-1f, 0.0f, 300.0f);
        ImGui::DragFloat("Center metering", &m_ExposureSettings.CenterMeteringStrength, 1e-2f, 0.0f, 32.0f);
        ImGui::Checkbox("Auto", &m_ExposureSettings.UseAutomaticExposure);
        ImGui::Checkbox("Visualize", &m_ExposureSettings.Visualize);
        ImGui::Checkbox("Overlay", &m_ExposureSettings.VisualizationInfo.AsOverlay);
        m_ExposureSettings.ShutterTime = 1.0f / shutterTimeInverse;
        ImGui::End();
    }

    primaryView.Dt = 1.0f / 60.0f;
    primaryView.FrameNumber = (f32)GetFrameContext().FrameNumberTick;
    primaryView.FrameNumberU32 = (u32)GetFrameContext().FrameNumberTick;
    
    primaryView.Shading.FixedExposure = Passes::PbrCameraExposure::convertEV100ToExposure(
        *CVars::Get().GetF32CVar("Renderer.FixedExposure"_hsv));
    primaryView.Shading.FixedExposureInverse = 1.0f / primaryView.Shading.FixedExposure;

    globalResources.FrameNumberTick = GetFrameContext().FrameNumberTick;
    globalResources.Resolution = GetFrameContext().Resolution;
    globalResources.PrimaryCamera = m_Camera.get();

    if (globalResources.FrameNumberTick > 0)
    {
        globalResources.PrimaryViewInfoResource = m_Graph->Import(
            "PrimaryView"_hsv, globalResources.PrimaryViewInfoBuffer);

        globalResources.PrimaryViewInfoEnvironmentCaptureResource = m_Graph->Import("InitialPrimaryView"_hsv,
            globalResources.PrimaryViewInfoEnvironmentCaptureBuffer);
    }

    const u64 shadingInfoExposeOffsetBegin = offsetof(ViewInfoGPU, Shading) + offsetof(ShadingSettings, Exposure);
    const u64 shadingInfoExposeOffsetEnd = offsetof(ViewInfoGPU, Shading) + 
        offsetof(ShadingSettings, ExposureToFixedExposureMultiplier) +
        sizeof(ShadingSettings::ExposureToFixedExposureMultiplier);
    globalResources.PrimaryViewInfoResource = Passes::CopyToBuffer::addToGraph("CopyPrimaryViewInfoChunk"_hsv,
        *m_Graph, {
            .Source = Span<const std::byte>({primaryView}).subspan(0, shadingInfoExposeOffsetBegin),
            .Destination = globalResources.PrimaryViewInfoResource,
            .DestinationOffset = 0
    }).Destination;
    globalResources.PrimaryViewInfoResource = Passes::CopyToBuffer::addToGraph("CopyPrimaryViewInfoChunk"_hsv,
        *m_Graph, {
            .Source = Span<const std::byte>({primaryView}).subspan(
                shadingInfoExposeOffsetEnd, sizeof(primaryView) - shadingInfoExposeOffsetEnd),
            .Destination = globalResources.PrimaryViewInfoResource,
            .DestinationOffset = shadingInfoExposeOffsetEnd
    }).Destination;
}

RG::CsmData Renderer::RenderGraphShadows(const ScenePass& scenePass, const lux::CommonLight& directionalLight)
{
    using namespace RG;

    bool useGpuShadowCameras = CVars::Get().GetI32CVar("Renderer.UseGpuShadowCameras"_hsv).value_or(false);
    ImGui::Begin("UseGpuShadowCameras");
    ImGui::Checkbox("Enabled", &useGpuShadowCameras);
    CVars::Get().SetI32CVar("Renderer.UseGpuShadowCameras"_hsv, useGpuShadowCameras);
    ImGui::End();

    f32 shadowMin = 0.01f;
    f32 shadowMax = 100.0f;
    if (!useGpuShadowCameras)
    {
        if (m_MinMaxDepthReductions[GetFrameContext().FrameNumber].HasValue())
        {
            auto& readBackDepthMinMax = Passes::DepthReductionReadback::addToGraph("DepthReductionReadback"_hsv,
                *m_Graph, {
                    .MinMaxDepthReduction = m_Graph->ImportPersistent("DepthReduction"_hsv,
                        m_MinMaxDepthReductions[GetFrameContext().FrameNumber]),
                    .PrimaryCamera = m_Camera.get()
                });
            shadowMin = readBackDepthMinMax.Min;
            shadowMax = readBackDepthMinMax.Max;
        }
    }
    
    auto& csmInit = Passes::SceneCsm::addToGraph("CSM"_hsv,
        *m_Graph, {
            .Pass = &scenePass,
            .Geometry = &m_SceneGeometryRGResources,
            .MultiviewVisibility = &m_ShadowMultiviewVisibility,
            .MainCamera = m_Camera.get(),
            .DirectionalLight = DirectionalLight{{
                .Direction = directionalLight.PositionDirection,
                .Color = directionalLight.Color,
                .Intensity = directionalLight.Intensity,
                .Radius = directionalLight.Radius
            }},
            .ShadowMin = shadowMin,
            .ShadowMax = shadowMax,
            .DepthMinMaxBuffer = m_DepthMinMaxCurrentFrame,
            .CreateCamerasInGpu = useGpuShadowCameras,
            .StabilizeCascades = false
        });

    m_ShadowMultiviewVisibilityResources = SceneVisibilityPassesResources::FromSceneMultiviewVisibility(
        *m_Graph, m_SceneGeometryRGResources, m_ShadowMultiviewVisibility);
    
    auto& meta = Passes::SceneMetaDraw::addToGraph("MetaCsmPass"_hsv,
        *m_Graph, {
            .MultiviewVisibility = &m_ShadowMultiviewVisibility,
            .Resources = &m_ShadowMultiviewVisibilityResources,
            .DrawPasses = csmInit.MetaPassDescriptions
        });

    Passes::SceneCsm::mergeCsm(*m_Graph, csmInit, scenePass, meta.DrawPassViewAttachments);

    Passes::ImGuiArrayTexture::addToGraph("Csm atlas"_hsv, *m_Graph, csmInit.CsmData.ShadowMap,
        Passes::ImGuiArrayTexture::DrawAs::Atlas, Passes::ChannelComposition::RComposition());

    return csmInit.CsmData;
}

Passes::SceneMetaDraw::PassData& Renderer::RenderGraphDepthPrepass(RG::ImageResource depth, const ScenePass& scenePass)
{
    using namespace RG;
    
    auto& meta = Passes::SceneMetaDraw::addToGraph("MetaDepthPrepass"_hsv,
        *m_Graph, {
            .MultiviewVisibility = &m_PrimaryVisibility,
            .Resources = &m_PrimaryVisibilityResources,
            .DrawPasses = {RenderGraphDepthPrepassDescription(depth, scenePass)}
        });

    return meta;
}

SceneDrawPassDescription Renderer::RenderGraphDepthPrepassDescription(RG::ImageResource depth, 
    const ScenePass& scenePass)
{
    using namespace RG;
    
    auto initDepthPrepass = [&](StringId name, Graph& graph, const SceneDrawPassExecutionInfo& info)
    {
        auto& pass = Passes::SceneDepthPrepass::addToGraph(
            name.Concatenate(".DepthPrepass"), graph, {
                .DrawInfo = info,
                .Geometry = &m_SceneGeometryRGResources});

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

SceneDrawPassDescription Renderer::RenderGraphForwardPbrDescription(RG::ImageResource color, RG::ImageResource depth,
    RG::CsmData csmData, const ScenePass& scenePass)
{
    using namespace RG;

    auto initForwardPbr = [&](StringId name, Graph& graph, const SceneDrawPassExecutionInfo& info)
    {
        const bool renderAtmosphere = CVars::Get().GetI32CVar("Renderer.Atmosphere"_hsv).value_or(false);
        
        Passes::SceneForwardPbr::ExecutionInfo executionInfo = {
            .DrawInfo = info,
            .Geometry = &m_SceneGeometryRGResources,
            .DirectionalLights = m_Graph->Import(
                "DirectionalLights"_hsv, m_Scene->Lights().GetBuffers().DirectionalLights),
            .PointLights = m_Graph->Import("PointLights"_hsv, m_Scene->Lights().GetBuffers().PointLights),
            .SSAO = {.SSAO = m_Ssao},
            .IBL = {
                .IrradianceSH = renderAtmosphere ? 
                    m_SkyIrradianceSHResource :
                    m_Graph->ImportPersistent("IrradianceSH"_hsv, m_IrradianceSH),
                .PrefilterEnvironment = renderAtmosphere ?
                    m_SkyPrefilterMapResource :
                    m_Graph->ImportPersistent("PrefilterMap"_hsv, m_SkyboxPrefilterMap),
                .BRDF = m_Graph->ImportPersistent("BRDF"_hsv, m_BRDFLut)
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

SceneDrawPassDescription Renderer::RenderGraphVBufferDescription(RG::ImageResource vbuffer, RG::ImageResource depth,
    const ScenePass& scenePass)
{
    using namespace RG;

    auto initVbuffer = [&](StringId name, Graph& graph, const SceneDrawPassExecutionInfo& info)
    {
        Passes::SceneVBuffer::ExecutionInfo executionInfo = {
            .DrawInfo = info,
            .Geometry = &m_SceneGeometryRGResources
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

RG::ImageResource Renderer::RenderGraphVBufferPbr(RG::ImageResource vbuffer, RG::BufferResource visibleMeshlets, 
    RG::BufferResource viewInfo, RG::CsmData csmData)
{
    const bool renderAtmosphere = CVars::Get().GetI32CVar("Renderer.Atmosphere"_hsv).value_or(false);
    
    auto& pbr = Passes::SceneVBufferPbr::addToGraph("VBufferPbr"_hsv, *m_Graph, {
        .Geometry = &m_SceneGeometryRGResources,
        .VisibleMeshlets = visibleMeshlets,
        .VisibilityTexture = vbuffer,
        .ViewInfo = viewInfo,
        .DirectionalLights = m_Graph->Import("DirectionalLights"_hsv, m_Scene->Lights().GetBuffers().DirectionalLights),
        .PointLights = m_Graph->Import("PointLights"_hsv, m_Scene->Lights().GetBuffers().PointLights),
        .SSAO = {.SSAO = m_Ssao},
        .IBL = {
            .IrradianceSH = renderAtmosphere ? m_SkyIrradianceSHResource :
                m_Graph->ImportPersistent("IrradianceSH"_hsv, m_IrradianceSH),
            .PrefilterEnvironment = renderAtmosphere ? m_SkyPrefilterMapResource :
                m_Graph->ImportPersistent("PrefilterMap"_hsv, m_SkyboxPrefilterMap),
            .BRDF = m_Graph->ImportPersistent("BRDF"_hsv, m_BRDFLut)
        },
        .Clusters = m_ClusterLightsInfo.Clusters,
        .Tiles = m_TileLightsInfo.Tiles,
        .ZBins = m_TileLightsInfo.ZBins,
        .CsmData = csmData,
    });

    return pbr.Color;
}

Passes::SceneMetaDraw::PassData& Renderer::RenderGraphForwardPass(RG::ImageResource& color, RG::ImageResource& depth)
{
    auto& shadowPass = m_OpaqueSet.FindPass("Shadow"_hsv);
    auto* depthPrepass = m_OpaqueSet.TryFindPass("DepthPrepass"_hsv);
    auto& pbrPass = m_OpaqueSet.FindPass("ForwardPbr"_hsv);

    if (CVars::Get().GetI32CVar("Renderer.DepthPrepass"_hsv).value_or(false))
    {
        auto& depthPrepassMeta = RenderGraphDepthPrepass(depth, *depthPrepass);
        m_DepthMinMaxCurrentFrame = depthPrepassMeta.DrawPassViewAttachments.GetMinMaxDepthReduction(
            m_OpaqueSetPrimaryView.Name);
        depth = depthPrepassMeta.DrawPassViewAttachments.Get(
            m_OpaqueSetPrimaryView.Name, depthPrepass->Name()).Depth->Resource;

        RenderGraphOnFrameDepthGenerated(depthPrepass->Name(), depth);
    }

    if (m_SunLight != nullptr)
        m_CsmData = RenderGraphShadows(shadowPass, *m_SunLight);
    
    auto& meta = Passes::SceneMetaDraw::addToGraph("MetaUgb"_hsv,
        *m_Graph, {
            .MultiviewVisibility = &m_PrimaryVisibility,
            .Resources = &m_PrimaryVisibilityResources,
            .DrawPasses = {RenderGraphForwardPbrDescription(color, depth, m_CsmData, pbrPass)}
        });

    color = meta.DrawPassViewAttachments.Get(m_OpaqueSetPrimaryView.Name, pbrPass.Name()).Colors[0].Resource;

    return meta;
}

Passes::SceneMetaDraw::PassData& Renderer::RenderGraphVBuffer(RG::ImageResource& vbuffer, RG::ImageResource& color, 
    RG::ImageResource& depth)
{
    auto& shadowPass = m_OpaqueSet.FindPass("Shadow"_hsv);
    auto& vbufferPass = m_OpaqueSet.FindPass("Vbuffer"_hsv);
    
    auto& meta = Passes::SceneMetaDraw::addToGraph("MetaUgb"_hsv,
        *m_Graph, {
            .MultiviewVisibility = &m_PrimaryVisibility,
            .Resources = &m_PrimaryVisibilityResources,
            .DrawPasses = {RenderGraphVBufferDescription(vbuffer, depth, vbufferPass)}});

    m_DepthMinMaxCurrentFrame = meta.DrawPassViewAttachments.GetMinMaxDepthReduction(m_OpaqueSetPrimaryView.Name);

    if (m_SunLight != nullptr)
        m_CsmData = RenderGraphShadows(shadowPass, *m_SunLight);

    depth = meta.DrawPassViewAttachments.Get(m_OpaqueSetPrimaryView.Name, vbufferPass.Name()).Depth->Resource;
    vbuffer = meta.DrawPassViewAttachments.Get(m_OpaqueSetPrimaryView.Name, vbufferPass.Name()).Colors[0].Resource;
    RenderGraphOnFrameDepthGenerated("VBufferDepth"_hsv, depth);
    color = RenderGraphVBufferPbr(vbuffer, m_PrimaryVisibilityResources.VisibleMeshletsData,
        m_Graph->GetGlobalResources().PrimaryViewInfoResource, m_CsmData);

    return meta;
}

void Renderer::RenderGraphOnFrameDepthGenerated(StringId passName, RG::ImageResource depth)
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

RG::ImageResource Renderer::RenderGraphSSAO(StringId baseName, RG::ImageResource depth)
{
    using namespace RG;

    auto& ssao = Passes::Ssao::addToGraph(baseName.Concatenate("SSAO"), *m_Graph, {
        .Depth = depth,
        .MaxSampleCount = 32});

    auto& ssaoBlurHorizontal = Passes::SsaoBlur::addToGraph(baseName.Concatenate("SSAO.Blur.Horizontal"), *m_Graph, {
        .SsaoIn = ssao.Ssao,
        .SsaoOut = {},
        .BlurKind = SsaoBlurPassKind::Horizontal});
    auto& ssaoBlurVertical = Passes::SsaoBlur::addToGraph(baseName.Concatenate("SSAO.Blur.Vertical"), *m_Graph, {
        .SsaoIn = ssaoBlurHorizontal.Ssao,
        .SsaoOut = ssao.Ssao,
        .BlurKind = SsaoBlurPassKind::Vertical});

    Passes::ImGuiTexture::addToGraph(baseName.Concatenate("SSAO.Visualize"), *m_Graph, ssaoBlurVertical.Ssao,
        Passes::ChannelComposition::RComposition());

    return ssaoBlurVertical.Ssao;
}

Renderer::TileLightsInfo Renderer::RenderGraphCullLightsTiled(StringId baseName, RG::ImageResource depth)
{
    using namespace RG;
    
    struct TileLightsInfo
    {
        BufferResource Tiles{};
        BufferResource ZBins{};
    };

    auto zbins = LightZBinner::ZBinLights(m_Scene->Lights(), *GetFrameContext().PrimaryCamera);
    BufferResource zbinsResource = Passes::Upload::addToGraph(baseName.Concatenate("Upload.Light.ZBins"), *m_Graph,
        zbins.Bins);
    auto& tilesSetup = Passes::LightTilesSetup::addToGraph(baseName.Concatenate("Tiles.Setup"), *m_Graph, {
        .ViewInfo =  m_Graph->GetGlobalResources().PrimaryViewInfoResource
    });
    auto& binLightsTiles = Passes::LightTilesBin::addToGraph(baseName.Concatenate("Tiles.Bin"), *m_Graph, {
        .ViewInfo = m_Graph->GetGlobalResources().PrimaryViewInfoResource,
        .Tiles = tilesSetup.Tiles, 
        .Depth = depth,
        .PointLights = m_Graph->Import("PointLights"_hsv, m_Scene->Lights().GetBuffers().PointLights),
    });
    auto& visualizeTiles = Passes::LightTilesVisualize::addToGraph(baseName.Concatenate("Tiles.Visualize"), *m_Graph, {
        .ViewInfo = m_Graph->GetGlobalResources().PrimaryViewInfoResource,
        .Tiles = binLightsTiles.Tiles,
        .Bins = zbinsResource,
        .Depth = depth});
    Passes::ImGuiTexture::addToGraph(baseName.Concatenate("Tiles.Visualize.Texture"), *m_Graph,
        visualizeTiles.Color);

    return {
        .Tiles = binLightsTiles.Tiles,
        .ZBins = zbinsResource};
}

Renderer::ClusterLightsInfo Renderer::RenderGraphCullLightsClustered(StringId baseName, RG::ImageResource depth)
{
    using namespace RG;
    
    auto& blackboard = m_Graph->GetBlackboard();
    
    struct ClusterLightsInfo
    {
        BufferResource Clusters{};
    };
    
    auto& clustersSetup = Passes::LightClustersSetup::addToGraph(baseName.Concatenate("Clusters.Setup"), *m_Graph, {
        .ViewInfo = m_Graph->GetGlobalResources().PrimaryViewInfoResource
    });
    auto& binLightsClusters = Passes::LightClustersBin::addToGraph(baseName.Concatenate("Clusters.Bin"), *m_Graph, {
        .ViewInfo = m_Graph->GetGlobalResources().PrimaryViewInfoResource,
        .Clusters = clustersSetup.Clusters,
        .ClusterVisibility = clustersSetup.ClusterVisibility,
        .Depth = depth,
        .PointLights = m_Graph->Import("PointLights"_hsv, m_Scene->Lights().GetBuffers().PointLights),
    });

    auto& visualizeClusters = Passes::LightClustersVisualize::addToGraph(baseName.Concatenate("Clusters.Visualize"),
        *m_Graph, {
        .ViewInfo = m_Graph->GetGlobalResources().PrimaryViewInfoResource,
        .Clusters = binLightsClusters.Clusters,
        .Depth = depth
    });
    Passes::ImGuiTexture::addToGraph(baseName.Concatenate("Clusters.Visualize.Texture"), *m_Graph,
        visualizeClusters.Color);

    return {
        .Clusters = binLightsClusters.Clusters};
}

RG::ImageResource Renderer::RenderGraphSkyBox(RG::ImageResource color, RG::ImageResource depth)
{
    auto& skybox = Passes::Skybox::addToGraph("Skybox"_hsv, *m_Graph, {
        .ViewInfo = m_Graph->GetGlobalResources().PrimaryViewInfoResource,
        .SkyboxResource = m_Graph->ImportPersistent("SkyboxTexture"_hsv, m_SkyboxTexture),
        .Color = color,
        .Depth = depth,
        .Resolution = GetFrameContext().Resolution
    });

    return skybox.Color;
}

Passes::Atmosphere::LutPasses::PassData& Renderer::RenderGraphAtmosphereLutPasses()
{
    auto& luts = Passes::Atmosphere::LutPasses::addToGraph("AtmosphereLutPasses"_hsv, *m_Graph, {
        .ViewInfo = m_Graph->GetGlobalResources().PrimaryViewInfoResource,
    });
    
    if (m_FrameNumber == 0)
    {
        const RG::ImageResource copy = Passes::CopyTexture::addToGraph("CopySkyViewEnvironment"_hsv, *m_Graph,
            {.TextureIn = luts.SkyViewLut}).TextureOut;
        m_Graph->Export(copy, m_SkyViewEnvironmentCaptureLut, Device::DeletionQueue(), 
            ImageUsage::Sampled | ImageUsage::Destination);
    }
        

    m_Graph->GetBlackboard().Get<RG::GlobalResources>().PrimaryViewInfoResource = luts.ViewInfo;
    
    Passes::ImGuiTexture::addToGraph("Atmosphere.Transmittance.Lut"_hsv, *m_Graph, luts.TransmittanceLut);
    Passes::ImGuiTexture::addToGraph("Atmosphere.Multiscattering.Lut"_hsv, *m_Graph, luts.MultiscatteringLut);
    Passes::ImGuiTexture::addToGraph("Atmosphere.SkyView.Lut"_hsv, *m_Graph, luts.SkyViewLut);

    return luts;
}

Renderer::AtmosphereEnvironmentInfo Renderer::RenderGraphAtmosphereEnvironment(
    Passes::Atmosphere::LutPasses::PassData& lut, const CloudMapsInfo& cloudMaps)
{
    const u32 faceIndex = (u32)(m_FrameNumber % 6);
    
    
    m_SkyIrradianceSHResource = m_Graph->ImportPersistent("SkyIrradiance.ImportPersistent"_hsv, m_SkyIrradianceSH);
    m_SkyPrefilterMapResource = m_Graph->ImportPersistent("SkyPrefilterMap"_hsv, m_SkyPrefilterMap);
    const RG::ImageResource skyViewLut = m_Graph->ImportPersistent("SkyViewLutEnv"_hsv, m_SkyViewEnvironmentCaptureLut);
    
    auto& environment = Passes::Atmosphere::Environment::addToGraph("Atmosphere.Environment"_hsv, *m_Graph, {
        .PrimaryView = &m_Graph->GetGlobalResources().PrimaryViewInfo,
        .PrimaryViewResource = m_Graph->GetGlobalResources().PrimaryViewInfoEnvironmentCaptureResource,
        .SkyViewLut = skyViewLut,
        .ColorIn = m_SkyAtmosphereWithCloudsEnvironment.HasValue() ?
            m_Graph->ImportPersistent("AtmosphereEnvironment.Imported"_hsv, m_SkyAtmosphereWithCloudsEnvironment) :
            RG::ImageResource{},
        .FaceIndices = m_FrameNumber == 0 ? Span<const u32>({0, 1, 2, 3, 4, 5}) : Span<const u32>({faceIndex})
    });
    
    if (m_FrameNumber == 0)
    {
        m_SkyIrradianceSHResource = Passes::DiffuseIrradianceSH::addToGraph("Sky.DiffuseIrradianceSH"_hsv, *m_Graph,
            environment.Color, m_SkyIrradianceSHResource, true).DiffuseIrradiance;
    }
        
    auto& cloudsEnvironment = Passes::Clouds::VP::Environment::addToGraph("Clouds.Environment"_hsv, *m_Graph, {
        .PrimaryView = &m_Graph->GetGlobalResources().PrimaryViewInfo,
        .PrimaryViewResource = m_Graph->GetGlobalResources().PrimaryViewInfoEnvironmentCaptureResource,
        .CloudCoverage = cloudMaps.Coverage,
        .CloudProfile = cloudMaps.Profile,
        .CloudShapeLowFrequencyMap = cloudMaps.ShapeLowFrequency,
        .CloudShapeHighFrequencyMap = cloudMaps.ShapeHighFrequency, 
        .CloudCurlNoise = cloudMaps.CurlNoise,
        .ColorIn = m_CloudsEnvironment.HasValue() ?
            m_Graph->ImportPersistent("CloudsEnvironment.Imported"_hsv, m_CloudsEnvironment) :
            RG::ImageResource{},
        .AtmosphereEnvironment = environment.Color,
        .IrradianceSH = m_SkyIrradianceSHResource,
        .CloudParameters = m_CloudParametersResource,
        .CloudsRenderingMode = Passes::Clouds::VP::CloudsRenderingMode::FullResolution,
        .FaceIndices = m_FrameNumber == 0 ? Span<const u32>({0, 1, 2, 3, 4, 5}) : Span<const u32>({faceIndex})
    });

    const bool environmentCaptureIsComplete = faceIndex == 5;
    if (environmentCaptureIsComplete)
    {
        auto& mipmapped = Passes::Mipmap::addToGraph("AtmosphereEnvironment.Mipmaps"_hsv, *m_Graph,
            cloudsEnvironment.AtmosphereWithCloudsEnvironment);
        cloudsEnvironment.AtmosphereWithCloudsEnvironment = mipmapped.Texture;
        
        m_SkyIrradianceSHResource = Passes::DiffuseIrradianceSH::addToGraph("Sky.DiffuseIrradianceSH"_hsv, *m_Graph,
            cloudsEnvironment.AtmosphereWithCloudsEnvironment, m_SkyIrradianceSHResource, true).DiffuseIrradiance;

        m_SkyPrefilterMapResource = Passes::EnvironmentPrefilter::addToGraph(
            "Sky.EnvironmentPrefilter"_hsv, *m_Graph, cloudsEnvironment.AtmosphereWithCloudsEnvironment,
            m_SkyPrefilterMapResource, true).PrefilteredTexture;

        Passes::CopyBuffer::addToGraph("CopyPrimaryViewEnv"_hsv,
            *m_Graph, {
                .Source = m_Graph->GetGlobalResources().PrimaryViewInfoResource,
                .Destination = m_Graph->Import("InitialPrimaryView"_hsv,
                    m_Graph->GetGlobalResources().PrimaryViewInfoEnvironmentCaptureBuffer),
                .SizeBytes = sizeof(ViewInfoGPU),
            });
        
        Passes::CopyTexture::addToGraph("CopySkyViewEnvironment"_hsv, *m_Graph,
            {.TextureIn = lut.SkyViewLut, .TextureOut = skyViewLut});
    }
    
    Passes::ImGuiCubeTexture::addToGraph("Atmosphere.Environment.Lut"_hsv, *m_Graph,
        cloudsEnvironment.AtmosphereWithCloudsEnvironment);

    return {
        .AtmosphereWithClouds = cloudsEnvironment.AtmosphereWithCloudsEnvironment,
        .CloudsEnvironment = cloudsEnvironment.CloudEnvironment
    };
}

RG::ImageResource Renderer::RenderGraphAtmosphere(Passes::Atmosphere::LutPasses::PassData& lut,
    RG::ImageResource aerialPerspective, RG::ImageResource color, RG::ImageResource depth, RG::CsmData csmData,
    RG::ImageResource clouds, RG::ImageResource cloudsDepth, RG::ImageResource cloudsEnvironment)
{
    auto& atmosphere = Passes::Atmosphere::Render::addToGraph("AtmosphereRender"_hsv, *m_Graph, {
        .ViewInfo = m_Graph->GetGlobalResources().PrimaryViewInfoResource,
        .SkyViewLut = lut.SkyViewLut,
        .AerialPerspective = aerialPerspective,
        .ColorIn = color,
        .DepthIn = depth,
        .IsPrimaryView = true 
    });

    auto& composed = Passes::Clouds::Compose::addToGraph("CloudsCompose"_hsv, *m_Graph, {
        .ViewInfo = m_Graph->GetGlobalResources().PrimaryViewInfoResource,
        .SceneColor = atmosphere.Color,
        .SceneDepth = depth,
        .CloudColor = clouds,
        .CloudDepth = cloudsDepth
    });
    
    Passes::ImGuiTexture::addToGraph("Atmosphere.Atmosphere"_hsv, *m_Graph, composed.Color);
    Passes::ImGuiTexture3d::addToGraph("Atmosphere.AerialPerspective"_hsv, *m_Graph,
        aerialPerspective, Passes::ChannelComposition::RGBComposition());

    return composed.Color;
}

Renderer::CloudMapsInfo Renderer::RenderGraphGetCloudMaps()
{
    using namespace RG;

    m_CloudParametersResource = Passes::Upload::addToGraph("Upload.CloudParameters"_hsv, *m_Graph, m_CloudParameters);

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

    ImageResource cloudCoverageResource = {};
    ImageResource cloudProfileMapResource = {};

    const bool loadCoverage = (bool)CVars::Get().GetI32CVar("Clouds.LoadCoverage"_hsv, bool(1));
    const bool loadProfile = (bool)CVars::Get().GetI32CVar("Clouds.LoadProfile"_hsv, bool(1));
    if (loadCoverage)
    {
        if (!m_CloudCoverage.HasValue())
            m_CloudCoverage.Load(*m_Graph, *m_ImageAssetManager, "../assets/textures/clouds/coverage.png");
       cloudCoverageResource = m_Graph->ImportPersistent("CloudCoverage.Loaded"_hsv,
           m_CloudCoverage.Update(*m_Graph, *m_ImageAssetManager).Image);
    }
    else
    {
        if (!isCloudCoverageDirty && m_CloudCoverage.HasValue())
        {
            cloudCoverageResource = m_Graph->ImportPersistent("CloudCoverage.ImportPersistent"_hsv,
                m_CloudCoverage.Image);
        }
        else
        {
            const BufferResource coverageParametersBuffer = Passes::Upload::addToGraph(
                "CoverageNoiseParameters"_hsv, *m_Graph, m_CloudCoverageNoiseParameters);
            
            auto& cloudCoverage = Passes::Clouds::VP::Coverage::addToGraph("CoverageMapGen"_hsv, *m_Graph, {
                .CoverageMap = cloudCoverageResource,
                .NoiseParameters = coverageParametersBuffer,
            });
            cloudCoverageResource = cloudCoverage.CoverageMap;
            m_Graph->Export(cloudCoverage.CoverageMap, m_CloudCoverage.Image, Device::DeletionQueue());
        }
    }
    if (loadProfile)
    {
        if (!m_CloudProfileMap.HasValue())
            m_CloudProfileMap.Load(*m_Graph, *m_ImageAssetManager, "../assets/textures/clouds/profile.png");
        cloudProfileMapResource = m_Graph->ImportPersistent("CloudProfileMap.Loaded"_hsv,
            m_CloudProfileMap.Update(*m_Graph, *m_ImageAssetManager).Image);
    }
    else
    {
        if (!isCloudCoverageDirty && m_CloudProfileMap.HasValue())
        {
            cloudProfileMapResource = m_Graph->ImportPersistent("CloudProfileMap.ImportPersistent"_hsv, 
                m_CloudProfileMap.Image);
        }
        else
        {
            auto& cloudProfileMap = Passes::Clouds::VP::ProfileMap::addToGraph("ProfileMapGen"_hsv, *m_Graph, {
                .ProfileMap = cloudProfileMapResource,
            });
            cloudProfileMapResource = cloudProfileMap.ProfileMap;
            m_Graph->Export(cloudProfileMap.ProfileMap, m_CloudProfileMap.Image, Device::DeletionQueue());
        }    
    }
    
    Passes::ImGuiTexture::addToGraph("CloudCoverage.Tex"_hsv, *m_Graph, cloudCoverageResource,
        Passes::ChannelComposition::RComposition());
    Passes::ImGuiTexture::addToGraph("CloudProfileMap.Tex"_hsv, *m_Graph, cloudProfileMapResource);

    bool isShapeDirty = false;
    isShapeDirty |= imguiNoiseParametersControls("Low frequency shape noise", m_CloudShapeLowFrequencyNoiseParameters);
    isShapeDirty |= imguiNoiseParametersControls("High frequency shape noise",
        m_CloudShapeHighFrequencyNoiseParameters);
    if (ImGui::Button("Regenerate"))
        isShapeDirty = true;
    
    ImageResource lowFrequencyNoiseResource = {};
    ImageResource highFrequencyNoiseResource = {};
    if (!isShapeDirty && m_CloudShapeLowFrequency.HasValue() && m_CloudShapeHighFrequency.HasValue())
    {
        lowFrequencyNoiseResource = m_Graph->ImportPersistent("LowFrequency.ImportPersistent"_hsv, 
            m_CloudShapeLowFrequency);
        highFrequencyNoiseResource = m_Graph->ImportPersistent("HighFrequency.ImportPersistent"_hsv, 
            m_CloudShapeHighFrequency);
    }
    else
    {
        const BufferResource lowFrequencyParametersBuffer = Passes::Upload::addToGraph(
            "LowFrequencyNoiseParameters"_hsv, *m_Graph, m_CloudShapeLowFrequencyNoiseParameters);
        const BufferResource highFrequencyParametersBuffer = Passes::Upload::addToGraph(
            "HighFrequencyNoiseParameters"_hsv, *m_Graph, m_CloudShapeHighFrequencyNoiseParameters);
        
        auto& cloudShape = Passes::Clouds::ShapeNoise::addToGraph("CloudShapeNoise"_hsv, *m_Graph, {
            .LowFrequencyTextureSize = 128.0f,
            .HighFrequencyTextureSize = 32.0f,
            .LowFrequencyTexture = lowFrequencyNoiseResource,
            .HighFrequencyTexture = highFrequencyNoiseResource,
            .LowFrequencyNoiseParameters = lowFrequencyParametersBuffer,
            .HighFrequencyNoiseParameters = highFrequencyParametersBuffer
        });
        lowFrequencyNoiseResource = cloudShape.LowFrequencyTexture;
        highFrequencyNoiseResource = cloudShape.HighFrequencyTexture;
        m_Graph->Export(cloudShape.LowFrequencyTexture, m_CloudShapeLowFrequency, Device::DeletionQueue());
        m_Graph->Export(cloudShape.HighFrequencyTexture, m_CloudShapeHighFrequency, Device::DeletionQueue());
    }

    ImageResource curlNoiseResource = {};
    if (m_CloudCurlNoise.HasValue())
    {
        curlNoiseResource = m_Graph->ImportPersistent("CloudsCurlNoise.ImportPersistent"_hsv, m_CloudCurlNoise);
    }
    else
    {
        auto& curlNoise = Passes::Clouds::CurlNoise::addToGraph("CloudsCurlNoise"_hsv, *m_Graph, {
            .CloudCurlNoise = curlNoiseResource
        });
        curlNoiseResource = curlNoise.CloudCurlNoise;
        m_Graph->Export(curlNoise.CloudCurlNoise, m_CloudCurlNoise, Device::DeletionQueue());
    }
    
    Passes::ImGuiTexture3d::addToGraph("LowFreq.Tex"_hsv, *m_Graph, lowFrequencyNoiseResource,
        Passes::ChannelComposition::RComposition());
    Passes::ImGuiTexture3d::addToGraph("HighFreq.Tex"_hsv, *m_Graph, highFrequencyNoiseResource,
        Passes::ChannelComposition::RComposition());
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

Renderer::CloudsInfo Renderer::RenderGraphClouds(const CloudMapsInfo& cloudMaps, RG::ImageResource color,
    RG::ImageResource aerialPerspective, RG::ImageResource minMaxDepth, RG::ImageResource sceneDepth)
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
    ImGui::DragFloat("Curl noise scale multiplier", &m_CloudParameters.CurlNoiseScaleMultiplier, 1e-3f);
    ImGui::DragFloat("Curl noise height", &m_CloudParameters.CurlNoiseHeight, 1e-3f);
    ImGui::DragFloat("Curl noise contribution", &m_CloudParameters.CurlNoiseContribution, 1e-3f);

    ImGui::DragFloat("HG eccentricity", &m_CloudParameters.HGEccentricity, 1e-2f, 0.0f, 1.0f);
    ImGui::DragFloat("HG backward eccentricity", &m_CloudParameters.HGBackwardEccentricity, 1e-2f, -1.0f, 0.0f);
    
    ImGui::Checkbox("Reprojection", &m_CloudsReprojectionEnabled);
    
    ImGui::End();

    const bool renderAtmosphere = CVars::Get().GetI32CVar("Renderer.Atmosphere"_hsv).value_or(false);
    
    auto& clouds = Passes::Clouds::VP::addToGraph("Clouds"_hsv, *m_Graph, {
        .ViewInfo = m_Graph->GetGlobalResources().PrimaryViewInfoResource,
        .CloudCoverage = cloudMaps.Coverage,
        .CloudProfile = cloudMaps.Profile,
        .CloudShapeLowFrequencyMap = cloudMaps.ShapeLowFrequency,
        .CloudShapeHighFrequencyMap = cloudMaps.ShapeHighFrequency, 
        .CloudCurlNoise = cloudMaps.CurlNoise, 
        .DepthIn = sceneDepth,
        .MinMaxDepthIn = minMaxDepth,
        .AerialPerspectiveLut = aerialPerspective,
        .IrradianceSH = renderAtmosphere ? m_SkyIrradianceSHResource :
            m_Graph->ImportPersistent("IrradianceSH"_hsv, m_IrradianceSH),
        .CloudParameters = m_CloudParametersResource,
        .CloudsRenderingMode = m_CloudsReprojectionEnabled ?
            Passes::Clouds::VP::CloudsRenderingMode::Reprojection :
            Passes::Clouds::VP::CloudsRenderingMode::FullResolution,
    });

    Passes::ImGuiTexture::addToGraph("Clouds.Color"_hsv, *m_Graph, clouds.ColorOut);
    Passes::ImGuiTexture::addToGraph("Clouds.Depth"_hsv, *m_Graph, clouds.DepthOut);
    
    if (m_CloudsReprojectionEnabled)
    {
        auto& reprojection = Passes::Clouds::Reproject::addToGraph("Clouds.Reproject"_hsv, *m_Graph, {
            .ViewInfo = m_Graph->GetGlobalResources().PrimaryViewInfoResource,
            .Color = clouds.ColorOut,
            .Depth = clouds.DepthOut,
            .SceneDepth = sceneDepth,
            .ColorAccumulationIn = m_CloudColorAccumulation[m_CloudsAccumulationIndexPrev].HasValue() ?
                m_Graph->ImportPersistent("Clouds.Color.Accumulation.In"_hsv,
                    m_CloudColorAccumulation[m_CloudsAccumulationIndexPrev]) :
                RG::ImageResource{},
            .DepthAccumulationIn = m_CloudDepthAccumulation[m_CloudsAccumulationIndexPrev].HasValue() ?
                m_Graph->ImportPersistent("Clouds.Depth.Accumulation.In"_hsv,
                    m_CloudDepthAccumulation[m_CloudsAccumulationIndexPrev]) :
                RG::ImageResource{},
            .ReprojectionFactorIn = m_CloudReprojectionFactor[m_CloudsAccumulationIndexPrev].HasValue() ?
                m_Graph->ImportPersistent("Clouds.ReprojectionFactor.In"_hsv,
                    m_CloudReprojectionFactor[m_CloudsAccumulationIndexPrev]) :
                RG::ImageResource{},
            .ColorAccumulationOut = m_CloudColorAccumulation[m_CloudsAccumulationIndexPrev].HasValue() ?
                m_Graph->ImportPersistent("Clouds.Color.Accumulation"_hsv,
                    m_CloudColorAccumulation[m_CloudsAccumulationIndex]) :
                RG::ImageResource{},
            .DepthAccumulationOut = m_CloudDepthAccumulation[m_CloudsAccumulationIndexPrev].HasValue() ?
                m_Graph->ImportPersistent("Clouds.Depth.Accumulation"_hsv,
                    m_CloudDepthAccumulation[m_CloudsAccumulationIndex]) :
                RG::ImageResource{},
            .ReprojectionFactorOut = m_CloudReprojectionFactor[m_CloudsAccumulationIndexPrev].HasValue() ?
                m_Graph->ImportPersistent("Clouds.ReprojectionFactor"_hsv,
                    m_CloudReprojectionFactor[m_CloudsAccumulationIndex]) :
                RG::ImageResource{},
            .CloudParameters = m_CloudParametersResource,
        });

        Passes::ImGuiTexture::addToGraph("Clouds.Reprojection.Color"_hsv, *m_Graph, reprojection.ColorAccumulation);
        Passes::ImGuiTexture::addToGraph("Clouds.Reprojection.Depth"_hsv, *m_Graph, reprojection.DepthAccumulation);
        Passes::ImGuiTexture::addToGraph("Clouds.Reprojection.Factor"_hsv, *m_Graph, reprojection.ReprojectionFactor);

        return {
            .ColorPrevious = reprojection.ColorAccumulationPrevious,
            .DepthPrevious = reprojection.DepthAccumulationPrevious,
            .ReprojectionPrevious = reprojection.ReprojectionFactorPrevious,
            .Color = reprojection.ColorAccumulation,
            .Depth = reprojection.DepthAccumulation,
            .Reprojection = reprojection.ReprojectionFactor
        };
    }
    
    return {
        .Color = clouds.ColorOut,
        .Depth = clouds.DepthOut
    };
}

Renderer::CloudShadowInfo Renderer::RenderGraphCloudShadows(const CloudMapsInfo& cloudMaps)
{
    auto& cloudShadow = Passes::Clouds::VP::Shadow::addToGraph("CloudsShadow"_hsv, *m_Graph, {
        .PrimaryCamera = m_Graph->GetGlobalResources().PrimaryCamera,
        .PrimaryView = &m_Graph->GetGlobalResources().PrimaryViewInfo,
        .CloudCoverage = cloudMaps.Coverage,
        .CloudProfile = cloudMaps.Profile,
        .CloudShapeLowFrequencyMap = cloudMaps.ShapeLowFrequency,
        .CloudShapeHighFrequencyMap = cloudMaps.ShapeHighFrequency, 
        .CloudCurlNoise = cloudMaps.CurlNoise, 
        .CloudParameters = m_CloudParametersResource,
        .Light = m_SunLight,
    });

    Passes::ImGuiTexture::addToGraph("Clouds.Shadow"_hsv, *m_Graph, cloudShadow.Shadow);

    return {
        .Shadow = cloudShadow.Shadow,
        .View = cloudShadow.ShadowView
    };
}

Renderer* Renderer::Get()
{
    static Renderer renderer = {};
    return &renderer;
}

void Renderer::Run()
{
    while(!m_Window->ShouldClose())
    {
        Input::OnUpdate();
        m_Window->OnUpdate();
        
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

namespace 
{
lux::SceneInstanceHandle spawnRandomScene(Scene& scene, const std::vector<lux::SceneHandle>& scenes,
    const Camera& camera)
{
    if (scenes.empty())
        return {};
    
    u32 sceneIndex = Random::UInt32(0u, (u32)scenes.size() - 1);
    lux::SceneHandle sceneHandle = scenes[sceneIndex];
    
    glm::vec3 position = camera.GetPosition() +
        camera.GetForward() * 7.0f +
        camera.GetRight() * Random::Float(-2.0f, 2.0f) +
        camera.GetUp() * Random::Float(-2.0f, 2.0f);
    
    return scene.Instantiate(sceneHandle, {
        .Transform = {
            .Position = position,
            .Orientation = glm::angleAxis(
                Random::Float(0.0f, (f32)std::numbers::pi), glm::normalize(Random::Float3(0.0f, 1.0f))),
            .Scale = glm::vec3{0.5f},
        }
    });
}
}

void Renderer::OnUpdate()
{
    CPU_PROFILE_FRAME("On update")

    m_CameraController->OnUpdate(1.0f / 60.0f);

    LightFrustumCuller::CullDepthSort(m_Scene->Lights(), *GetFrameContext().PrimaryCamera);
    m_Scene->OnUpdate(GetFrameContext());
    m_OpaqueSet.OnUpdate(GetFrameContext());

    m_ShadowMultiviewVisibility.OnUpdate(GetFrameContext());
    m_PrimaryVisibility.OnUpdate(GetFrameContext());

    struct InstanceWithLife
    {
        lux::SceneInstanceHandle Instance;
        i32 LifeTimeMs{2000000};
    };
    static std::vector<InstanceWithLife> instances;

    if (Input::IsKeyJustPressed(Key::R))
    {
        instances.push_back({spawnRandomScene(*m_Scene, m_Scenes, *m_Camera)});
        LUX_LOG_TRACE("Meshes: {}\tMeshlets: {}\tTriangles: {}",
            m_OpaqueSet.RenderObjectCount(), m_OpaqueSet.MeshletCount(), m_OpaqueSet.TriangleCount());
    }
    if (Input::IsKeyJustPressed(Key::T) && !instances.empty())
    {
        const u32 index = Random::UInt32(0, (u32)instances.size() - 1);
        m_Scene->Delete(instances[index].Instance);
        std::swap(instances[index], instances.back());
        instances.pop_back();
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
    const FrameSync& frameSync = GetFrameContext().FrameSync;
    m_SwapchainImageIndex = Device::AcquireNextImage(m_Swapchain, frameSync.RenderFence, frameSync.PresentSemaphore);
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
                .SourceStage = PipelineStage::Host,
                .DestinationStage = PipelineStage::AllCommands,
                .SourceAccess = PipelineAccess::WriteAll | PipelineAccess::WriteHost,
                .DestinationAccess = PipelineAccess::ReadAll
            }
        },
        GetFrameContext().DeletionQueue)
    });
    
    m_ResourceUploader.BeginFrame(GetFrameContext());
    m_Graph->OnFrameBegin(GetFrameContext());
    m_ShaderAssetManager->OnFrameBegin(GetFrameContext());
    m_ImageAssetManager->OnFrameBegin(GetFrameContext());
    m_SceneAssetManager->OnFrameBegin(GetFrameContext());
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
                .OnStore = AttachmentStore::Store
            },
            .Image = swapchain.DrawImage,
            .Layout = ImageLayout::ColorAttachment
        }, GetFrameContext().DeletionQueue)}
    }, GetFrameContext().DeletionQueue);
}

void Renderer::EndFrame()
{
    ImGuiUI::EndFrame(GetFrameContext().CommandList, GetImGuiUIRenderingInfo());
    /* transition swapchain draw image to the layout swapchain expects */
    GetFrameContext().CommandList.WaitOnBarrier({
        .DependencyInfo = Device::CreateDependencyInfo({
            .LayoutTransitionInfo = LayoutTransitionInfo{
                .ImageSubresource = {
                    .Image = Device::GetSwapchainDescription(m_Swapchain).DrawImage,
                    .Description = {.Mipmaps = 1, .Layers = 1}
                },
                .SourceStage = PipelineStage::ColorOutput,
                .DestinationStage = PipelineStage::Bottom,
                .SourceAccess = PipelineAccess::WriteColorAttachment,
                .DestinationAccess = PipelineAccess::None,
                .OldLayout = ImageLayout::ColorAttachment,
                .NewLayout = ImageLayout::Source
            }
        }, GetFrameContext().DeletionQueue)
    });

    CommandBuffer cmd = GetFrameContext().Cmd;
    GetFrameContext().CommandList.PrepareSwapchainPresent({
        .Swapchain = m_Swapchain,
        .ImageIndex = m_SwapchainImageIndex
    });
    
    GPU_COLLECT_PROFILE_FRAMES()
    
    m_ResourceUploader.SubmitUpload(GetFrameContext());

    Device::EndCommandBuffer(cmd);
    
    Device::SubmitCommandBuffer(cmd, QueueKind::Graphics, BufferSubmitSyncInfo{
        .WaitStages = {PipelineStage::ColorOutput},
        .WaitSemaphores = {GetFrameContext().FrameSync.PresentSemaphore},
        .SignalSemaphores = {Device::GetSwapchainRenderSemaphore(m_Swapchain, m_SwapchainImageIndex)},
        .Fence = GetFrameContext().FrameSync.RenderFence});
    
    bool isFramePresentSuccessful = Device::Present(m_Swapchain, QueueKind::Presentation, m_SwapchainImageIndex); 
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
    m_Window = std::make_unique<lux::Window>(lux::WindowParameters{
        .Name = "Renderer",
        .Size = {.Width = 1600, .Height = 900},
        .UserPointer = this,
        .InputEventFn = [](void* renderer, const lux::InputEvent& event) {
            const auto thisRenderer = (Renderer*)renderer;
            thisRenderer->OnInputEvent(event);
        }
    });

    static constexpr bool ASYNC_COMPUTE = true;
    Device::Init(DeviceCreateInfo::Default(m_Window.get(), ASYNC_COMPUTE));
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

        m_FrameContexts[i].FrameNumber = i;
        m_FrameContexts[i].FrameSync = {
            .RenderFence = Device::CreateFence({.IsSignaled = true}),
            .PresentSemaphore = Device::CreateSemaphore(),
        };
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
    
    m_ResourceUploader.Shutdown();
    m_ShaderAssetManager->Shutdown();
    m_ImageAssetManager->Shutdown();

    m_AssetSystem.Shutdown();
    
    for (auto& ctx : m_FrameContexts)
        ctx.DeletionQueue.Flush();
    ProfilerContext::Get()->Shutdown();

    Device::Shutdown();
    m_Window.reset();
}

void Renderer::OnWindowResize()
{
    m_IsWindowResized = true;
}

void Renderer::RecreateSwapchain()
{
    m_IsWindowResized = false;
    lux::WindowSize windowSize = m_Window->GetWindowSize();
    while (windowSize.Width == 0 || windowSize.Height == 0)
    {
        m_Window->WaitAnyEvent();
        windowSize = m_Window->GetWindowSize();
    }
    
    Device::WaitIdle();

    Device::Destroy(m_Swapchain);
    m_Swapchain = Device::CreateSwapchain({}, Device::DummyDeletionQueue());

    const SwapchainDescription& swapchain = Device::GetSwapchainDescription(m_Swapchain);
    m_Graph->SetBackbufferImage(swapchain.DrawImage);

    Input::OnWindowResized(swapchain.SwapchainResolution.x, swapchain.SwapchainResolution.y);
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
