#include "Renderer.h"

#include <volk.h>
#include <glm/ext/matrix_transform.hpp>
#include <tracy/Tracy.hpp>

#include "RenderObject.h"
#include "Model.h"
#include "VisibilityPass.h"
#include "Core/Input.h"
#include "Core/Random.h"

#include "GLFW/glfw3.h"
#include "Imgui/ImguiUI.h"
#include "Vulkan/RenderCommand.h"
#include "Rendering/RenderingUtils.h"
#include "RenderGraph/ModelCollection.h"
#include "RenderGraph/RenderPassGeometry.h"
#include "RenderGraph/RenderPassGeometryCull.h"
#include "RenderGraph/Culling/CullMetaPass.h"
#include "RenderGraph/Culling/MeshletCullPass.h"
#include "RenderGraph/Extra/SlimeMold/SlimeMoldPass.h"
#include "RenderGraph/DrawIndirectCulledPass/DrawIndirectCountPass.h"
#include "RenderGraph/HiZ/HiZVisualize.h"
#include "RenderGraph/PostProcessing/CRT/CrtPass.h"
#include "RenderGraph/PostProcessing/Sky/SkyGradientPass.h"
#include "RenderGraph/Utility/BlitPass.h"
#include "RenderGraph/Utility/CopyTexturePass.h"
#include "Rendering/DepthPyramid.h"

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
        ctx.Camera = m_Camera.get();

    InitRenderGraph();
}

void Renderer::InitRenderGraph()
{
    Model* car = Model::LoadFromAsset("../assets/models/armor/scene.model");
    m_GraphModelCollection.RegisterModel(car, "car");
    m_GraphModelCollection.AddModelInstance("car", {glm::mat4{1.0f}});
    m_GraphModelCollection.AddModelInstance("car", {glm::translate(glm::mat4{1.0f}, glm::vec3{-2.0f, 0.0f, 0.0f})});
    m_GraphModelCollection.AddModelInstance("car", {glm::translate(glm::mat4{1.0f}, glm::vec3{2.0f, 0.0f, 0.0f})});
    m_GraphOpaqueGeometry = RenderPassGeometry::FromModelCollectionFiltered(m_GraphModelCollection,
        *GetFrameContext().ResourceUploader,
            [](auto& obj) { return true; });

    m_Graph = std::make_unique<RenderGraph::Graph>();
    m_Graph->SetBackbuffer(m_Swapchain.GetDrawImage());

    m_TriangleCull = std::make_shared<CullMetaPass>(*m_Graph, CullMetaPassInitInfo{
        .DrawTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
            "../assets/shaders/processed/render-graph/general/draw-indirect-culled-vert.shader",
            "../assets/shaders/processed/render-graph/general/draw-indirect-culled-frag.shader",},
            "Pass.DrawCulled", m_Graph->GetArenaAllocators()),
        .Resolution = m_Swapchain.GetResolution(),
        .Geometry = &m_GraphOpaqueGeometry}, "MetaCull");

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

