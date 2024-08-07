#include "Renderer.h"

#include <tracy/Tracy.hpp>

#include "CameraGPU.h"
#include "Model.h"
#include "ShadingSettingsGPU.h"
#include "Core/Input.h"

#include "GLFW/glfw3.h"
#include "Imgui/ImguiUI.h"
#include "Vulkan/RenderCommand.h"
#include "Scene/ModelCollection.h"
#include "Scene/SceneGeometry.h"
#include "RenderGraph/Passes/AO/SsaoBlurPass.h"
#include "RenderGraph/Passes/AO/SsaoPass.h"
#include "RenderGraph/Passes/AO/SsaoVisualizePass.h"
#include "RenderGraph/Passes/Extra/SlimeMold/SlimeMoldPass.h"
#include "RenderGraph/Passes/General/VisibilityPass.h"
#include "RenderGraph/Passes/HiZ/HiZVisualize.h"
#include "RenderGraph/Passes/PBR/PbrVisibilityBufferIBLPass.h"
#include "RenderGraph/Passes/PBR/Translucency/PbrForwardTranslucentIBLPass.h"
#include "RenderGraph/Passes/PostProcessing/CRT/CrtPass.h"
#include "RenderGraph/Passes/Shadows/CSMVisualizePass.h"
#include "RenderGraph/Passes/Shadows/ShadowPassesCommon.h"
#include "RenderGraph/Passes/Skybox/SkyboxPass.h"
#include "RenderGraph/Passes/Utility/CopyTexturePass.h"
#include "RenderGraph/Passes/Utility/ImGuiTexturePass.h"
#include "Rendering/ShaderCache.h"
#include "Scene/Sorting/DepthGeometrySorter.h"
#include "Rendering/Image/Processing/BRDFProcessor.h"
#include "Rendering/Image/Processing/CubemapProcessor.h"
#include "Rendering/Image/Processing/DiffuseIrradianceProcessor.h"
#include "Rendering/Image/Processing/EnvironmentPrefilterProcessor.h"

Renderer::Renderer() = default;

void Renderer::Init()
{
    ShaderCache::Init();
    
    InitRenderingStructures();

    Input::s_MainViewportSize = m_Swapchain.GetResolution();
    m_Camera = std::make_shared<Camera>(CameraType::Perspective);
    m_CameraController = std::make_unique<CameraController>(m_Camera);
    for (auto& ctx : m_FrameContexts)
        ctx.MainCamera = m_Camera.get();

    m_Graph = std::make_unique<RG::Graph>();

    InitRenderGraph();
}

