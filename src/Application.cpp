#include "Application.h"

#include "core.h"
#include "utils.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <set>


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
    CreateInstance();
    CreateSurface();
    PickPhysicalDevice(); // we do not need to destroy it (it's basically a gpu)
    CreateLogicalDevice();
    CreateSwapchain();
    CreateSwapchainImageViews();
    CreateRenderPass();
    CreateGraphicsPipeline();
}

void Application::CreateInstance()
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
    std::vector<const char*> requiredExtensions = GetRequiredInstanceExtensions();
    ASSERT(CheckInstanceExtensions(requiredExtensions), "Not all of the required extensions are supported")
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

void Application::CreateSurface()
{
    VkResult res = glfwCreateWindowSurface(m_Instance, m_Window, nullptr, &m_Surface);
    ASSERT(res == VK_SUCCESS, "Failed to initialize surface")
}

void Application::PickPhysicalDevice()
{
    u32 deviceCount = 0;
    vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());

    // todo: might use some scoring system, to define the best possible device to use
    for (auto& device : devices)
    {
        if (IsDeviceSuitable(device))
        {
            m_PhysicalDevice = device;
            break;
        }
    }

    ASSERT(m_PhysicalDevice != VK_NULL_HANDLE, "No suitable physical device found")
}

void Application::CreateLogicalDevice()
{
    // describe the queues that we are going to use
    QueueFamilyIndices queueFamilies = GetQueueFamilies(m_PhysicalDevice);
    std::set<u32> uniqueQueueFamilies = { *queueFamilies.GraphicsFamily, *queueFamilies.PresentationFamily };
    std::vector<VkDeviceQueueCreateInfo> deviceQueueCreateInfos;
    for (auto family : uniqueQueueFamilies)
    {
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.pNext = nullptr;
        queueCreateInfo.queueFamilyIndex = family;
        queueCreateInfo.queueCount = 1;
        f32 queuePriority = 1.0f;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        deviceQueueCreateInfos.push_back(queueCreateInfo);
    }
    
    VkPhysicalDeviceFeatures deviceFeatures = {}; // no specific features atm
    
    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = nullptr;
    std::vector<const char*> deviceExtensions = GetRequiredDeviceExtensions();
    deviceCreateInfo.enabledExtensionCount = (u32)deviceExtensions.size(); 
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data(); 
    deviceCreateInfo.queueCreateInfoCount = (u32)deviceQueueCreateInfos.size();
    deviceCreateInfo.pQueueCreateInfos = deviceQueueCreateInfos.data();
    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

    VkResult res = vkCreateDevice(m_PhysicalDevice, &deviceCreateInfo, nullptr, &m_Device);
    ASSERT(res == VK_SUCCESS, "Failed to create logical device")

    // retrieve graphics queue from device, once it's created
    vkGetDeviceQueue(m_Device, *queueFamilies.GraphicsFamily, 0, &m_GraphicsQueue);
    vkGetDeviceQueue(m_Device, *queueFamilies.PresentationFamily, 0, &m_PresentationQueue);
}

void Application::CreateSwapchain()
{
    SwapchainDetails swapchainDetails = GetSwapchainDetails(m_PhysicalDevice);

    VkSurfaceFormatKHR format = ChooseSwapchainFormat(swapchainDetails.Formats);
    VkPresentModeKHR presentMode =  ChooseSwapchainPresentMode(swapchainDetails.PresentModes);
    VkExtent2D extent =  ChooseSwapchainExtent(swapchainDetails.Capabilities);
    u32 imageCount = ChooseSwapchainImageCount(swapchainDetails.Capabilities);

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.pNext = nullptr;
    createInfo.surface = m_Surface;
    createInfo.imageFormat = format.format;
    createInfo.imageColorSpace = format.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // use swap chain as color attachment
    createInfo.minImageCount = imageCount;
    createInfo.presentMode = presentMode;
    
    // pick sharing mode based on queue families
    std::array queueFamilies = GetQueueFamilies(m_PhysicalDevice).AsArray();
    if (queueFamilies[0] == queueFamilies[1])
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }
    else
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = (u32)queueFamilies.size();
        createInfo.pQueueFamilyIndices = queueFamilies.data();
    }
    createInfo.preTransform = swapchainDetails.Capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    VkResult res = vkCreateSwapchainKHR(m_Device, &createInfo, nullptr, &m_Swapchain);
    ASSERT(res == VK_SUCCESS, "Failed to create swap chain")

    // retrieve swapchain images
    vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, nullptr);
    m_SwapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, m_SwapchainImages.data());

    m_SwapchainFormat = format;
    m_SwapchainExtent = extent;
}