void Renderer::SetupRenderGraph()
{
    m_Graph->Reset();

    m_SkyGradientPass->AddToGraph(*m_Graph,
        m_Graph->CreateResource("Sky.Target", RenderGraph::GraphTextureDescription{
            .Width = GetFrameContext().Resolution.x,
            .Height = GetFrameContext().Resolution.y,
            .Format = Format::RGBA16_FLOAT}));
    auto& skyGradientOutput = m_Graph->GetBlackboard().GetOutput<SkyGradientPass::PassData>();
    
    m_TriangleCull->AddToGraph(*m_Graph, {
        .FrameContext = &GetFrameContext(),
        .Colors = {skyGradientOutput.ColorOut},
        .Depth = RenderGraph::Resource{}});
    auto& triangleCullOut = m_Graph->GetBlackboard().GetOutput<CullMetaPass::PassData>(m_TriangleCull->GetNameHash());
    
    m_CrtPass->AddToGraph(*m_Graph, triangleCullOut.ColorsOut[0], m_Graph->GetBackbuffer());

    m_HiZVisualizePass->AddToGraph(*m_Graph, triangleCullOut.HiZOut);
    auto& hizVisualizePassOutput = m_Graph->GetBlackboard().GetOutput<HiZVisualize::PassData>();
    m_BlitHiZ->AddToGraph(*m_Graph, hizVisualizePassOutput.ColorOut, m_Graph->GetBackbuffer(),
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

    {
        CPU_PROFILE_FRAME("Setup Render Graph")
        SetupRenderGraph();
    }
    
    BeginFrame();
    ImGuiUI::BeginFrame();
    if (!m_FrameEarlyExit)
    {
        //if (!m_ComputeDepthPyramidData.DepthPyramid)
        //{
        //    CreateDepthPyramid();
        //    InitVisibilityPass();
        //}
//
        //{
        //    TracyVkZone(ProfilerContext::Get()->GraphicsContext(), Driver::GetProfilerCommandBuffer(ProfilerContext::Get()), "Scene passes")
        //    SceneVisibilityPass();
        //}

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
    //glm::vec3 pos = m_Camera->GetPosition();
    //glm::mat4 translation = glm::translate(glm::mat4(1.0f), 2.0f * glm::vec3(sinf(glfwGetTime() * 2), cosf(glfwGetTime() * 3), sinf(5 * glfwGetTime())));
    //m_Camera->SetPosition(translation * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
    UpdateCameraBuffers();
    UpdateScene();
    UpdateComputeCullBuffers();
}

void Renderer::UpdateCameraBuffers()
{
    m_CameraDataUBO.CameraData = {
        .View = m_Camera->GetView(),
        .Projection = m_Camera->GetProjection(),
        .ViewProjection = m_Camera->GetViewProjection()};
    u64 offsetBytes = renderUtils::alignUniformBufferSizeBytes(sizeof(CameraData)) * GetFrameContext().FrameNumber;
    m_ResourceUploader.UpdateBuffer(
        m_CameraDataUBO.Buffer, &m_CameraDataUBO.CameraData, sizeof(CameraData), offsetBytes);

    FrustumPlanes frustumPlanes = m_Camera->GetFrustumPlanes();
    
    m_CameraDataExtendedUBO.CameraData = {
        .View = m_Camera->GetView(),
        .Projection = m_Camera->GetProjection(),
        .ViewProjection = m_Camera->GetViewProjection(),
        .ViewProjectionInverse = glm::inverse(m_Camera->GetViewProjection()),
        .CameraPosition = glm::vec4(m_Camera->GetPosition(), 0.0),
        .WindowSize = {(f32)m_Swapchain.GetResolution().x, (f32)m_Swapchain.GetResolution().y},
        .FrustumNear = frustumPlanes.Near,
        .FrustumFar = frustumPlanes.Far};
    offsetBytes = renderUtils::alignUniformBufferSizeBytes(sizeof(CameraDataExtended)) * GetFrameContext().FrameNumber;
    m_ResourceUploader.UpdateBuffer(
        m_CameraDataExtendedUBO.Buffer, &m_CameraDataExtendedUBO.CameraData, sizeof(CameraDataExtended), offsetBytes);
}

void Renderer::UpdateComputeCullBuffers()
{
    m_OpaqueGeometryCull.Prepare(*m_Camera, m_ResourceUploader, GetFrameContext());
}

void Renderer::UpdateScene()
{
    FrameContext& context = GetFrameContext();
    
    f32 freq = (f32)glfwGetTime() / 10.0f;
    f32 red = (sin(freq) + 1.0f) * 0.5f;
    f32 green = (cos(freq) + 1.0f) * 0.5f;
    f32 blue = (red + green) * 0.5f;
    f32 sunFreq = (f32)glfwGetTime();
    f32 sunPos = sin(sunFreq);
    m_SceneDataUBO.SceneData.SunlightDirection = {sunPos * 2.0f, (sunPos + 2.0f) * 10.0f, sunPos * 8.0f, 1.0f};
    m_SceneDataUBO.SceneData.SunlightColor = {0.8f, 0.1f, 0.1f, 1.0};
    m_SceneDataUBO.SceneData.FogColor = {0.3f, 0.1f, 0.1f, 1.0f};
    u64 offsetBytes = renderUtils::alignUniformBufferSizeBytes(sizeof(SceneData)) * context.FrameNumber;
    m_ResourceUploader.UpdateBuffer(m_SceneDataUBO.Buffer, &m_SceneDataUBO.SceneData, sizeof(SceneData), offsetBytes);
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

RenderingInfo Renderer::GetColorRenderingInfo()
{
    RenderingAttachment color = RenderingAttachment::Builder()
        .SetType(RenderingAttachmentType::Color)
        .FromImage(m_Swapchain.GetDrawImage(), ImageLayout::ColorAttachment)
        .LoadStoreOperations(AttachmentLoad::Load, AttachmentStore::Store)
        .Build(GetFrameContext().DeletionQueue);

    RenderingInfo renderingInfo = RenderingInfo::Builder()
        .AddAttachment(color)
        .SetResolution(m_Swapchain.GetResolution())
        .Build(GetFrameContext().DeletionQueue);

    return renderingInfo;
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

    // todo: is this the best place for it?
    m_ResourceUploader.StartRecording();
}

void Renderer::Dispatch(const ComputeDispatch& dispatch)
{
    /*CommandBuffer& cmd = GetFrameContext().ComputeCommandBuffer;
    
    dispatch.Pipeline->Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    u32 computeUboOffset = (u32)(vkUtils::alignUniformBufferSizeBytes(sizeof(f32) * GetFrameContext().FrameNumber));
    dispatch.DescriptorSet->Bind(cmd, DescriptorKind::Global, dispatch.Pipeline->GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE, {computeUboOffset});
    RenderCommand::Dispatch(cmd, dispatch.GroupSize);*/
}

void Renderer::CreateDepthPyramid()
{
    CPU_PROFILE_FRAME("Create depth pyramid")

    CommandBuffer& cmd = GetFrameContext().Cmd;
    if (m_ComputeDepthPyramidData.DepthPyramid)
        m_ComputeDepthPyramidData.DepthPyramid.reset();
    
    m_ComputeDepthPyramidData.DepthPyramid = std::make_unique<DepthPyramid>(m_Swapchain.GetDepthImage(), cmd,
        &m_ComputeDepthPyramidData);

    m_OpaqueGeometryCull.SetDepthPyramid(*m_ComputeDepthPyramidData.DepthPyramid, GetFrameContext().Resolution);
}

void Renderer::ComputeDepthPyramid()
{
    CPU_PROFILE_FRAME("Compute depth pyramid")

    CommandBuffer& cmd = GetFrameContext().Cmd;
    m_ComputeDepthPyramidData.DepthPyramid->Compute(m_Swapchain.GetDepthImage(), cmd, GetFrameContext().DeletionQueue);
}

void Renderer::SceneVisibilityPass()
{
    m_VisibilityPass.RenderVisibility({
        .FrameContext = &GetFrameContext(),
        .DepthBuffer = &m_Swapchain.GetDepthImage()});

    u32 cameraDataOffset = u32(renderUtils::alignUniformBufferSizeBytes(sizeof(CameraDataExtended)) * GetFrameContext().FrameNumber);

    CommandBuffer& cmd = GetFrameContext().Cmd;
    RenderCommand::SetViewport(cmd, m_Swapchain.GetResolution());
    RenderCommand::SetScissors(cmd, {0, 0}, m_Swapchain.GetResolution());

    RenderCommand::BeginRendering(cmd, GetColorRenderingInfo());

    m_VisibilityBufferVisualizeData.Pipeline.BindGraphics(cmd);
    const PipelineLayout& layout = m_VisibilityBufferVisualizeData.Pipeline.GetLayout();
    m_VisibilityBufferVisualizeData.DescriptorSet.BindGraphics(cmd, DescriptorKind::Global, layout, {cameraDataOffset});
    m_VisibilityBufferVisualizeData.DescriptorSet.BindGraphics(cmd, DescriptorKind::Pass, layout);
    RenderCommand::Draw(cmd, 3);

    RenderCommand::EndRendering(cmd);
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

    // descriptors
    m_PersistentDescriptorAllocator = DescriptorAllocator::Builder()
        .SetMaxSetsPerPool(100000)
        .Build();

    m_CullDescriptorAllocator = DescriptorAllocator::Builder()
        .SetMaxSetsPerPool(100)
        .Build();

    m_ResolutionDependentAllocator = DescriptorAllocator::Builder()
        .SetMaxSetsPerPool(100000)
        .Build();
    
    m_ResolutionDependentCullAllocator = DescriptorAllocator::Builder()
        .SetMaxSetsPerPool(100)
        .Build();

    m_SceneDataUBO.Buffer = Buffer::Builder()
        .SetUsage(BufferUsage::Uniform | BufferUsage::Upload)
        .SetSizeBytes(renderUtils::alignUniformBufferSizeBytes(sizeof(SceneData)) * BUFFERED_FRAMES)
        .Build();

    m_CameraDataUBO.Buffer = Buffer::Builder()
        .SetUsage(BufferUsage::Uniform | BufferUsage::Upload)
        .SetSizeBytes(renderUtils::alignUniformBufferSizeBytes(sizeof(CameraData)) * BUFFERED_FRAMES)
        .Build();

    m_CameraDataExtendedUBO.Buffer = Buffer::Builder()
        .SetUsage(BufferUsage::Uniform | BufferUsage::Upload)
        .SetSizeBytes(renderUtils::alignUniformBufferSizeBytes(sizeof(CameraDataExtended)) * BUFFERED_FRAMES)
        .Build();

    m_CurrentFrameContext = &m_FrameContexts.front();
}

void Renderer::InitDepthPyramidComputeStructures()
{
    m_ComputeDepthPyramidData.PipelineTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate(
        {"../assets/shaders/processed/culling/depth-pyramid-comp.shader"},
        "depth-pyramid",
        m_ResolutionDependentCullAllocator);

    m_ComputeDepthPyramidData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(m_ComputeDepthPyramidData.PipelineTemplate)
        .Build();
}

void Renderer::InitVisibilityPass()
{
    m_VisibilityPass.Init({
        .Size = m_Swapchain.GetResolution(),
        .Cmd = &GetFrameContext().Cmd,
        .DescriptorAllocator = &m_PersistentDescriptorAllocator,
        .RenderingDetails = m_Swapchain.GetRenderingDetails(),
        .CameraBuffer = &m_CameraDataUBO.Buffer,
        .RenderPassGeometry = &m_OpaqueGeometry,
        .RenderPassGeometryCull = &m_OpaqueGeometryCull});

    m_VisibilityBufferVisualizeData.DescriptorSet = ShaderDescriptorSet::Builder()
        .SetTemplate(m_VisibilityBufferVisualizeData.Template)
        .AddBinding("u_visibility_texture", m_VisibilityPass.GetVisibilityImage().CreateBindingInfo(
            ImageFilter::Nearest, ImageLayout::General))
        .AddBinding("u_camera_buffer", m_CameraDataExtendedUBO.Buffer, sizeof(CameraDataExtended), 0)
        .AddBinding("u_object_buffer", m_OpaqueGeometry.GetRenderObjectsBuffer())
        .AddBinding("u_positions_buffer", m_OpaqueGeometry.GetAttributeBuffers().Positions)
        .AddBinding("u_normals_buffer", m_OpaqueGeometry.GetAttributeBuffers().Normals)
        .AddBinding("u_tangents_buffer", m_OpaqueGeometry.GetAttributeBuffers().Tangents)
        .AddBinding("u_uvs_buffer", m_OpaqueGeometry.GetAttributeBuffers().UVs)
        .AddBinding("u_indices_buffer", m_OpaqueGeometry.GetAttributeBuffers().Indices)
        .AddBinding("u_command_buffer", m_OpaqueGeometry.GetCommandsBuffer())
        .AddBinding("u_material_buffer", m_OpaqueGeometry.GetMaterialsBuffer())
        .AddBinding("u_textures", BINDLESS_TEXTURES_COUNT)
        .Build();

    m_ModelCollection.ApplyMaterialTextures(m_VisibilityBufferVisualizeData.DescriptorSet);
}

void Renderer::InitVisibilityBufferVisualizationStructures()
{
    m_VisibilityBufferVisualizeData.Template = ShaderTemplateLibrary::LoadShaderPipelineTemplate(
        {"../assets/shaders/processed/visibility-buffer/visualize-vert.shader",
        "../assets/shaders/processed/visibility-buffer/visualize-frag.shader"},
        "visualize-visibility-pipeline",
        m_ResolutionDependentAllocator);

    RenderingDetails renderingDetails = m_Swapchain.GetRenderingDetails();
    renderingDetails.DepthFormat = Format::Undefined;
    
    m_VisibilityBufferVisualizeData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(m_VisibilityBufferVisualizeData.Template)
        .SetRenderingDetails(renderingDetails)
        .Build();
}

void Renderer::Shutdown()
{
    m_Device.WaitIdle();

    m_ComputeDepthPyramidData.DepthPyramid.reset();

    Swapchain::DestroyImages(m_Swapchain);
    Swapchain::Destroy(m_Swapchain);

    m_TriangleCull.reset();
    m_Graph.reset();
    ShutdownVisibilityPass();
    RenderPassGeometryCull::Shutdown(m_OpaqueGeometryCull);
    m_ResourceUploader.Shutdown();
    for (auto& ctx : m_FrameContexts)
        ctx.DeletionQueue.Flush();
    ProfilerContext::Get()->Shutdown();

    ImGuiUI::Shutdown();
    Driver::Shutdown();
    glfwDestroyWindow(m_Window); // optional (glfwTerminate does same thing)
    glfwTerminate();
}

void Renderer::ShutdownVisibilityPass()
{
    m_VisibilityPass.Shutdown();
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
    m_ResolutionDependentAllocator.ResetPools();
    m_ResolutionDependentCullAllocator.ResetPools();
    
    m_Swapchain = newSwapchainBuilder.BuildManualLifetime();
    m_ComputeDepthPyramidData.DepthPyramid.reset();

    m_Graph->SetBackbuffer(m_Swapchain.GetDrawImage());

    Input::s_MainViewportSize = m_Swapchain.GetResolution();
    m_Camera->SetViewport(m_Swapchain.GetResolution().x, m_Swapchain.GetResolution().y);
    for (auto& frameContext : m_FrameContexts)
        frameContext.Resolution = m_Swapchain.GetResolution();
}

void Renderer::LoadScene()
{
    Model* car = Model::LoadFromAsset("../assets/models/car/scene.model");
    Model* mori = Model::LoadFromAsset("../assets/models/mori/scene.model");
    Model* helmet = Model::LoadFromAsset("../assets/models/flight_helmet/FlightHelmet.model");
    Model* armor = Model::LoadFromAsset("../assets/models/armor/scene.model");
    Model* mask = Model::LoadFromAsset("../assets/models/mask/scene.model");
    Model* sphere = Model::LoadFromAsset("../assets/models/sphere_big/scene.model");
    Model* cube = Model::LoadFromAsset("../assets/models/real_cube/scene.model");
    
    m_ModelCollection.RegisterModel(car, "car");
    m_ModelCollection.RegisterModel(mori, "mori");
    m_ModelCollection.RegisterModel(helmet, "helmet");
    m_ModelCollection.RegisterModel(armor, "armor");
    m_ModelCollection.RegisterModel(mask, "mask");
    m_ModelCollection.RegisterModel(sphere, "sphere");
    m_ModelCollection.RegisterModel(cube, "cube");

    std::vector models = {"car", "armor", "helmet", "mori", "mask"};

    for (i32 x = -6; x <= 6; x++)
    {
        for (i32 y = 0; y <= 0; y++)
        {
            for (i32 z = -6; z <= 6; z++)
            {
                u32 modelIndex = Random::UInt32(0, (u32)models.size() - 1);
                glm::mat4 rotate = glm::rotate(glm::mat4(1.0), Random::Float(0.0f, glm::two_pi<f32>()),
                    glm::normalize(Random::Float3()));
                glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(x * 2.0f, y * 1.5f, z * 2.0f)) *
                    rotate *
                    glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));

                m_ModelCollection.AddModelInstance(models[modelIndex], {
                    .Transform = transform});
            }
        }
    }

    m_OpaqueGeometry = RenderPassGeometry::FromModelCollectionFiltered(
        m_ModelCollection,
        m_ResourceUploader,
        [this](const RenderObject& renderObject)
        {
            return m_ModelCollection.GetMaterials()[renderObject.Material].Type ==
                assetLib::ModelInfo::MaterialType::Opaque;
        });

    m_OpaqueGeometryCull = RenderPassGeometryCull::ForGeometry(m_OpaqueGeometry,
        m_CullDescriptorAllocator, m_ResolutionDependentCullAllocator);
}

const FrameContext& Renderer::GetFrameContext() const
{
    return *m_CurrentFrameContext;
}

FrameContext& Renderer::GetFrameContext()
{
    return *m_CurrentFrameContext;
}
