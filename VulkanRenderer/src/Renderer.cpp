#include "Renderer.h"

#include <volk.h>
#include <glm/ext/matrix_transform.hpp>
#include <tracy/Tracy.hpp>

#include "CameraGPU.h"
#include "Model.h"
#include "Core/Input.h"
#include "Core/Random.h"

#include "GLFW/glfw3.h"
#include "Imgui/ImguiUI.h"
#include "Vulkan/RenderCommand.h"
#include "RenderGraph/ModelCollection.h"
#include "RenderGraph/RGGeometry.h"
#include "RenderGraph/Passes/AO/SsaoBlurPass.h"
#include "RenderGraph/Passes/AO/SsaoPass.h"
#include "RenderGraph/Passes/AO/SsaoVisualizePass.h"
#include "RenderGraph/Passes/Culling/MeshletCullPass.h"
#include "RenderGraph/Passes/Extra/SlimeMold/SlimeMoldPass.h"
#include "RenderGraph/Passes/General/VisibilityPass.h"
#include "RenderGraph/Passes/HiZ/HiZVisualize.h"
#include "RenderGraph/Passes/PBR/VisualizeBRDFPass.h"
#include "RenderGraph/Passes/PBR/PbrVisibilityBufferIBLPass.h"
#include "RenderGraph/Passes/PBR/Translucency/PbrForwardTranslucentIBLPass.h"
#include "RenderGraph/Passes/PostProcessing/CRT/CrtPass.h"
#include "RenderGraph/Passes/PostProcessing/Sky/SkyGradientPass.h"
#include "RenderGraph/Passes/Skybox/SkyboxPass.h"
#include "RenderGraph/Passes/Utility/BlitPass.h"
#include "RenderGraph/Passes/Utility/CopyTexturePass.h"
#include "RenderGraph/Sorting/RGDepthGeometrySorter.h"
#include "Rendering/Image/Processing/BRDFProcessor.h"
#include "Rendering/Image/Processing/CubemapProcessor.h"
#include "Rendering/Image/Processing/DiffuseIrradianceProcessor.h"
#include "Rendering/Image/Processing/EnvironmentPrefilterProcessor.h"

Renderer::Renderer() = default;

void Renderer::Init()
{
    InitRenderingStructures();

    Input::s_MainViewportSize = m_Swapchain.GetResolution();
    m_Camera = std::make_shared<Camera>();
    m_CameraController = std::make_unique<CameraController>(m_Camera);
    for (auto& ctx : m_FrameContexts)
        ctx.MainCamera = m_Camera.get();

    InitRenderGraph();
}