void Application::CreateSwapchainImageViews()
{
    m_SwapchainImageViews.reserve(m_SwapchainImages.size());
    for (auto image : m_SwapchainImages)
    {
        VkImageViewCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.pNext = nullptr;
        createInfo.image = image;
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = m_SwapchainFormat.format;

        // leave color as is
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        VkImageView imageView;
        VkResult res = vkCreateImageView(m_Device, &createInfo, nullptr, &imageView);
        ASSERT(res == VK_SUCCESS, "Failed to create image view")
        m_SwapchainImageViews.push_back(imageView);
    }
}

void Application::CreateRenderPass()
{
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = m_SwapchainFormat.format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // prepare attachment to be presented
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // no stencil yet
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // no stencil yet

    VkAttachmentReference colorAttachmentReference = {};
    colorAttachmentReference.attachment = 0;
    colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpassDescription = {};
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // not compute
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &colorAttachmentReference;
    // it is later referenced in shaders (layout(location = ...))

    VkRenderPassCreateInfo renderPassCreateInfo = {};
    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.pNext = nullptr;
    renderPassCreateInfo.attachmentCount = 1;
    renderPassCreateInfo.pAttachments = &colorAttachment;
    renderPassCreateInfo.subpassCount = 1;
    renderPassCreateInfo.pSubpasses = &subpassDescription;

    VkResult res = vkCreateRenderPass(m_Device, &renderPassCreateInfo, nullptr, &m_RenderPass);
    ASSERT(res == VK_SUCCESS, "Failed to create render pass")
}

