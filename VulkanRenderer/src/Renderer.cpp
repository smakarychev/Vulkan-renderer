#include "Renderer.h"

#include <volk.h>
#include <glm/ext/matrix_transform.hpp>
#include <tracy/Tracy.hpp>

#include "Model.h"
#include "Core/Input.h"
#include "Core/Random.h"

#include "GLFW/glfw3.h"
#include "Imgui/ImguiUI.h"
#include "Vulkan/RenderCommand.h"
#include "RenderGraph/ModelCollection.h"
#include "RenderGraph/RenderPassGeometry.h"
#include "RenderGraph/AO/SsaoBlurPass.h"
#include "RenderGraph/AO/SsaoPass.h"
#include "RenderGraph/AO/SsaoVisualizePass.h"
#include "RenderGraph/Culling/CullMetaPass.h"
#include "RenderGraph/Culling/MeshletCullPass.h"
#include "RenderGraph/Extra/SlimeMold/SlimeMoldPass.h"
#include "RenderGraph/DrawIndirectCulledPass/DrawIndirectCountPass.h"
#include "RenderGraph/HiZ/HiZVisualize.h"
#include "RenderGraph/PBR/VisualizeBRDFPass.h"
#include "RenderGraph\PBR\PbrVisibilityBufferIBLPass.h"
#include "RenderGraph/PostProcessing/CRT/CrtPass.h"
#include "RenderGraph/PostProcessing/Sky/SkyGradientPass.h"
#include "RenderGraph/Skybox/SkyboxPass.h"
#include "RenderGraph/Utility/BlitPass.h"
#include "RenderGraph/Utility/CopyTexturePass.h"
#include "Rendering/Image/Processing/BRDFProcessor.h"
#include "Rendering/Image/Processing/CubemapProcessor.h"
#include "Rendering/Image/Processing/DiffuseIrradianceProcessor.h"
#include "Rendering/Image/Processing/EnvironmentPrefilterProcessor.h"

Renderer::Renderer() = default;

void Renderer::Init()
{
    InitRenderingStructures();
    //LoadScene();
    //InitDepthPyramidComputeStructures();
    //InitVisibilityBufferVisualizationStructures();

    Input::s_MainViewportSize = m_Swapchain.GetResolution();
    m_Camera = std::make_shared<Camera>();
    m_CameraController = std::make_unique<CameraController>(m_Camera);
    for (auto& ctx : m_FrameContexts)
        ctx.MainCamera = m_Camera.get();

    InitRenderGraph();
}