void Renderer::InitRenderGraph()
{
    Model* helmet = Model::LoadFromAsset("../assets/models/flight_helmet/flightHelmet.model");
    Model* brokenHelmet = Model::LoadFromAsset("../assets/models/broken_helmet/scene.model");
    Model* car = Model::LoadFromAsset("../assets/models/shadow/scene.model");
    Model* plane = Model::LoadFromAsset("../assets/models/plane/scene.model");
    m_GraphModelCollection.CreateDefaultTextures();
    m_GraphModelCollection.RegisterModel(helmet, "helmet");
    m_GraphModelCollection.RegisterModel(brokenHelmet, "broken helmet");
    m_GraphModelCollection.RegisterModel(car, "car");
    m_GraphModelCollection.RegisterModel(plane, "plane");

    m_GraphModelCollection.AddModelInstance("car", {
        .Transform = {
            .Position = glm::vec3{0.0f, 0.0f, 0.0f},
            .Scale = glm::vec3{1.0f}}});
    
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
    
    m_SkyboxTexture = Texture::Builder({.Usage = ImageUsage::Sampled | ImageUsage::Storage})
        .FromEquirectangular("../assets/textures/forest.tx")
        .Build();
    m_SkyboxIrradianceMap = DiffuseIrradianceProcessor::CreateEmptyTexture();
    m_SkyboxPrefilterMap = EnvironmentPrefilterProcessor::CreateEmptyTexture();
    DiffuseIrradianceProcessor::Add(m_SkyboxTexture, m_SkyboxIrradianceMap);
    EnvironmentPrefilterProcessor::Add(m_SkyboxTexture, m_SkyboxPrefilterMap);

    m_Graph->SetBackbuffer(m_Swapchain.GetDrawImage());

    auto drawTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
        "../assets/shaders/processed/render-graph/general/draw-indirect-culled-vert.stage",
        "../assets/shaders/processed/render-graph/general/draw-indirect-culled-frag.stage",},
        "Pass.DrawCulled", m_Graph->GetArenaAllocators());
    
    ShaderDescriptors materialDescriptors = ShaderDescriptors::Builder()
            .SetTemplate(drawTemplate, DescriptorAllocatorKind::Resources)
            // todo: make this (2) an enum
            .ExtractSet(2)
            .BindlessCount(1024)
            .Build();
    materialDescriptors.UpdateGlobalBinding(UNIFORM_MATERIALS, m_GraphOpaqueGeometry.GetMaterialsBuffer().BindingInfo());
    m_GraphModelCollection.ApplyMaterialTextures(materialDescriptors);

    ShaderCache::SetAllocators(m_Graph->GetArenaAllocators());
    ShaderCache::AddBindlessDescriptors("main_materials", materialDescriptors);
    
    // model collection might not have any translucent objects
    if (m_GraphTranslucentGeometry.IsValid())
    {
        /* todo:
         * this is actually unnecessary, there are at least two more options:
         * - have both `u_materials` and `u_translucent_materials` (not so bad)
         * - have two different descriptors: set (2) for materials and set (3) for bindless textures
         */
        ShaderDescriptors translucentMaterialDescriptors = ShaderDescriptors::Builder()
                .SetTemplate(drawTemplate, DescriptorAllocatorKind::Resources)
                // todo: make this (2) an enum
                .ExtractSet(2)
                .BindlessCount(1024)
                .Build();
        translucentMaterialDescriptors.UpdateGlobalBinding(UNIFORM_MATERIALS,
            m_GraphTranslucentGeometry.GetMaterialsBuffer().BindingInfo());
        m_GraphModelCollection.ApplyMaterialTextures(translucentMaterialDescriptors);
    }
    
    // todo: separate geometry for shadow casters

    m_SlimeMoldContext = std::make_shared<SlimeMoldContext>(
        SlimeMoldContext::RandomIn(m_Swapchain.GetResolution(), 1, 5000000, *GetFrameContext().ResourceUploader));
    m_SlimeMoldPass = std::make_shared<SlimeMoldPass>(*m_Graph);
}

void Renderer::SetupRenderSlimePasses()
{
    m_SlimeMoldPass->AddToGraph(*m_Graph, SlimeMoldPassStage::UpdateSlimeMap, *m_SlimeMoldContext);
    m_SlimeMoldPass->AddToGraph(*m_Graph, SlimeMoldPassStage::DiffuseSlimeMap, *m_SlimeMoldContext);
    m_SlimeMoldPass->AddToGraph(*m_Graph, SlimeMoldPassStage::Gradient, *m_SlimeMoldContext);
    auto& slimeMoldOutput = m_Graph->GetBlackboard().Get<SlimeMoldPass::GradientPassData>();
    Passes::CopyTexture::addToGraph("CopyTexture.Mold", *m_Graph,
        slimeMoldOutput.GradientMap, m_Graph->GetBackbuffer(),
        glm::vec3{0.0f}, glm::vec3{1.0f});
    m_SlimeMoldPass->AddToGraph(*m_Graph, SlimeMoldPassStage::CopyDiffuse, *m_SlimeMoldContext);
}

