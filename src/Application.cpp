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
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.pApplicationInfo = &appInfo;

    // specify used extensions
    std::vector<const char*> requiredExtensions = GetRequiredExtensions();
    ASSERT(CheckExtensions(requiredExtensions), "Not all of the required extensions are supported")
    createInfo.enabledExtensionCount = (u32)requiredExtensions.size();
    createInfo.ppEnabledExtensionNames = requiredExtensions.data();

    // specify used validation layers
#ifdef VULKAN_VAL_LAYERS
    std::vector<const char*> requiredValidationLayers = GetRequiredValidationLayers();
    ASSERT(CheckValidationLayers(requiredValidationLayers), "Not all of the required validation layers are supported")
    createInfo.enabledLayerCount = (u32)requiredValidationLayers.size();
    createInfo.ppEnabledLayerNames = requiredValidationLayers.data();
#else
    createInfo.enabledLayerCount = 0;
#endif
    
    
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

std::vector<const char*> Application::GetRequiredExtensions()
{
    // get all the required extensions from glfw
    u32 extensionCount = 0;
    const char** extensionNames = glfwGetRequiredInstanceExtensions(&extensionCount);
    std::vector<const char*> extensions(extensionNames + 0, extensionNames + extensionCount);
#ifdef VULKAN_VAL_LAYERS
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
    return extensions;
}

bool Application::CheckExtensions(const std::vector<const char*>& requiredExtensions)
{
    // get all available extension from vulkan
    u32 availableExtensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, nullptr);
    std::vector<VkExtensionProperties> availExtension(availableExtensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, availExtension.data());

    bool success = true;    
    for (auto& req : requiredExtensions)
    {
        if (std::ranges::none_of(availExtension, [req](auto& ex){ return std::strcmp(req, ex.extensionName); }))
        {
            LOG("Unsopported extension: {}", req);
            success = false;
        }
    }

    return success;
}

std::vector<const char*> Application::GetRequiredValidationLayers()
{
    return {
        "VK_LAYER_KHRONOS_validation",
    };
}

bool Application::CheckValidationLayers(const std::vector<const char*>& requiredLayers)
{
    // get all available layers from vulkan
    u32 availableLayerCount = 0;
    vkEnumerateInstanceLayerProperties(&availableLayerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(availableLayerCount);
    vkEnumerateInstanceLayerProperties(&availableLayerCount, availableLayers.data());

    bool success = true;
    for (auto& req : requiredLayers)
    {
        if (std::ranges::none_of(availableLayers, [req](auto& layer){ return std::strcmp(req, layer.layerName); }))
        {
            LOG("Unsopported validation layer: {}", req);
            success = false;
        }
    }

    return success;
}