void Renderer::InitRenderGraph()
{
    Model* car = Model::LoadFromAsset("../assets/models/car/scene.model");
    m_GraphModelCollection.CreateDefaultTextures();
    m_GraphModelCollection.RegisterModel(car, "car");
    m_GraphModelCollection.AddModelInstance("car", {glm::mat4{1.0f}});
    m_GraphOpaqueGeometry = RenderPassGeometry::FromModelCollectionFiltered(m_GraphModelCollection,
        *GetFrameContext().ResourceUploader,
        [this](auto& obj) {
            return m_GraphModelCollection.GetMaterials()[obj.Material].Type ==
                assetLib::ModelInfo::MaterialType::Opaque;
        });
    m_SkyboxTexture = Texture::Builder({.Usage = ImageUsage::Sampled | ImageUsage::Storage})
        .FromEquirectangular("../assets/textures/evening_meadow_4k.tx")
        .Build();
    m_SkyboxIrradianceMap = DiffuseIrradianceProcessor::CreateEmptyTexture();
    m_SkyboxPrefilterMap = EnvironmentPrefilterProcessor::CreateEmptyTexture();
    DiffuseIrradianceProcessor::Add(m_SkyboxTexture, m_SkyboxIrradianceMap);
    EnvironmentPrefilterProcessor::Add(m_SkyboxTexture, m_SkyboxPrefilterMap);

    m_Graph = std::make_unique<RenderGraph::Graph>();
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
    materialDescriptors.UpdateBinding("u_materials", m_GraphOpaqueGeometry.GetMaterialsBuffer().BindingInfo());
    m_GraphOpaqueGeometry.GetModelCollection().ApplyMaterialTextures(materialDescriptors);
    
    auto visibilityTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
        "../assets/shaders/processed/render-graph/general/visibility-buffer-vert.shader",
        "../assets/shaders/processed/render-graph/general/visibility-buffer-frag.shader",},
        "Pass.VisibilityBuffer", m_Graph->GetArenaAllocators());
    ShaderPipeline visibilityPipeline = ShaderPipeline::Builder()
        .SetTemplate(visibilityTemplate)
        .SetRenderingDetails({
            .ColorFormats = {Format::R32_UINT},
            .DepthFormat = Format::D32_FLOAT})
        .AlphaBlending(AlphaBlending::None)
        .UseDescriptorBuffer()
        .Build();
    CullMetaPassInitInfo visibilityPassInitInfo = {
        .DrawPipeline = &visibilityPipeline,
        .MaterialDescriptors = &materialDescriptors,
        .DrawFeatures = CullMetaPassInitInfo::Features::AlphaTest,
        .Resolution = m_Swapchain.GetResolution(),
        .Geometry = &m_GraphOpaqueGeometry};
    m_VisibilityBufferPass = std::make_shared<CullMetaPass>(*m_Graph, visibilityPassInitInfo, "VisibilityBuffer");
    m_PbrVisibilityBufferIBLPass = std::make_shared<PbrVisibilityBufferIBL>(*m_Graph, PbrVisibilityBufferInitInfo{
        .MaterialDescriptors = &materialDescriptors});
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
    auto& slimeMoldOutput = m_Graph->GetBlackboard().GetOutput<SlimeMoldPass::GradientPassData>();
    m_CopyTexturePass->AddToGraph(*m_Graph, slimeMoldOutput.GradientMap, m_Graph->GetBackbuffer(),
        glm::vec3{0.0f}, glm::vec3{1.0f});
    m_SlimeMoldPass->AddToGraph(*m_Graph, SlimeMoldPassStage::CopyDiffuse, *m_SlimeMoldContext);
}

CullMetaPass::PassData Renderer::SetupVisibilityBufferPass()
{
    using namespace RenderGraph;

    Resource visibility = m_Graph->CreateResource(m_VisibilityBufferPass->GetName() + ".VisibilityBuffer",
        GraphTextureDescription{
            .Width = m_Swapchain.GetResolution().x,
            .Height = m_Swapchain.GetResolution().y,
            .Format = Format::R32_UINT});

    m_VisibilityBufferPass->AddToGraph(*m_Graph, {
        .FrameContext = &GetFrameContext(),
        .Colors = {
            CullMetaPassExecutionInfo::ColorInfo{
                .Color = visibility,
                .OnLoad = AttachmentLoad::Clear,
                .ClearValue = {.Color = {.U = glm::uvec4{std::numeric_limits<u32>::max(), 0, 0, 0}}}}},
        .Depth = RenderGraph::Resource{}});

    auto& output = m_Graph->GetBlackboard().GetOutput<CullMetaPass::PassData>(m_VisibilityBufferPass->GetNameHash());

    return output; 
}