void Renderer::SetupRenderGraph()
{
    using namespace RG;
    
    m_Graph->Reset(GetFrameContext());
    Resource backbuffer = m_Graph->GetBackbuffer();

    // todo: move to proper place (this is just testing atm)
    if (m_GraphTranslucentGeometry.IsValid())
    {
        DepthGeometrySorter translucentSorter(m_Camera->GetPosition(), m_Camera->GetForward());
        translucentSorter.Sort(m_GraphTranslucentGeometry, *GetFrameContext().ResourceUploader);
    }

    // update camera
    CameraGPU cameraGPU = CameraGPU::FromCamera(*m_Camera, m_Swapchain.GetResolution());
    static ShadingSettingsGPU shadingSettingsGPU = {
        .EnvironmentPower = 1.0f,
        .SoftShadows = false};
    ImGui::Begin("Shading Settings");
    ImGui::DragFloat("Environment power", &shadingSettingsGPU.EnvironmentPower, 1e-2f, 0.0f, 1.0f);
    ImGui::Checkbox("Soft shadows", (bool*)&shadingSettingsGPU.SoftShadows);
    ImGui::End();

    // todo: should not create and delete every frame
    Buffer mainCameraBuffer = Buffer::Builder({
            .SizeBytes = sizeof(CameraGPU),
            .Usage = BufferUsage::Uniform | BufferUsage::Upload | BufferUsage::DeviceAddress})
        .Build(GetFrameContext().DeletionQueue);
    mainCameraBuffer.SetData(&cameraGPU, sizeof(CameraGPU));
    Buffer shadingSettingsBuffer = Buffer::Builder({
            .SizeBytes = sizeof(ShadingSettingsGPU),
            .Usage = BufferUsage::Uniform | BufferUsage::Upload | BufferUsage::DeviceAddress})
        .Build(GetFrameContext().DeletionQueue);
    shadingSettingsBuffer.SetData(&shadingSettingsGPU, sizeof(ShadingSettingsGPU));
    
    GlobalResources globalResources = {
        .MainCameraGPU = m_Graph->AddExternal("MainCamera", mainCameraBuffer),
        .ShadingSettings = m_Graph->AddExternal("ShadingSettings", shadingSettingsBuffer)};
    m_Graph->GetBlackboard().Update(globalResources);

    auto& visibility = Passes::Draw::Visibility::addToGraph("Visibility", *m_Graph, {
        .Geometry = &m_GraphOpaqueGeometry,
        .Resolution = m_Swapchain.GetResolution(),
        .Camera = GetFrameContext().MainCamera});
    auto& visibilityOutput = m_Graph->GetBlackboard().Get<Passes::Draw::Visibility::PassData>(visibility);

    auto& ssao = Passes::Ssao::addToGraph("SSAO", 32, *m_Graph, visibilityOutput.DepthOut);
    auto& ssaoOutput = m_Graph->GetBlackboard().Get<Passes::Ssao::PassData>(ssao);

    auto& ssaoBlurHorizontal = Passes::SsaoBlur::addToGraph("SSAO.Blur.Horizontal", *m_Graph,
        ssaoOutput.SSAO, {},
        SsaoBlurPassKind::Horizontal);
    auto& ssaoBlurHorizontalOutput = m_Graph->GetBlackboard().Get<Passes::SsaoBlur::PassData>(ssaoBlurHorizontal);
    auto& ssaoBlurVertical = Passes::SsaoBlur::addToGraph("SSAO.Blur.Vertical", *m_Graph,
        ssaoBlurHorizontalOutput.SsaoOut, ssaoOutput.SSAO,
        SsaoBlurPassKind::Vertical);
    auto& ssaoBlurVerticalOutput = m_Graph->GetBlackboard().Get<Passes::SsaoBlur::PassData>(ssaoBlurVertical);

    auto& ssaoVisualize = Passes::SsaoVisualize::addToGraph("SSAO.Visualize", *m_Graph,
        ssaoBlurVerticalOutput.SsaoOut, {});
    auto& ssaoVisualizeOutput = m_Graph->GetBlackboard().Get<Passes::SsaoVisualize::PassData>(ssaoVisualize);

    // todo: should not be here obv
    DirectionalLight directionalLight = m_SceneLights.GetDirectionalLight();
    ImGui::Begin("Directional Light");
    ImGui::DragFloat3("Direction", &directionalLight.Direction[0], 1e-2f, -1.0f, 1.0f);
    ImGui::ColorPicker3("Color", &directionalLight.Color[0]);
    ImGui::DragFloat("Intensity", &directionalLight.Intensity, 1e-1f, 0.0f, 100.0f);
    ImGui::DragFloat("Size", &directionalLight.Size, 1e-1f, 0.0f, 100.0f);
    ImGui::End();
    directionalLight.Direction = glm::normalize(directionalLight.Direction);

    m_SceneLights.SetDirectionalLight(directionalLight);

    static f32 shadowDistance = 200.0f;
    ImGui::DragFloat("Shadow distance", &shadowDistance, 1e-1f, 0.0f, 400.0f);
    auto& csm = Passes::CSM::addToGraph("CSM", *m_Graph, {
        .Geometry = &m_GraphOpaqueGeometry,
        .MainCamera = m_Camera.get(),
        .DirectionalLight = &m_SceneLights.GetDirectionalLight(),
        .ViewDistance = shadowDistance,
        .GeometryBounds = m_GraphOpaqueGeometry.GetBounds()});
    auto& csmOutput = m_Graph->GetBlackboard().Get<Passes::CSM::PassData>(csm);

    auto& pbr = Passes::Pbr::VisibilityIbl::addToGraph("Pbr.Visibility.Ibl", *m_Graph, {
        .VisibilityTexture = visibilityOutput.ColorOut,
        .ColorIn = {},
        .SceneLights = &m_SceneLights,
        .IBL = {
            .Irradiance = m_Graph->AddExternal("IrradianceMap", m_SkyboxIrradianceMap),
            .PrefilterEnvironment = m_Graph->AddExternal("PrefilterMap", m_SkyboxPrefilterMap),
            .BRDF = m_Graph->AddExternal("BRDF", *m_BRDF)},
        .SSAO = {
            .SSAO = ssaoBlurVerticalOutput.SsaoOut},
        .CSMData = {
            .ShadowMap = csmOutput.ShadowMap,
            .CSM = csmOutput.CSM},
        .Geometry = &m_GraphOpaqueGeometry});
    auto& pbrOutput = m_Graph->GetBlackboard().Get<Passes::Pbr::VisibilityIbl::PassData>(pbr);

    auto& skybox = Passes::Skybox::addToGraph("Skybox", *m_Graph,
        m_SkyboxPrefilterMap, pbrOutput.ColorOut, visibilityOutput.DepthOut, GetFrameContext().Resolution, 1.2f);
    auto& skyboxOutput = m_Graph->GetBlackboard().Get<Passes::Skybox::PassData>(skybox);
    Resource renderedColor = skyboxOutput.ColorOut;
    Resource renderedDepth = skyboxOutput.DepthOut;
    
    // model collection might not have any translucent objects
    if (m_GraphTranslucentGeometry.IsValid())
    {
        /*Passes::Pbr::ForwardTranslucentIbl::addToGraph("Pbr.Translucent.Ibl", *m_Graph, {
            .Geometry = m_GraphTranslucentGeometry,
            .Resolution = m_Swapchain.GetResolution(),
            .Camera = GetFrameContext().MainCamera,
            .ColorIn = renderedColor,
            .DepthIn = renderedDepth,
            .SceneLights = &m_SceneLights,
            .IBL = {
                 .Irradiance = m_Graph->AddExternal("IrradianceMap", m_SkyboxIrradianceMap),
                 .PrefilterEnvironment = m_Graph->AddExternal("PrefilterMap", m_SkyboxPrefilterMap),
                 .BRDF = m_Graph->AddExternal("BRDF", *m_BRDF)},
            .HiZContext = m_VisibilityPass->GetHiZContext()})
        auto& pbrTranslucentOutput = m_Graph->GetBlackboard().Get<PbrForwardTranslucentIBLPass::PassData>();
        
        renderedColor = pbrTranslucentOutput.ColorOut;*/
    }

    auto& copyRendered = Passes::CopyTexture::addToGraph("CopyRendered", *m_Graph,
        renderedColor, backbuffer, glm::vec3{}, glm::vec3{1.0f});
    backbuffer = m_Graph->GetBlackboard().Get<Passes::CopyTexture::PassData>(copyRendered).TextureOut;

    auto& hizVisualize = Passes::HiZVisualize::addToGraph("HiZ.Visualize", *m_Graph, visibilityOutput.HiZOut);
    auto& hizVisualizePassOutput = m_Graph->GetBlackboard().Get<Passes::HiZVisualize::PassData>(hizVisualize);

    auto& csmVisualize = Passes::VisualizeCSM::addToGraph("CSM.Visualize", *m_Graph, csmOutput, {});
    auto& visualizeCSMPassOutput = m_Graph->GetBlackboard().Get<Passes::VisualizeCSM::PassData>(csmVisualize);

    Passes::ImGuiTexture::addToGraph("SSAO.Texture", *m_Graph, ssaoVisualizeOutput.ColorOut);
    Passes::ImGuiTexture::addToGraph("Visibility.HiZ.Texture", *m_Graph, hizVisualizePassOutput.ColorOut);
    Passes::ImGuiTexture::addToGraph("CSM.Texture", *m_Graph, visualizeCSMPassOutput.ColorOut);
    Passes::ImGuiTexture::addToGraph("BRDF.Texture", *m_Graph, *m_BRDF);

    //SetupRenderSlimePasses();
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

        // todo: move to OnUpdate
        m_CameraController->OnUpdate(1.0f / 60.0f);
        m_SceneLights.UpdateBuffers(*GetFrameContext().ResourceUploader);    

        
        OnRender();
    }
}