void Application::CreateGraphicsPipeline()
{
    std::vector<u32> vertexShaderSrc = utils::compileShaderToSPIRV("assets/shaders/triangle.vert");
    std::vector<u32> fragmentShaderSrc = utils::compileShaderToSPIRV("assets/shaders/triangle.frag");
    
    VkShaderModule vertexShaderModule = CreateShaderModule(vertexShaderSrc);
    VkShaderModule fragmentShaderModule = CreateShaderModule(fragmentShaderSrc);

    VkPipelineShaderStageCreateInfo vertexStageCreateInfo = {};
    vertexStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexStageCreateInfo.pNext = nullptr;
    vertexStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertexStageCreateInfo.module = vertexShaderModule;
    vertexStageCreateInfo.pName = "main";
    
    VkPipelineShaderStageCreateInfo fragmentStageCreateInfo = {};
    fragmentStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragmentStageCreateInfo.pNext = nullptr;
    fragmentStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragmentStageCreateInfo.module = fragmentShaderModule;
    fragmentStageCreateInfo.pName = "main";

    std::array stageCreateInfos = { vertexStageCreateInfo, fragmentStageCreateInfo };

    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
    dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStateCreateInfo.pNext = nullptr;
    dynamicStateCreateInfo.dynamicStateCount = (u32)dynamicStates.size();
    dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();
    VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
    viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportStateCreateInfo.pNext = nullptr;
    viewportStateCreateInfo.viewportCount = 1;
    viewportStateCreateInfo.scissorCount = 1;

    VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};
    vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputStateCreateInfo.pNext = nullptr;
    vertexInputStateCreateInfo.vertexBindingDescriptionCount = 0; // so far, the triangle is hardcoded in shader
    vertexInputStateCreateInfo.vertexAttributeDescriptionCount = 0;

    VkPipelineInputAssemblyStateCreateInfo assemblyStateCreateInfo = {};
    assemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assemblyStateCreateInfo.pNext = nullptr;
    assemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    assemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

    VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {};
    rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationStateCreateInfo.pNext = nullptr;
    rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
    rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
    rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE; // if we do not want an output
    rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationStateCreateInfo.lineWidth = 1.0f;
    rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // todo: if something goes wrong, check me

    // no multisampling for now
    VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {};
    multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleStateCreateInfo.pNext = nullptr;
    multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
    multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // no depth buffer for now
    //VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {};

    // enable standard blending
    VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
    colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                               VK_COLOR_COMPONENT_G_BIT |
                                               VK_COLOR_COMPONENT_B_BIT |
                                               VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachmentState.blendEnable = VK_TRUE;
    colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
    VkPipelineColorBlendStateCreateInfo  colorBlendStateCreateInfo = {};
    colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendStateCreateInfo.pNext = nullptr;
    colorBlendStateCreateInfo.attachmentCount = 1;
    colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;
    colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.pNext = nullptr;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
    pipelineLayoutCreateInfo.setLayoutCount = 0;

    VkResult res = vkCreatePipelineLayout(m_Device, &pipelineLayoutCreateInfo, nullptr, &m_PipelineLayout);
    ASSERT(res == VK_SUCCESS, "Failed to create pipeline layout")
    
    vkDestroyShaderModule(m_Device, vertexShaderModule, nullptr);
    vkDestroyShaderModule(m_Device, fragmentShaderModule, nullptr);
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
    vkDestroyPipelineLayout(m_Device, m_PipelineLayout, nullptr);
    vkDestroyRenderPass(m_Device, m_RenderPass, nullptr);
    for (auto imageView : m_SwapchainImageViews)
        vkDestroyImageView(m_Device, imageView, nullptr);
    vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
    vkDestroyDevice(m_Device, nullptr);
    vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
    vkDestroyInstance(m_Instance, nullptr);
    
    glfwDestroyWindow(m_Window);
    
    glfwTerminate();
}

std::vector<const char*> Application::GetRequiredInstanceExtensions()
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

bool Application::CheckInstanceExtensions(const std::vector<const char*>& requiredExtensions)
{
    // get all available extension from vulkan
    u32 availableExtensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(availableExtensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, availableExtensions.data());

    return utils::checkArrayContainsSubArray(requiredExtensions, availableExtensions,
        [](const char* req, const VkExtensionProperties& avail) { return std::strcmp(req, avail.extensionName); },
        [](const char* req) { LOG("Unsopported instance extension: {}", req); }
    );
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

    return utils::checkArrayContainsSubArray(requiredLayers, availableLayers,
        [](const char* req, const VkLayerProperties& avail) { return std::strcmp(req, avail.layerName); },
        [](const char* req) { LOG("Unsopported validation layer: {}", req); }
    );
}