void Renderer::SetupRenderGraph()
{
    using namespace RenderGraph;
    
    m_Graph->Reset(GetFrameContext());
    Resource backbuffer = m_Graph->GetBackbuffer();

    auto visibility = SetupVisibilityBufferPass();

    //m_SkyGradientPass->AddToGraph(*m_Graph,
    //    m_Graph->CreateResource("Sky.Target", GraphTextureDescription{
    //        .Width = GetFrameContext().Resolution.x,
    //        .Height = GetFrameContext().Resolution.y,
    //        .Format = Format::RGBA16_FLOAT}));
    //auto& skyGradientOutput = m_Graph->GetBlackboard().GetOutput<SkyGradientPass::PassData>();

    if (visibility.DepthOut.has_value())
    {
        m_SsaoPass->AddToGraph(*m_Graph, *visibility.DepthOut);
        auto& ssaoOutput = m_Graph->GetBlackboard().GetOutput<SsaoPass::PassData>();

        m_SsaoBlurHorizontalPass->AddToGraph(*m_Graph, ssaoOutput.SSAO, {});
        auto& ssaoBlurHorizontalOutput = m_Graph->GetBlackboard().GetOutput<SsaoBlurPass::PassData>(
            m_SsaoBlurHorizontalPass->GetNameHash());

        m_SsaoBlurVerticalPass->AddToGraph(*m_Graph, ssaoBlurHorizontalOutput.SsaoOut, ssaoOutput.SSAO);
        auto& ssaoBlurVerticalOutput = m_Graph->GetBlackboard().GetOutput<SsaoBlurPass::PassData>(
                m_SsaoBlurVerticalPass->GetNameHash());
        
        m_SsaoVisualizePass->AddToGraph(*m_Graph, ssaoBlurVerticalOutput.SsaoOut, backbuffer);
        backbuffer = m_Graph->GetBlackboard().GetOutput<SsaoVisualizePass::PassData>().ColorOut;
    }

    auto ssaoBlurVerticalOutput = m_Graph->GetBlackboard().TryGetOutput<SsaoBlurPass::PassData>(
        m_SsaoBlurVerticalPass->GetNameHash());

    m_PbrVisibilityBufferIBLPass->AddToGraph(*m_Graph, {
        .VisibilityTexture = visibility.ColorsOut[0],
        .SSAOTexture = ssaoBlurVerticalOutput != nullptr ? ssaoBlurVerticalOutput->SsaoOut : Resource{},
        .IrradianceMap = m_Graph->AddExternal("IrradianceMap", m_SkyboxIrradianceMap),
        .PrefilterMap = m_Graph->AddExternal("PrefilterMap", m_SkyboxPrefilterMap),
        .BRDF = m_Graph->AddExternal("BRDF", *m_BRDF),
        .ColorIn = {},
        .Geometry = &m_GraphOpaqueGeometry});
    auto& pbrOutput = m_Graph->GetBlackboard().GetOutput<PbrVisibilityBufferIBL::PassData>();

    m_SkyboxPass->AddToGraph(*m_Graph,
        m_SkyboxTexture, pbrOutput.ColorOut, *visibility.DepthOut, GetFrameContext().Resolution);
    auto& skyboxOutput = m_Graph->GetBlackboard().GetOutput<SkyboxPass::PassData>();
    
    m_CrtPass->AddToGraph(*m_Graph, skyboxOutput.ColorOut, backbuffer);
    auto& crtOut = m_Graph->GetBlackboard().GetOutput<CrtPass::PassData>();
    backbuffer = crtOut.ColorOut;

    //m_VisualizeBRDFPass->AddToGraph(*m_Graph, *m_BRDF, backbuffer, GetFrameContext().Resolution);
    //auto& visualizeBRDFOutput = m_Graph->GetBlackboard().GetOutput<VisualizeBRDFPass::PassData>();
    //backbuffer = visualizeBRDFOutput.ColorOut;

    m_HiZVisualizePass->AddToGraph(*m_Graph, visibility.HiZOut);
    auto& hizVisualizePassOutput = m_Graph->GetBlackboard().GetOutput<HiZVisualize::PassData>();
    m_BlitHiZ->AddToGraph(*m_Graph, hizVisualizePassOutput.ColorOut, backbuffer,
        glm::vec3{0.25f, 0.05f, 0.0f}, glm::vec3{0.2f, 0.2f, 1.0f});

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
    if (CubemapProcessor::HasPending())
        CubemapProcessor::Process(GetFrameContext().Cmd);
}

void Renderer::ProcessPendingPBRTextures()
{
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

    // todo: is this the best place for it?
    m_ResourceUploader.SubmitUpload();
    
    cmd.End();

    cmd.Submit(m_Device.GetQueues().Graphics,
        BufferSubmitSyncInfo{
            .WaitStages = {PipelineStage::ColorOutput},
            .WaitSemaphores = {&sync.PresentSemaphore},
            .SignalSemaphores = {&sync.RenderSemaphore},
            .Fence = &sync.RenderFence});
    
    bool isFramePresentSuccessful = m_Swapchain.PresentImage(m_Device.GetQueues().Presentation, m_SwapchainImageIndex, frameNumber); 
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

    m_VisibilityBufferPass.reset();
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
