#include "ImguiUI.h"

#include "types.h"

#include <array>

#include <volk.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_vulkan.h>

#include "Rendering/Device.h"
#include "Vulkan/Driver.h"
#include "Vulkan/RenderCommand.h"

struct ImGuiUI::Payload
{
    VkDescriptorPool Pool;
    GLFWwindow* Window;
};

std::unique_ptr<ImGuiUI::Payload> ImGuiUI::s_Payload = {};

void ImGuiUI::Init(void* window)
{
    std::array poolSizes = {
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};

    VkDescriptorPoolCreateInfo poolCreateInfo = {};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolCreateInfo.maxSets = 1000;
    poolCreateInfo.poolSizeCount = (u32)poolSizes.size();
    poolCreateInfo.pPoolSizes = poolSizes.data();

    s_Payload = std::make_unique<Payload>();
    s_Payload->Window = (GLFWwindow*)window;
    vkCreateDescriptorPool(Driver::DeviceHandle(), &poolCreateInfo, nullptr, &s_Payload->Pool);

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; 
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;    
    ImGui_ImplGlfw_InitForVulkan((GLFWwindow*)window, true);

    DriverResources::DeviceResource& deviceResource = Driver::Resources()[Driver::GetDevice()];
    DriverResources::QueueResource& queueResource = Driver::Resources()[Driver::GetDevice().GetQueues().Graphics];
    ImGui_ImplVulkan_InitInfo imguiInitInfo = {};
    imguiInitInfo.Instance = deviceResource.Instance;
    imguiInitInfo.PhysicalDevice = deviceResource.GPU;
    imguiInitInfo.Device = deviceResource.Device;
    imguiInitInfo.QueueFamily = Driver::GetDevice().GetQueues().Graphics.Family;
    imguiInitInfo.Queue = queueResource.Queue;
    imguiInitInfo.DescriptorPool = s_Payload->Pool;
    imguiInitInfo.MinImageCount = 3;
    imguiInitInfo.ImageCount = 3;
    imguiInitInfo.UseDynamicRendering = true;
    imguiInitInfo.PipelineRenderingCreateInfo = {};
    imguiInitInfo.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    imguiInitInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;
    imguiInitInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats = &format;
    ImGui_ImplVulkan_LoadFunctions([](const char* functionName, void* instance)
    {
        return vkGetInstanceProcAddr(*(VkInstance*)instance, functionName);
    }, &deviceResource.Instance);
    ImGui_ImplVulkan_Init(&imguiInitInfo);
    ImGui_ImplVulkan_CreateFontsTexture();
}

void ImGuiUI::Shutdown()
{
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    vkDestroyDescriptorPool(Driver::DeviceHandle(), s_Payload->Pool, nullptr);
}

void ImGuiUI::BeginFrame()
{
    CPU_PROFILE_FRAME("ImGui begin frame")

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiUI::EndFrame(const CommandBuffer& cmd, const RenderingInfo& renderingInfo)
{
    ImGui::Render();
    
    RenderCommand::BeginRendering(cmd, renderingInfo);
    
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), Driver::Resources()[cmd].CommandBuffer);

    RenderCommand::EndRendering(cmd);
}