void Renderer::OnRender()
{
    CPU_PROFILE_FRAME("On render")

    BeginFrame();
    ShaderCache::OnFrameBegin(GetFrameContext());
    ImGuiUI::BeginFrame(GetFrameContext().FrameNumber);
    ProcessPendingCubemaps();
    ProcessPendingPBRTextures();

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
        
        ImGuiUI::EndFrame(GetFrameContext().Cmd, GetImGuiUIRenderingInfo());
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

    u32 frameNumber = GetFrameContext().FrameNumber;
    m_SwapchainImageIndex = m_Swapchain.AcquireImage(frameNumber);
    if (m_SwapchainImageIndex == INVALID_SWAPCHAIN_IMAGE)
    {
        m_FrameEarlyExit = true;
        RecreateSwapchain();
        return;
    }
    
    m_CurrentFrameContext->DeletionQueue.Flush();

    CommandBuffer& cmd = GetFrameContext().Cmd;
    cmd.Reset();
    cmd.Begin();
}

RenderingInfo Renderer::GetImGuiUIRenderingInfo()
{
    RenderingAttachment color = RenderingAttachment::Builder()
      .SetType(RenderingAttachmentType::Color)
      .FromImage(m_Swapchain.GetDrawImage(), ImageLayout::General)
      .LoadStoreOperations(AttachmentLoad::Load, AttachmentStore::Store)
      .Build(GetFrameContext().DeletionQueue);

    RenderingInfo info = RenderingInfo::Builder()
        .AddAttachment(color)
        .SetResolution(m_Swapchain.GetResolution())
        .Build(GetFrameContext().DeletionQueue);

    return info;
}