void Renderer::InitRenderGraph()
{
    Model* helmet = Model::LoadFromAsset("../assets/models/flight_helmet/flightHelmet.model");
    Model* brokenHelmet = Model::LoadFromAsset("../assets/models/broken_helmet/scene.model");
    Model* car = Model::LoadFromAsset("../assets/models/car/scene.model");
    m_GraphModelCollection.CreateDefaultTextures();
    m_GraphModelCollection.RegisterModel(helmet, "helmet");
    m_GraphModelCollection.RegisterModel(brokenHelmet, "broken helmet");
    m_GraphModelCollection.RegisterModel(car, "car");
    m_GraphModelCollection.AddModelInstance("car", {
               .Transform = {
                   .Position = glm::vec3{0.0f, 0.0f, 0.0f},
                   .Scale = glm::vec3{1.0f}}});
    
    m_GraphOpaqueGeometry = RG::Geometry::FromModelCollectionFiltered(m_GraphModelCollection,
        *GetFrameContext().ResourceUploader,
        [this](const Mesh&, const Material& material) {
            return material.Type == assetLib::ModelInfo::MaterialType::Opaque;
        });
    m_GraphTranslucentGeometry = RG::Geometry::FromModelCollectionFiltered(m_GraphModelCollection,
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

    m_Graph = std::make_unique<RG::Graph>();
    m_Graph->SetBackbuffer(m_Swapchain.GetDrawImage());

    auto drawTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
        "../assets/shaders/processed/render-graph/general/draw-indirect-culled-vert.shader",
        "../assets/shaders/processed/render-graph/general/draw-indirect-culled-frag.shader",},
        "Pass.DrawCulled", m_Graph->GetArenaAllocators());
    
    ShaderDescriptors materialDescriptors = ShaderDescriptors::Builder()
            .SetTemplate(drawTemplate, DescriptorAllocatorKind::Resources)
            // todo: make this (2) an enum
            .ExtractSet(2)
            .BindlessCount(1024)
            .Build();
    materialDescriptors.UpdateGlobalBinding("u_materials", m_GraphOpaqueGeometry.GetMaterialsBuffer().BindingInfo());
    m_GraphModelCollection.ApplyMaterialTextures(materialDescriptors);

    m_VisibilityPass = std::make_shared<VisibilityPass>(*m_Graph, VisibilityPassInitInfo{
        .MaterialDescriptors = &materialDescriptors,
        .Geometry = &m_GraphOpaqueGeometry});
    
    m_PbrVisibilityBufferIBLPass = std::make_shared<PbrVisibilityBufferIBL>(*m_Graph, PbrVisibilityBufferInitInfo{
        .MaterialDescriptors = &materialDescriptors});
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
        translucentMaterialDescriptors.UpdateGlobalBinding("u_materials",
            m_GraphTranslucentGeometry.GetMaterialsBuffer().BindingInfo());
        m_GraphModelCollection.ApplyMaterialTextures(translucentMaterialDescriptors);
        
        m_PbrForwardIBLTranslucentPass = std::make_shared<PbrForwardTranslucentIBLPass>(*m_Graph,
            PbrForwardTranslucentIBLPassInitInfo{
                .MaterialDescriptors = &translucentMaterialDescriptors,
                .Geometry = &m_GraphTranslucentGeometry});
    }
    
    m_SsaoPass = std::make_shared<SsaoPass>(*m_Graph, 32);
    m_SsaoBlurHorizontalPass = std::make_shared<SsaoBlurPass>(*m_Graph, SsaoBlurPassKind::Horizontal);
    m_SsaoBlurVerticalPass = std::make_shared<SsaoBlurPass>(*m_Graph, SsaoBlurPassKind::Vertical);
    m_SsaoVisualizePass = std::make_shared<SsaoVisualizePass>(*m_Graph);

    m_VisualizeBRDFPass = std::make_shared<VisualizeBRDFPass>(*m_Graph);

    m_SkyboxPass = std::make_shared<SkyboxPass>(*m_Graph);

    m_SkyGradientPass = std::make_shared<SkyGradientPass>(*m_Graph);
    m_CrtPass = std::make_shared<CrtPass>(*m_Graph);
    m_HiZVisualizePass = std::make_shared<HiZVisualize>(*m_Graph);
    m_BlitPartialDraw = std::make_shared<BlitPass>("Blit.PartialDraw");
    m_BlitHiZ = std::make_shared<BlitPass>("Blit.Hiz");
    m_CopyTexturePass = std::make_shared<CopyTexturePass>("Copy.Texture");

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
    m_CopyTexturePass->AddToGraph(*m_Graph, slimeMoldOutput.GradientMap, m_Graph->GetBackbuffer(),
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
    CameraGPU cameraGPU = {
        .ViewProjection = m_Camera->GetViewProjection(),
        .Projection = m_Camera->GetProjection(),
        .View = m_Camera->GetView(),
        .Position = m_Camera->GetPosition(),
        .Near = m_Camera->GetFrustumPlanes().Near,
        .Forward = m_Camera->GetForward(),
        .Far = m_Camera->GetFrustumPlanes().Far,
        .InverseViewProjection = glm::inverse(m_Camera->GetViewProjection()),
        .InverseProjection = glm::inverse(m_Camera->GetProjection()),
        .InverseView = glm::inverse(m_Camera->GetView()),
        .Resolution = glm::vec2{m_Swapchain.GetResolution()}};

    // todo: should not create and delete every frame
    Buffer mainCameraBuffer = Buffer::Builder({
            .SizeBytes = sizeof(CameraGPU),
            .Usage = BufferUsage::Uniform | BufferUsage::Upload | BufferUsage::DeviceAddress})
        .Build(GetFrameContext().DeletionQueue);
    mainCameraBuffer.SetData(&cameraGPU, sizeof(CameraGPU));
    
    GlobalResources globalResources = {
        .MainCameraGPU = m_Graph->AddExternal("MainCamera", mainCameraBuffer)};
    m_Graph->GetBlackboard().Register(globalResources);

    m_VisibilityPass->AddToGraph(*m_Graph, m_Swapchain.GetResolution());
    auto& visibility = m_Graph->GetBlackboard().Get<VisibilityPass::PassData>();

    m_SsaoPass->AddToGraph(*m_Graph, visibility.DepthOut);
    auto& ssaoOutput = m_Graph->GetBlackboard().Get<SsaoPass::PassData>();

    m_SsaoBlurHorizontalPass->AddToGraph(*m_Graph, ssaoOutput.SSAO, {});
    auto& ssaoBlurHorizontalOutput = m_Graph->GetBlackboard().Get<SsaoBlurPass::PassData>(
        m_SsaoBlurHorizontalPass->GetNameHash());

    m_SsaoBlurVerticalPass->AddToGraph(*m_Graph, ssaoBlurHorizontalOutput.SsaoOut, ssaoOutput.SSAO);
    auto& ssaoBlurVerticalOutput = m_Graph->GetBlackboard().Get<SsaoBlurPass::PassData>(
            m_SsaoBlurVerticalPass->GetNameHash());
        
    //m_SsaoVisualizePass->AddToGraph(*m_Graph, ssaoBlurVerticalOutput.SsaoOut, backbuffer);
    //backbuffer = m_Graph->GetBlackboard().Get<SsaoVisualizePass::PassData>().ColorOut;
    
    m_PbrVisibilityBufferIBLPass->AddToGraph(*m_Graph, {
        .VisibilityTexture = visibility.ColorOut,
        .ColorIn = {},
        .IBL = {
            .Irradiance = m_Graph->AddExternal("IrradianceMap", m_SkyboxIrradianceMap),
            .PrefilterEnvironment = m_Graph->AddExternal("PrefilterMap", m_SkyboxPrefilterMap),
            .BRDF = m_Graph->AddExternal("BRDF", *m_BRDF)},
        .SSAO = {
            .SSAOTexture = ssaoBlurVerticalOutput.SsaoOut},
        .Geometry = &m_GraphOpaqueGeometry});
    auto& pbrOutput = m_Graph->GetBlackboard().Get<PbrVisibilityBufferIBL::PassData>();

    
    m_SkyboxPass->AddToGraph(*m_Graph,
        m_SkyboxPrefilterMap, pbrOutput.ColorOut, visibility.DepthOut, GetFrameContext().Resolution, 1.2f);
    auto& skyboxOutput = m_Graph->GetBlackboard().Get<SkyboxPass::PassData>();
    Resource renderedColor = skyboxOutput.ColorOut;
    Resource renderedDepth = skyboxOutput.DepthOut;
    
    // model collection might not have any translucent objects
    if (m_GraphTranslucentGeometry.IsValid())
    {
        m_PbrForwardIBLTranslucentPass->AddToGraph(*m_Graph, {
            .Resolution = m_Swapchain.GetResolution(),
            .ColorIn = renderedColor,
            .DepthIn = renderedDepth,
            .IBL = {
                 .Irradiance = m_Graph->AddExternal("IrradianceMap", m_SkyboxIrradianceMap),
                 .PrefilterEnvironment = m_Graph->AddExternal("PrefilterMap", m_SkyboxPrefilterMap),
                 .BRDF = m_Graph->AddExternal("BRDF", *m_BRDF)},
            .HiZContext = m_VisibilityPass->GetHiZContext()});
        auto& pbrTranslucentOutput = m_Graph->GetBlackboard().Get<PbrForwardTranslucentIBLPass::PassData>();
        
        renderedColor = pbrTranslucentOutput.ColorOut;
    }

    m_CopyTexturePass->AddToGraph(*m_Graph, renderedColor, backbuffer, glm::vec3{}, glm::vec3{1.0f});
    backbuffer = m_Graph->GetBlackboard().Get<CopyTexturePass::PassData>().TextureOut;
    
    //m_CrtPass->AddToGraph(*m_Graph, skyboxOutput.ColorOut, backbuffer);
    //auto& crtOut = m_Graph->GetBlackboard().Get<CrtPass::PassData>();
    //backbuffer = crtOut.ColorOut;

    //m_VisualizeBRDFPass->AddToGraph(*m_Graph, *m_BRDF, backbuffer, GetFrameContext().Resolution);
    //auto& visualizeBRDFOutput = m_Graph->GetBlackboard().Get<VisualizeBRDFPass::PassData>();
    //backbuffer = visualizeBRDFOutput.ColorOut;

    m_HiZVisualizePass->AddToGraph(*m_Graph, visibility.HiZOut);
    auto& hizVisualizePassOutput = m_Graph->GetBlackboard().Get<HiZVisualize::PassData>();
    m_BlitHiZ->AddToGraph(*m_Graph, hizVisualizePassOutput.ColorOut, backbuffer,
        glm::vec3{0.75f, 0.05f, 0.0f}, glm::vec3{0.2f, 0.2f, 1.0f});

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
        //OnUpdate();
        m_CameraController->OnUpdate(1.0f / 60.0f);
        OnRender();
    }
}

void Renderer::OnRender()
{
    CPU_PROFILE_FRAME("On render")

    BeginFrame();
    ImGuiUI::BeginFrame();
    ProcessPendingCubemaps();
    ProcessPendingPBRTextures();

    {
        CPU_PROFILE_FRAME("Setup Render Graph")
        SetupRenderGraph();
    }
    

    if (!m_FrameEarlyExit)
    {
        m_Graph->Compile(GetFrameContext());
        static u32 twice = 2;
        if (twice)
        {
            LOG("{}", m_Graph->MermaidDump());
            twice --;
        }
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
    m_Graph->GetResolutionDeletionQueue().Flush();

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
