#include "Renderer.h"

#include "GLFW/glfw3.h"
#include "Vulkan/RenderCommand.h"

Renderer::Renderer()
{
    Init();
}

Renderer::~Renderer()
{
    ShutDown();
}

void Renderer::Run()
{
    while(!glfwWindowShouldClose(m_Window))
    {
        glfwPollEvents();
        OnRender();
    }
}

void Renderer::OnRender()
{
    u32 imageIndex = m_Swapchain.AcquireImage();

    m_CommandBuffer.Begin();

    f32 green = (std::sinf((f32)glfwGetTime()) + 1.0f) * 0.5f;
    VkClearValue colorClear = {.color = {{0.2f, green, 0.3f, 1.0f}}};
    m_RenderPass.Begin(m_CommandBuffer, m_Framebuffers[imageIndex], {colorClear});

    RenderCommand::SetViewport(m_CommandBuffer, m_Swapchain.GetSize());
    RenderCommand::SetScissors(m_CommandBuffer, {0, 0}, m_Swapchain.GetSize());

    m_Pipeline.Bind(m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS);

    RenderCommand::Draw(m_CommandBuffer);
    
    m_RenderPass.End(m_CommandBuffer);

    m_CommandBuffer.End();
    m_CommandBuffer.Submit(m_Device.GetQueues().Graphics, m_Swapchain.GetFrameSync());
    
    m_Swapchain.PresentImage(m_Device.GetQueues().Presentation, imageIndex);
}

void Renderer::Init()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // do not create opengl context
    m_Window = glfwCreateWindow(600, 400, "My window", nullptr, nullptr);

    m_Device = Device::Builder().
        Defaults().
        SetWindow(m_Window).
        Build();

    m_Swapchain = Swapchain::Builder().
        DefaultHints().
        FromDetails(m_Device.GetSurfaceDetails()).
        SetDevice(m_Device).
        BufferedFrames(1).
        Build();

    std::vector<AttachmentTemplate> attachmentTemplates = m_Swapchain.GetAttachmentTemplates();
    
    Subpass subpass = Subpass::Builder().
        SetAttachments(attachmentTemplates).
        Build();

    m_RenderPass = RenderPass::Builder().
        AddSubpass(subpass).
        AddSubpassDependency(
            VK_SUBPASS_EXTERNAL,
            subpass,
            {
                .SourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .DestinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .SourceAccessMask = 0,
                .DestinationAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
            }).
        SetDevice(m_Device).
        Build();

    m_Framebuffers = m_Swapchain.GetFramebuffers(m_RenderPass);
    
    m_Pipeline = Pipeline::Builder().
        SetRenderPass(m_RenderPass).
        AddShader(ShaderKind::Vertex, "assets/shaders/triangle_smallest.vert").
        AddShader(ShaderKind::Pixel, "assets/shaders/triangle_smallest.frag").
        FixedFunctionDefaults().
        Build();

    m_CommandPool = CommandPool::Builder().
        SetQueue(m_Device, QueueKind::Graphics).
        PerBufferReset(true).
        Build();

    m_CommandBuffer = m_CommandPool.AllocateBuffer(CommandBufferKind::Primary);

    
}

void Renderer::ShutDown()
{
    Driver::Shutdown(m_Device);
    glfwDestroyWindow(m_Window); // optional (glfwTerminate does same thing)
    glfwTerminate();
}