void Renderer::ProcessPendingCubemaps()
{
    CPU_PROFILE_FRAME("ProcessPendingCubemaps")

    if (CubemapProcessor::HasPending())
        CubemapProcessor::Process(GetFrameContext().Cmd);
}

void Renderer::ProcessPendingPBRTextures()
{
    CPU_PROFILE_FRAME("ProcessPendingPBRTextures")

    if (DiffuseIrradianceProcessor::HasPending())
        DiffuseIrradianceProcessor::Process(GetFrameContext().Cmd);
    if (EnvironmentPrefilterProcessor::HasPending())
        EnvironmentPrefilterProcessor::Process(GetFrameContext().Cmd);
    if (!m_BRDF)
        m_BRDF = std::make_shared<Texture>(BRDFProcessor::CreateBRDF(GetFrameContext().Cmd));
}

void Renderer::EndFrame()
{
    CommandBuffer& cmd = GetFrameContext().Cmd;
    m_Swapchain.PreparePresent(cmd, m_SwapchainImageIndex);
    
    u32 frameNumber = GetFrameContext().FrameNumber;
    SwapchainFrameSync& sync = GetFrameContext().FrameSync;

    TracyVkCollect(ProfilerContext::Get()->GraphicsContext(), Driver::GetProfilerCommandBuffer(ProfilerContext::Get()))

    m_ResourceUploader.SubmitUpload(cmd);
    
    cmd.End();

    cmd.Submit(m_Device.GetQueues().Graphics,
        BufferSubmitSyncInfo{
            .WaitStages = {PipelineStage::ColorOutput},
            .WaitSemaphores = {&sync.PresentSemaphore},
            .SignalSemaphores = {&sync.RenderSemaphore},
            .Fence = &sync.RenderFence});
    
    bool isFramePresentSuccessful = m_Swapchain.PresentImage(m_Device.GetQueues().Presentation, m_SwapchainImageIndex,
        frameNumber); 
    bool shouldRecreateSwapchain = m_IsWindowResized || !isFramePresentSuccessful;
    if (shouldRecreateSwapchain)
        RecreateSwapchain();
    
    m_FrameNumber++;
    m_CurrentFrameContext = &m_FrameContexts[(u32)m_FrameNumber % BUFFERED_FRAMES];
    m_CurrentFrameContext->FrameNumberTick = m_FrameNumber;
    
    ProfilerContext::Get()->NextFrame();

    m_ResourceUploader.StartRecording();
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

    m_Device = Device::Builder()
        .Defaults()
        .SetWindow(m_Window)
        .AsyncCompute()
        .Build();

    Driver::Init(m_Device);
    ImGuiUI::Init(m_Window);
    ImageUtils::DefaultTextures::Init();

    m_ResourceUploader.Init();
    
    m_Swapchain = Swapchain::Builder()
        .SetDevice(m_Device)
        .BufferedFrames(BUFFERED_FRAMES)
        .BuildManualLifetime();

    m_FrameContexts.resize(BUFFERED_FRAMES);
    for (u32 i = 0; i < BUFFERED_FRAMES; i++)
    {
        CommandPool pool = CommandPool::Builder()
            .SetQueue(QueueKind::Graphics)
            .PerBufferReset(true)
            .Build();

        m_FrameContexts[i].FrameSync = m_Swapchain.GetFrameSync(i);
        m_FrameContexts[i].FrameNumber = i;
        m_FrameContexts[i].Resolution = m_Swapchain.GetResolution();

        m_FrameContexts[i].Cmd = pool.AllocateBuffer(CommandBufferKind::Primary);
        m_FrameContexts[i].ResourceUploader = &m_ResourceUploader;
    }

    std::array<CommandBuffer*, BUFFERED_FRAMES> cmds;
    for (u32 i = 0; i < BUFFERED_FRAMES; i++)
        cmds[i] = &m_FrameContexts[i].Cmd;
    ProfilerContext::Get()->Init(cmds);

    m_CurrentFrameContext = &m_FrameContexts.front();
}

