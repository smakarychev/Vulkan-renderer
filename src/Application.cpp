#include "Application.h"

#include "core.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

void Application::Run()
{
    Init();
    MainLoop();
    CleanUp();
}

void Application::Init()
{
    InitWindow();
    InitVulkan();
}

void Application::InitWindow()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // do not crete opengl context
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);   // resizing in vulkan is not trivial
    m_Window = glfwCreateWindow(m_WindowProps.Width, m_WindowProps.Height, m_WindowProps.Name.data(), nullptr, nullptr);
}

void Application::InitVulkan()
{
    // provides some optional info for the driver
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pNext = nullptr;
    appInfo.pApplicationName = "VulkanApp";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;
    
    // tells the driver about extensions and validation layers
    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    createInfo.pNext = nullptr;
    createInfo.pApplicationInfo = &appInfo;

    // get all the required extensions from glfw
    u32 extensionCount = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions(&extensionCount);
    ASSERT(CheckExtensions(extensionCount, extensions), "Not all of the required extensions are supported")
    
    createInfo.enabledExtensionCount = extensionCount;
    createInfo.ppEnabledExtensionNames = extensions;
    createInfo.enabledLayerCount = 0; // disable val layers for now
    VkResult res = vkCreateInstance(&createInfo, nullptr, &m_Instance);
    ASSERT(res == VK_SUCCESS, "Failed to initialize vulkan instance")
}

void Application::MainLoop()
{
    while (!glfwWindowShouldClose(m_Window))
    {
        glfwPollEvents();
    }
}

void Application::CleanUp()
{
    vkDestroyInstance(m_Instance, nullptr);
    
    glfwDestroyWindow(m_Window);
    
    glfwTerminate();
}

bool Application::CheckExtensions(u32 reqExCount, const char** reqEx)
{
    // get all available extension from vulkan
    u32 availExCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &availExCount, nullptr);
    std::vector<VkExtensionProperties> availEx(availExCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &availExCount, availEx.data());

    bool success = true;    

    for (u32 i = 0; i < reqExCount; i++)
    {
        const char* req = reqEx[i];
        if (std::ranges::none_of(availEx, [req](auto& ex){ return std::strcmp(req, ex.extensionName); }))
        {
            LOG("Unsopported extension: {}", req);
            success = false;
        }
    }

    return success;
}