std::vector<const char*> Application::GetRequiredDeviceExtensions()
{
    return {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
}

bool Application::CheckDeviceExtensions(VkPhysicalDevice device, const std::vector<const char*>& requiredExtensions)
{
    u32 availableExtensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &availableExtensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(availableExtensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &availableExtensionCount, availableExtensions.data());

    return utils::checkArrayContainsSubArray(requiredExtensions, availableExtensions,
        [](const char* req, const VkExtensionProperties& avail) { return std::strcmp(req, avail.extensionName); },
        [](const char* req) { LOG("Unsopported device extension: {}", req); }
    );
}

bool Application::IsDeviceSuitable(VkPhysicalDevice device)
{
    QueueFamilyIndices queueFamilyIndices = GetQueueFamilies(device);
    
    std::vector<const char*> requiredExtensions = GetRequiredDeviceExtensions();
    bool extensionsSupported = CheckDeviceExtensions(device, requiredExtensions);

    bool swapchainIsSufficient = false;
    if (extensionsSupported)
    {
        SwapchainDetails details = GetSwapchainDetails(device);
        swapchainIsSufficient = !(details.Formats.empty() || details.PresentModes.empty());
    }

    // no need to check for `extensionsSupported` since if it's false than `swapchainIsSufficient` is also false
    return queueFamilyIndices.IsComplete() && swapchainIsSufficient;
}

QueueFamilyIndices Application::GetQueueFamilies(VkPhysicalDevice device)
{
    QueueFamilyIndices queueFamilyIndices;
    // we need at least one queue family that supports VK_QUEUE_GRAPHICS_BIT
    u32 familyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, nullptr);
    std::vector<VkQueueFamilyProperties> families(familyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, families.data());

    for (u32 i = 0; i < families.size(); i++)
    {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            queueFamilyIndices.GraphicsFamily = i;
        VkBool32 isPresentationSupported;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_Surface, &isPresentationSupported);
        if (isPresentationSupported)
            queueFamilyIndices.PresentationFamily = i;
        if (queueFamilyIndices.IsComplete())
            break;
    }
    
    return queueFamilyIndices;
}

SwapchainDetails Application::GetSwapchainDetails(VkPhysicalDevice device)
{
    SwapchainDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_Surface, &details.Capabilities);

    u32 surfaceFormatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &surfaceFormatCount, nullptr);
    if (surfaceFormatCount != 0)
    {
        details.Formats.resize(surfaceFormatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &surfaceFormatCount, details.Formats.data());
    }

    u32 surfacePresentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface, &surfacePresentModeCount, nullptr);
    if (surfacePresentModeCount != 0)
    {
        details.PresentModes.resize(surfaceFormatCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface, &surfacePresentModeCount, details.PresentModes.data());
    }

    return details;
}

VkSurfaceFormatKHR Application::ChooseSwapchainFormat(const std::vector<VkSurfaceFormatKHR>& formats)
{
    for (auto& format : formats)
    {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return format;
    }

    // fallback to first if `the best` isn't present
    return formats.front();
}

VkPresentModeKHR Application::ChooseSwapchainPresentMode(const std::vector<VkPresentModeKHR>& presentModes)
{
    for (auto& presentMode : presentModes)
    {
        if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            return presentMode;
    }

    // fallback to standard vsync, which is always present
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Application::ChooseSwapchainExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
    if (capabilities.currentExtent.width != std::numeric_limits<u32>::max())
        return capabilities.currentExtent;

    // indication that extent might not be same as window size
    i32 windowWidth, windowHeight;
    glfwGetFramebufferSize(m_Window, &windowWidth, &windowHeight);
    
    VkExtent2D extent;
    extent.width = std::clamp(windowWidth, (i32)capabilities.minImageExtent.width, (i32)capabilities.maxImageExtent.width);
    extent.height = std::clamp(windowHeight, (i32)capabilities.minImageExtent.height, (i32)capabilities.maxImageExtent.height);

    return extent;
}

u32 Application::ChooseSwapchainImageCount(const VkSurfaceCapabilitiesKHR& capabilities)
{
    if (capabilities.maxImageCount == 0)
        return capabilities.minImageCount + 1;

    return std::min(capabilities.minImageCount + 1, capabilities.maxImageCount); 
}

VkShaderModule Application::CreateShaderModule(const std::vector<u32>& spirv)
{
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.codeSize = spirv.size() * sizeof (u32);
    createInfo.pCode = spirv.data();

    VkShaderModule shaderModule;
    VkResult res = vkCreateShaderModule(m_Device, &createInfo, nullptr, &shaderModule);
    ASSERT(res == VK_SUCCESS, "Failed to create shader module")
    return shaderModule;
}