void Renderer::Shutdown()
{
    m_Device.WaitIdle();

    Swapchain::DestroyImages(m_Swapchain);
    Swapchain::Destroy(m_Swapchain);

    m_Graph.reset();
    m_ResourceUploader.Shutdown();
    ShaderCache::Shutdown();
    for (auto& ctx : m_FrameContexts)
        ctx.DeletionQueue.Flush();
    ProfilerContext::Get()->Shutdown();

    ImGuiUI::Shutdown();
    Driver::Shutdown();
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
    
    m_Device.WaitIdle();
    
    Swapchain::Builder newSwapchainBuilder = Swapchain::Builder()
        .SetDevice(m_Device)
        .BufferedFrames(BUFFERED_FRAMES)
        .SetSyncStructures(m_Swapchain.GetFrameSync());

    Swapchain::DestroyImages(m_Swapchain);
    Swapchain::Destroy(m_Swapchain);
    
    m_Swapchain = newSwapchainBuilder.BuildManualLifetime();

    m_Graph->SetBackbuffer(m_Swapchain.GetDrawImage());
    // todo: to multicast delegate
    m_Graph->OnResolutionChange();

    Input::s_MainViewportSize = m_Swapchain.GetResolution();
    m_Camera->SetViewport(m_Swapchain.GetResolution().x, m_Swapchain.GetResolution().y);
    for (auto& frameContext : m_FrameContexts)
        frameContext.Resolution = m_Swapchain.GetResolution();
}

const FrameContext& Renderer::GetFrameContext() const
{
    return *m_CurrentFrameContext;
}

FrameContext& Renderer::GetFrameContext()
{
    return *m_CurrentFrameContext;
}
