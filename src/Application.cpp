#include "Application.h"

#include "core.h"
#include "utils.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <glm/gtc/matrix_transform.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

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
    stbi_set_flip_vertically_on_load(true);
    InitVulkan();
}

void Application::InitWindow()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // do not create opengl context
    m_Window = glfwCreateWindow(m_WindowProps.Width, m_WindowProps.Height, m_WindowProps.Name.data(), nullptr, nullptr);
    glfwSetWindowUserPointer(m_Window, this);
    glfwSetFramebufferSizeCallback(m_Window, [](GLFWwindow* window, i32 width, i32 height)
    {
        Application* app = (Application*)glfwGetWindowUserPointer(window);
        app->m_WindowResized = true;
    });
}

void Application::InitVulkan()
{
    CreateInstance();
    CreateSurface();
    PickPhysicalDevice();
    CreateLogicalDevice();
    CreateSwapchain();
    CreateSwapchainImageViews();
    CreateRenderPass();
    CreateDescriptorSetLayout();
    CreateGraphicsPipeline();
    CreateCommandPool();
    CreateDepthResources();
    CreateFramebuffers();
    CreateTextureImage();
    CreateTextureImageView();
    CreateTextureSampler();
    m_BufferedFrames.resize(BUFFERED_FRAMES_COUNT);
    CreateVertexBuffer();
    CreateIndexBuffer();
    CreateUniformBuffers();
    CreateDescriptorPool();
    CreateDescriptorSets();
    CreateCommandBuffer();
    CreateSynchronizationPrimitives();
}

void Application::CreateInstance()
{
    // provides some optional info for the driver
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "VulkanApp";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;
    
    // tells the driver about extensions and validation layers
    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
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

    VulkanCheck(vkCreateInstance(&createInfo, nullptr, &m_Instance), "Failed to initialize vulkan instance");
}

void Application::CreateSurface()
{
    VulkanCheck(glfwCreateWindowSurface(m_Instance, m_Window, nullptr, &m_Surface), "Failed to initialize surface");
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
        queueCreateInfo.queueFamilyIndex = family;
        queueCreateInfo.queueCount = 1;
        f32 queuePriority = 1.0f;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        deviceQueueCreateInfos.push_back(queueCreateInfo);
    }
    
    VkPhysicalDeviceFeatures deviceFeatures = {};
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    
    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    std::vector<const char*> deviceExtensions = GetRequiredDeviceExtensions();
    deviceCreateInfo.enabledExtensionCount = (u32)deviceExtensions.size(); 
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data(); 
    deviceCreateInfo.queueCreateInfoCount = (u32)deviceQueueCreateInfos.size();
    deviceCreateInfo.pQueueCreateInfos = deviceQueueCreateInfos.data();
    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

    VulkanCheck(vkCreateDevice(m_PhysicalDevice, &deviceCreateInfo, nullptr, &m_Device), "Failed to create logical device");

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

    VulkanCheck(vkCreateSwapchainKHR(m_Device, &createInfo, nullptr, &m_Swapchain), "Failed to create swap chain");

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
        m_SwapchainImageViews.push_back(CreateImageView(image, m_SwapchainFormat.format, VK_IMAGE_ASPECT_COLOR_BIT));
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

    VkAttachmentDescription depthAttachment = {};
    depthAttachment.format = GetDepthFormat();
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // no stencil yet
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // no stencil yet

    VkAttachmentReference depthAttachmentReference = {};
    depthAttachmentReference.attachment = 1;
    depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpassDescription = {};
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // not compute
    subpassDescription.colorAttachmentCount = 1;
    // it is later referenced in shaders (layout(location = ...))
    subpassDescription.pColorAttachments = &colorAttachmentReference;
    subpassDescription.pDepthStencilAttachment = &depthAttachmentReference;

    VkSubpassDependency colorDependency = {};
    colorDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    colorDependency.dstSubpass = 0;
    colorDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    colorDependency.srcAccessMask = 0;
    colorDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    colorDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkSubpassDependency depthDependency = {};
    depthDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    depthDependency.dstSubpass = 0;
    depthDependency.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    depthDependency.srcAccessMask = 0;
    depthDependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    depthDependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    
    std::array attachments = {colorAttachment, depthAttachment};
    std::array dependencies = {colorDependency, depthDependency};
    
    VkRenderPassCreateInfo renderPassCreateInfo = {};
    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.attachmentCount = (u32)attachments.size();
    renderPassCreateInfo.pAttachments = attachments.data();
    renderPassCreateInfo.subpassCount = 1;
    renderPassCreateInfo.pSubpasses = &subpassDescription;
    renderPassCreateInfo.dependencyCount = (u32)dependencies.size();
    renderPassCreateInfo.pDependencies = dependencies.data();

    VulkanCheck(vkCreateRenderPass(m_Device, &renderPassCreateInfo, nullptr, &m_RenderPass), "Failed to create render pass");
}

void Application::CreateDescriptorSetLayout()
{
    VkDescriptorSetLayoutBinding uniformBinding = {};
    uniformBinding.binding = 0;
    uniformBinding.descriptorCount = 1;
    uniformBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uniformBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutBinding samplerBinding = {};
    samplerBinding.binding = 1;
    samplerBinding.descriptorCount = 1;
    samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uniformBinding, samplerBinding};
    
    VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {};
    layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutCreateInfo.bindingCount = (u32)bindings.size();
    layoutCreateInfo.pBindings = bindings.data();

    VulkanCheck(vkCreateDescriptorSetLayout(m_Device, &layoutCreateInfo, nullptr, &m_DescriptorSetLayout),
        "Failed to create descriptor set layout");
}

void Application::CreateGraphicsPipeline()
{
    std::vector<u32> vertexShaderSrc = utils::compileShaderToSPIRV("assets/shaders/triangle.vert");
    std::vector<u32> fragmentShaderSrc = utils::compileShaderToSPIRV("assets/shaders/triangle.frag");
    
    VkShaderModule vertexShaderModule = CreateShaderModule(vertexShaderSrc);
    VkShaderModule fragmentShaderModule = CreateShaderModule(fragmentShaderSrc);

    VkPipelineShaderStageCreateInfo vertexStageCreateInfo = {};
    vertexStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertexStageCreateInfo.module = vertexShaderModule;
    vertexStageCreateInfo.pName = "main";
    
    VkPipelineShaderStageCreateInfo fragmentStageCreateInfo = {};
    fragmentStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
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
    dynamicStateCreateInfo.dynamicStateCount = (u32)dynamicStates.size();
    dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();
    VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
    viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportStateCreateInfo.viewportCount = 1;
    viewportStateCreateInfo.scissorCount = 1;
    
    VkVertexInputBindingDescription bindingDescription = Vertex::GetBindingDescription();
    std::array attributesDescription = Vertex::GetAttributesDescription();
    VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};
    vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
    vertexInputStateCreateInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputStateCreateInfo.vertexAttributeDescriptionCount = (u32)attributesDescription.size();
    vertexInputStateCreateInfo.pVertexAttributeDescriptions = attributesDescription.data();

    VkPipelineInputAssemblyStateCreateInfo assemblyStateCreateInfo = {};
    assemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    assemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

    VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {};
    rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
    rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
    rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE; // if we do not want an output
    rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationStateCreateInfo.lineWidth = 1.0f;
    rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    // no multisampling for now
    VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {};
    multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
    multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // no depth buffer for now
    VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {};
    depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilStateCreateInfo.depthTestEnable = VK_TRUE;
    depthStencilStateCreateInfo.depthWriteEnable = VK_TRUE;
    depthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
    depthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;

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
    colorBlendStateCreateInfo.attachmentCount = 1;
    colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;
    colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &m_DescriptorSetLayout;

    VulkanCheck(vkCreatePipelineLayout(m_Device, &pipelineLayoutCreateInfo, nullptr, &m_PipelineLayout), "Failed to create pipeline layout");

    VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {};
    graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
    graphicsPipelineCreateInfo.basePipelineIndex = -1;
    graphicsPipelineCreateInfo.stageCount = 2;
    graphicsPipelineCreateInfo.pStages = stageCreateInfos.data();
    graphicsPipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
    graphicsPipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
    graphicsPipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
    graphicsPipelineCreateInfo.pTessellationState = nullptr;
    graphicsPipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
    graphicsPipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
    graphicsPipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
    graphicsPipelineCreateInfo.pInputAssemblyState = &assemblyStateCreateInfo;
    graphicsPipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
    graphicsPipelineCreateInfo.layout = m_PipelineLayout;
    graphicsPipelineCreateInfo.renderPass = m_RenderPass;
    graphicsPipelineCreateInfo.subpass = 0;

    VkPipelineCache pipelineCache = VK_NULL_HANDLE;

    VulkanCheck(vkCreateGraphicsPipelines(m_Device, pipelineCache, 1, &graphicsPipelineCreateInfo, nullptr, &m_Pipeline), "Failed to create pipeline");
    
    vkDestroyShaderModule(m_Device, vertexShaderModule, nullptr);
    vkDestroyShaderModule(m_Device, fragmentShaderModule, nullptr);
}

void Application::CreateFramebuffers()
{
    m_Framebuffers.resize(m_SwapchainImageViews.size());
    for (u32 i = 0; i < m_SwapchainImageViews.size(); i++)
    {
        std::array attachments = {
            m_SwapchainImageViews[i],
            m_DepthTexture.View
        };

        VkFramebufferCreateInfo framebufferCreateInfo = {};
        framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferCreateInfo.renderPass = m_RenderPass;
        framebufferCreateInfo.attachmentCount = (u32)attachments.size();
        framebufferCreateInfo.pAttachments = attachments.data();
        framebufferCreateInfo.width = m_SwapchainExtent.width;
        framebufferCreateInfo.height = m_SwapchainExtent.height;
        framebufferCreateInfo.layers = 1;

        VulkanCheck(vkCreateFramebuffer(m_Device, &framebufferCreateInfo, nullptr, &m_Framebuffers[i]), "Failed to create framebuffer");
    }
}

void Application::CreateCommandPool()
{
    QueueFamilyIndices queueFamilies = GetQueueFamilies(m_PhysicalDevice);
    
    VkCommandPoolCreateInfo poolCreateInfo = {};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolCreateInfo.queueFamilyIndex = *queueFamilies.GraphicsFamily;

    VulkanCheck(vkCreateCommandPool(m_Device, &poolCreateInfo, nullptr, &m_CommandPool), "Failed to create command pool");
}

void Application::CreateDepthResources()
{
    VkFormat depthFormat = GetDepthFormat();

    m_DepthTexture = CreateTexture({
        .Width = m_SwapchainExtent.width,
        .Height = m_SwapchainExtent.height,
        .Format = depthFormat,
        .Tiling = VK_IMAGE_TILING_OPTIMAL,
        .Usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .Properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT});

    m_DepthTexture.View = CreateImageView(m_DepthTexture.Texture, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

    TransitionTextureLayout(m_DepthTexture.Texture, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    
}

void Application::CreateTextureImage()
{
    std::string_view texturePath = "assets/textures/texture.png"; 
    i32 width, height, channels;
    
    u8* pixels = stbi_load(texturePath.data(), &width, &height, &channels, STBI_rgb_alpha);
    ASSERT(pixels, "Failed to load texture: {}", texturePath)

    VkDeviceSize textureSizeBytes = (u64)width * (u64)height * 4;

    BufferData stageBufferData = CreateBuffer({
        .SizeBytes = textureSizeBytes,
        .Usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .Properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT});

    void* data;
    vkMapMemory(m_Device, stageBufferData.BufferMemory, 0, textureSizeBytes, 0, &data);
    memcpy(data, pixels, (usize)textureSizeBytes);
    vkUnmapMemory(m_Device, stageBufferData.BufferMemory);
    
    m_TextureImage = CreateTexture({
        .Width = (u32)width,
        .Height = (u32)height,
        .Format = VK_FORMAT_R8G8B8A8_SRGB,
        .Tiling = VK_IMAGE_TILING_OPTIMAL,
        .Usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .Properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT});

    TransitionTextureLayout(m_TextureImage.Texture, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    CopyBufferToImage(stageBufferData.Buffer, m_TextureImage.Texture, (u32)width, (u32)height);
    TransitionTextureLayout(m_TextureImage.Texture, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    
    vkDestroyBuffer(m_Device, stageBufferData.Buffer, nullptr);
    vkFreeMemory(m_Device, stageBufferData.BufferMemory, nullptr);
        
    stbi_image_free(pixels);
}

void Application::CreateTextureImageView()
{
    m_TextureImage.View = CreateImageView(m_TextureImage.Texture, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
}

void Application::CreateTextureSampler()
{
    VkSamplerCreateInfo samplerCreateInfo = {};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    // query gpu for max anisotropic filtering level
    VkPhysicalDeviceProperties properties = {};
    vkGetPhysicalDeviceProperties(m_PhysicalDevice, &properties);
    samplerCreateInfo.anisotropyEnable = VK_TRUE;
    samplerCreateInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
    samplerCreateInfo.compareEnable = VK_FALSE;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.mipLodBias = 0.0f;
    samplerCreateInfo.minLod = 0.0f;
    samplerCreateInfo.maxLod = 0.0f;

    VulkanCheck(vkCreateSampler(m_Device, &samplerCreateInfo, nullptr, &m_TextureImageSampler),
        "Failed to create image sampler");
}

void Application::CreateVertexBuffer()
{
    // describe vertices
    m_Vertices = {
        {{-0.5f, -0.5f, 0.0f}, {0.5f, 0.5f, 0.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f, 0.0f}, {0.0f, 0.5f, 0.5f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f, 0.0f}, {0.5f, 0.0f, 0.5f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f, 0.0f}, {0.5f, 0.5f, 0.5f}, {0.0f, 1.0f}},

        {{-0.75f, -0.75f, 1.0f}, {0.5f, 0.5f, 0.0f}, {0.0f, 0.0f}},
        {{ 0.25f, -0.75f, 1.0f}, {0.0f, 0.5f, 0.5f}, {1.0f, 0.0f}},
        {{ 0.25f,  0.25f, 1.0f}, {0.5f, 0.0f, 0.5f}, {1.0f, 1.0f}},
        {{-0.75f,  0.25f, 1.0f}, {0.5f, 0.5f, 0.5f}, {0.0f, 1.0f}},
    };

    VkDeviceSize vertexBufferSizeBytes = sizeof(m_Vertices.front()) * m_Vertices.size();
    
    // mapping is possible only of memory allocated from a memory type that has `VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT` flag,
    // `VK_MEMORY_PROPERTY_HOST_COHERENT_BIT` ensures that the mapped memory always matches the contents of the allocated memory
    BufferData stageBufferData = CreateBuffer({
        .SizeBytes = vertexBufferSizeBytes,
        .Usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .Properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT});

    // map memory
    void* data;
    vkMapMemory(m_Device, stageBufferData.BufferMemory, 0, vertexBufferSizeBytes, 0, &data);
    memcpy(data, m_Vertices.data(), (usize)vertexBufferSizeBytes);
    vkUnmapMemory(m_Device, stageBufferData.BufferMemory);

    // the most optimal memory has the VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT flag
    m_VertexBuffer = CreateBuffer({
        .SizeBytes = vertexBufferSizeBytes,
        .Usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .Properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT});

    CopyBuffer(stageBufferData.Buffer, m_VertexBuffer.Buffer, vertexBufferSizeBytes);
    
    vkDestroyBuffer(m_Device, stageBufferData.Buffer, nullptr);
    vkFreeMemory(m_Device, stageBufferData.BufferMemory, nullptr);
}

void Application::CreateIndexBuffer()
{
    m_Indices = {
        0, 1, 2,
        2, 3, 0,

        4, 5, 6,
        6, 7, 4,
    };

    VkDeviceSize indexBufferSizeBytes = sizeof(m_Indices.front()) * m_Indices.size();

    BufferData stageBufferData = CreateBuffer({
        .SizeBytes = indexBufferSizeBytes,
        .Usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .Properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT});

    // map memory
    void* data;
    vkMapMemory(m_Device, stageBufferData.BufferMemory, 0, indexBufferSizeBytes, 0, &data);
    memcpy(data, m_Indices.data(), (usize)indexBufferSizeBytes);
    vkUnmapMemory(m_Device, stageBufferData.BufferMemory);

    m_IndexBuffer = CreateBuffer({
        .SizeBytes = indexBufferSizeBytes,
        .Usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        .Properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT});

    CopyBuffer(stageBufferData.Buffer, m_IndexBuffer.Buffer, indexBufferSizeBytes);
    
    vkDestroyBuffer(m_Device, stageBufferData.Buffer, nullptr);
    vkFreeMemory(m_Device, stageBufferData.BufferMemory, nullptr);
}

void Application::CreateUniformBuffers()
{
    VkDeviceSize bufferSizeBytes = sizeof(TransformUBO);

    m_UniformBuffers.resize(BUFFERED_FRAMES_COUNT);
    m_UniformBuffersMapped.resize(BUFFERED_FRAMES_COUNT);
    
    for (u32 i = 0; i < BUFFERED_FRAMES_COUNT; i++)
    {
        m_UniformBuffers[i] = CreateBuffer({
            .SizeBytes = bufferSizeBytes,
            .Usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            .Properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT});

        // leave unmapped
        vkMapMemory(m_Device, m_UniformBuffers[i].BufferMemory, 0, bufferSizeBytes, 0, &m_UniformBuffersMapped[i]);
    }
}

void Application::CreateDescriptorPool()
{
    std::array<VkDescriptorPoolSize, 2> poolSizes = {};

    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = BUFFERED_FRAMES_COUNT;

    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = BUFFERED_FRAMES_COUNT;
    
    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
    descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCreateInfo.maxSets = BUFFERED_FRAMES_COUNT;
    descriptorPoolCreateInfo.poolSizeCount = (u32)poolSizes.size();
    descriptorPoolCreateInfo.pPoolSizes = poolSizes.data();

    vkCreateDescriptorPool(m_Device, &descriptorPoolCreateInfo, nullptr, &m_DescriptorPool);
}

void Application::CreateDescriptorSets()
{
    std::vector<VkDescriptorSetLayout> layouts(BUFFERED_FRAMES_COUNT, m_DescriptorSetLayout);
    VkDescriptorSetAllocateInfo setAllocateInfo = {};
    setAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    setAllocateInfo.descriptorPool = m_DescriptorPool;
    setAllocateInfo.descriptorSetCount = (u32)layouts.size();
    setAllocateInfo.pSetLayouts = layouts.data();

    m_DescriptorSets.resize(BUFFERED_FRAMES_COUNT);

    VulkanCheck(vkAllocateDescriptorSets(m_Device, &setAllocateInfo, m_DescriptorSets.data()),
        "Failed to allocate descriptor sets");

    // configure allocated descriptor sets
    for (u32 i = 0; i < BUFFERED_FRAMES_COUNT; i++)
    {
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.buffer = m_UniformBuffers[i].Buffer;
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(TransformUBO);

        VkDescriptorImageInfo imageInfo = {};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = m_TextureImage.View;
        imageInfo.sampler = m_TextureImageSampler;

        std::array<VkWriteDescriptorSet, 2> writeDescriptorSets = {};

        writeDescriptorSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSets[0].descriptorType =  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;   
        writeDescriptorSets[0].descriptorCount = 1;
        writeDescriptorSets[0].dstSet = m_DescriptorSets[i];
        writeDescriptorSets[0].dstBinding = 0;
        writeDescriptorSets[0].dstArrayElement = 0;
        writeDescriptorSets[0].pBufferInfo = &bufferInfo;

        writeDescriptorSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSets[1].descriptorType =  VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;   
        writeDescriptorSets[1].descriptorCount = 1;
        writeDescriptorSets[1].dstSet = m_DescriptorSets[i];
        writeDescriptorSets[1].dstBinding = 1;
        writeDescriptorSets[1].dstArrayElement = 0;
        writeDescriptorSets[1].pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(m_Device, (u32)writeDescriptorSets.size(), writeDescriptorSets.data(), 0, nullptr);
    }
}

void Application::CreateCommandBuffer()
{
    VkCommandBufferAllocateInfo bufferAllocateInfo = {};
    bufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    bufferAllocateInfo.commandPool = m_CommandPool;
    bufferAllocateInfo.commandBufferCount = 1;
    bufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    for (auto& frame : m_BufferedFrames)
    {
        VulkanCheck(vkAllocateCommandBuffers(m_Device, &bufferAllocateInfo, &frame.m_CommandBuffer), "Failed to allocate command buffer");
    }
}

void Application::RecordCommandBuffer(VkCommandBuffer cmd, u32 imageIndex)
{
    VkCommandBufferBeginInfo bufferBeginInfo = {};
    bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    VulkanCheck(vkBeginCommandBuffer(cmd, &bufferBeginInfo), "Failed to begin command buffer");

    VkRenderPassBeginInfo renderPassBeginInfo = {};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.renderPass = m_RenderPass; 
    renderPassBeginInfo.framebuffer = m_Framebuffers[imageIndex];
    renderPassBeginInfo.renderArea = VkRect2D{.offset = {0, 0}, .extent = m_SwapchainExtent};
    std::array<VkClearValue, 2> clearValues;
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
    clearValues[1].depthStencil = {1.0f, 0};
    renderPassBeginInfo.clearValueCount = (u32)clearValues.size();
    renderPassBeginInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(cmd, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    
    // tell vulkan which operations to execute in the graphics pipeline and which attachment to use in the fragment shader
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline);

    // set dynamics
    VkViewport viewport = {
        .x = 0, .y = 0,
        .width = (f32)m_SwapchainExtent.width, .height = (f32)m_SwapchainExtent.height,
        .minDepth = 0.0f, .maxDepth = 1.0f
    };
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {.offset = {0, 0}, .extent = m_SwapchainExtent};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    std::array vertexBuffers = { m_VertexBuffer.Buffer };
    std::array<VkDeviceSize, 1> offsets = { 0 };
    vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers.data(), offsets.data());

    vkCmdBindIndexBuffer(cmd, m_IndexBuffer.Buffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 0, 1, &m_DescriptorSets[m_CurrentFrameToRender], 0, nullptr);
    
    vkCmdDrawIndexed(cmd, (u32)m_Indices.size(), 1, 0, 0, 0);

    vkCmdEndRenderPass(cmd);

    VulkanCheck(vkEndCommandBuffer(cmd), "Failed to record command buffer");
}

void Application::CreateSynchronizationPrimitives()
{
    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    VkFenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (auto& frame : m_BufferedFrames)
    {
        VulkanCheck(vkCreateSemaphore(m_Device, &semaphoreCreateInfo, nullptr, &frame.m_ImageAvailableSemaphore), "Failed to create present semaphore");
        VulkanCheck(vkCreateSemaphore(m_Device, &semaphoreCreateInfo, nullptr, &frame.m_ImageRenderedSemaphore), "Failed to create render semaphore");
        VulkanCheck(vkCreateFence(m_Device, &fenceCreateInfo, nullptr, &frame.m_ImageAvailableFence), "Failed to create render fence");
    }
}

void Application::RecreateSwapchain()
{
    // handle minimization
    i32 width, height;
    glfwGetFramebufferSize(m_Window, &width, &height);
    while (width == 0 || height == 0)
    {
        glfwGetFramebufferSize(m_Window, &width, &height);
        glfwWaitEvents();
    }
    
    vkDeviceWaitIdle(m_Device);

    CleanUpSwapchain();
    
    CreateSwapchain();
    CreateSwapchainImageViews();
    CreateDepthResources();
    CreateFramebuffers();
}

void Application::CleanUpSwapchain()
{
    vkDestroyImageView(m_Device, m_DepthTexture.View, nullptr);
    vkDestroyImage(m_Device, m_DepthTexture.Texture, nullptr);
    vkFreeMemory(m_Device, m_DepthTexture.TextureMemory, nullptr);
    
    for (auto framebuffer : m_Framebuffers)
        vkDestroyFramebuffer(m_Device, framebuffer, nullptr);
    for (auto imageView : m_SwapchainImageViews)
        vkDestroyImageView(m_Device, imageView, nullptr);
    m_Framebuffers.clear();
    m_SwapchainImageViews.clear();
    vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
}

void Application::MainLoop()
{
    while (!glfwWindowShouldClose(m_Window))
    {
        glfwPollEvents();
        OnDraw();
    }

    vkDeviceWaitIdle(m_Device);
}

void Application::OnDraw()
{
    FrameData& frame = m_BufferedFrames[m_CurrentFrameToRender];
    vkWaitForFences(m_Device, 1, &frame.m_ImageAvailableFence, VK_TRUE, std::numeric_limits<u64>::max());

    u32 imageIndex;
    VkResult nextImageRes = vkAcquireNextImageKHR(m_Device, m_Swapchain, std::numeric_limits<u64>::max(), frame.m_ImageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
    if (nextImageRes == VK_ERROR_OUT_OF_DATE_KHR)
    {
        RecreateSwapchain();
        return;
    }
    
    vkResetFences(m_Device, 1, &frame.m_ImageAvailableFence);

    vkResetCommandBuffer(frame.m_CommandBuffer, 0);
    
    RecordCommandBuffer(frame.m_CommandBuffer, imageIndex);

    OnUpdate();

    std::array submitSemaphores = { frame.m_ImageAvailableSemaphore };
    // wait only on output stage, that means that theoretically the implementation can
    // already start executing our vertex shader and such while the image is not yet available
    std::array<VkPipelineStageFlags, 1> submitStages = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    std::array presentSemaphores = { frame.m_ImageRenderedSemaphore };
    
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &frame.m_CommandBuffer;
    submitInfo.waitSemaphoreCount = (u32)submitSemaphores.size();
    submitInfo.pWaitSemaphores = submitSemaphores.data();
    submitInfo.pWaitDstStageMask = submitStages.data();
    submitInfo.signalSemaphoreCount = (u32)presentSemaphores.size();
    submitInfo.pSignalSemaphores = presentSemaphores.data();

    VulkanCheck(vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, frame.m_ImageAvailableFence), "Failed to submit draw command buffer");
    
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_Swapchain;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.waitSemaphoreCount = (u32)presentSemaphores.size();
    presentInfo.pWaitSemaphores = presentSemaphores.data();

    VkResult presentRes = vkQueuePresentKHR(m_PresentationQueue, &presentInfo);
    if (presentRes == VK_ERROR_OUT_OF_DATE_KHR || presentRes == VK_SUBOPTIMAL_KHR || m_WindowResized)
    {
        RecreateSwapchain();
        m_WindowResized = false;
    }

    m_CurrentFrameToRender = (m_CurrentFrameToRender + 1) % BUFFERED_FRAMES_COUNT;
}

void Application::OnUpdate()
{
    // update uniform buffer
    f32 time = (f32)glfwGetTime();

    TransformUBO transformUbo = {};
    transformUbo.Model = glm::rotate(glm::mat4(1.0), time * 0.1f, glm::vec3(0.0f, 0.0f, 1.0f));
    transformUbo.View = glm::lookAt(glm::vec3(0.0f, 0.0f, 3.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    transformUbo.Projection = glm::perspective(glm::radians(45.0f), (f32)m_SwapchainExtent.width / (f32)m_SwapchainExtent.height, 1e-3f, 100.0f);
    // opengl to vulkan projection
    transformUbo.Projection[1][1] *= -1.0f;

    memcpy(m_UniformBuffersMapped[m_CurrentFrameToRender], &transformUbo, sizeof transformUbo);
}

void Application::CleanUp()
{
    for (auto& frame : m_BufferedFrames)
    {
        vkDestroySemaphore(m_Device, frame.m_ImageAvailableSemaphore, nullptr);
        vkDestroySemaphore(m_Device, frame.m_ImageRenderedSemaphore, nullptr);
        vkDestroyFence(m_Device, frame.m_ImageAvailableFence, nullptr);
    }
    
    vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);

    CleanUpSwapchain();
    
    vkDestroySampler(m_Device, m_TextureImageSampler, nullptr);
    vkDestroyImageView(m_Device, m_TextureImage.View, nullptr);
    vkDestroyImage(m_Device, m_TextureImage.Texture, nullptr);
    vkFreeMemory(m_Device, m_TextureImage.TextureMemory, nullptr);
    
    for (auto& uniformBuffer : m_UniformBuffers)
    {
        vkDestroyBuffer(m_Device, uniformBuffer.Buffer, nullptr);
        vkFreeMemory(m_Device, uniformBuffer.BufferMemory, nullptr);
    }
    vkDestroyDescriptorPool(m_Device, m_DescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(m_Device, m_DescriptorSetLayout, nullptr);
    vkDestroyBuffer(m_Device, m_VertexBuffer.Buffer, nullptr);
    vkFreeMemory(m_Device, m_VertexBuffer.BufferMemory, nullptr);
    vkDestroyBuffer(m_Device, m_IndexBuffer.Buffer, nullptr);
    vkFreeMemory(m_Device, m_IndexBuffer.BufferMemory, nullptr);

    vkDestroyPipeline(m_Device, m_Pipeline, nullptr);
    vkDestroyPipelineLayout(m_Device, m_PipelineLayout, nullptr);
    vkDestroyRenderPass(m_Device, m_RenderPass, nullptr);

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

    VkPhysicalDeviceFeatures features = {};
    vkGetPhysicalDeviceFeatures(device, &features);
    
    // no need to check for `extensionsSupported` since if it's false than `swapchainIsSufficient` is also false
    return queueFamilyIndices.IsComplete() && swapchainIsSufficient && features.samplerAnisotropy;
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
    createInfo.codeSize = spirv.size() * sizeof(u32);
    createInfo.pCode = spirv.data();

    VkShaderModule shaderModule;
    
    VulkanCheck(vkCreateShaderModule(m_Device, &createInfo, nullptr, &shaderModule), "Failed to create shader module");
    return shaderModule;
}

BufferData Application::CreateBuffer(const BufferCreateData& bufferCreateData)
{
    BufferData bufferData = {};
    
    VkBufferCreateInfo bufferCreateInfo = {};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.usage = bufferCreateData.Usage;
    bufferCreateInfo.size = bufferCreateData.SizeBytes;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VulkanCheck(vkCreateBuffer(m_Device, &bufferCreateInfo, nullptr, &bufferData.Buffer), "Failed to create buffer");

    VkMemoryRequirements memoryRequirements = {};
    vkGetBufferMemoryRequirements(m_Device, bufferData.Buffer, &memoryRequirements);

    VkMemoryAllocateInfo allocateInfo = {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = memoryRequirements.size;
    allocateInfo.memoryTypeIndex = FindMemoryType(memoryRequirements.memoryTypeBits, bufferCreateData.Properties);
    VulkanCheck(vkAllocateMemory(m_Device, &allocateInfo, nullptr, &bufferData.BufferMemory),
        "Failed to allocate vertex buffer memory");

    vkBindBufferMemory(m_Device, bufferData.Buffer, bufferData.BufferMemory, 0);

    return bufferData;
}

void Application::CopyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize sizeBytes)
{
    VkCommandBuffer cmd = BeginSingleTimeCommands();

    VkBufferCopy copyRegion = {};
    copyRegion.size = sizeBytes;
    vkCmdCopyBuffer(cmd, src, dst, 1, &copyRegion);

    EndSingleTimeCommands(cmd);   
}

u32 Application::FindMemoryType(u32 filter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memoryProperties = {};
    vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &memoryProperties);

    for (u32 i = 0; i < memoryProperties.memoryTypeCount; i++)
    {
        if (filter & (1 << i) && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }

    ASSERT(false, "Failed to find sufficient memory type")
    std::unreachable();
}

TextureData Application::CreateTexture(const TextureCreateData& textureCreateData)
{
    TextureData textureData;
    
    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.extent = {.width = textureCreateData.Width, .height = textureCreateData.Height, .depth = 1};
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.format = textureCreateData.Format;
    imageCreateInfo.tiling = textureCreateData.Tiling;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.usage = textureCreateData.Usage;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    VulkanCheck(vkCreateImage(m_Device, &imageCreateInfo, nullptr, &textureData.Texture),
        "Failed to create texture image");

    VkMemoryRequirements memoryRequirements = {};
    vkGetImageMemoryRequirements(m_Device, textureData.Texture, &memoryRequirements);

    VkMemoryAllocateInfo allocateInfo = {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = memoryRequirements.size;
    allocateInfo.memoryTypeIndex = FindMemoryType(memoryRequirements.memoryTypeBits, textureCreateData.Properties);

    VulkanCheck(vkAllocateMemory(m_Device, &allocateInfo, nullptr, &textureData.TextureMemory),
        "Failed to allocate texture memory");

    vkBindImageMemory(m_Device, textureData.Texture, textureData.TextureMemory, 0);

    return textureData;
}

void Application::TransitionTextureLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
{
    VkCommandBuffer cmd = BeginSingleTimeCommands();

    VkImageMemoryBarrier memoryBarrier = {};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    memoryBarrier.image = image;
    memoryBarrier.oldLayout = oldLayout;
    memoryBarrier.newLayout = newLayout;
    // no queue transitions
    memoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    memoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    memoryBarrier.subresourceRange.baseMipLevel = 0;
    memoryBarrier.subresourceRange.levelCount = 1;
    memoryBarrier.subresourceRange.baseArrayLayer = 0;
    memoryBarrier.subresourceRange.layerCount = 1;

    if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    {
        memoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (HasStencilComponent(format))
            memoryBarrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    else
    {
        memoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }
    
    VkPipelineStageFlags sourceStage, destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        memoryBarrier.srcAccessMask = 0;
        memoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    {
        memoryBarrier.srcAccessMask = 0;
        memoryBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    }
    else
    {
        ASSERT(false, "Invalid layout transition")
        std::unreachable();
    }

    vkCmdPipelineBarrier(
        cmd,
        sourceStage, destinationStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &memoryBarrier);
    
    EndSingleTimeCommands(cmd);
}

void Application::CopyBufferToImage(VkBuffer buffer, VkImage image, u32 width, u32 height)
{
    VkCommandBuffer cmd = BeginSingleTimeCommands();

    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    // tightly packed
    region.bufferImageHeight = 0;
    region.bufferRowLength = 0;
    region.imageExtent = {.width = width, .height = height, .depth = 1};
    region.imageOffset = {0, 0, 0};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.layerCount = 1;
    region.imageSubresource.baseArrayLayer = 0;

    vkCmdCopyBufferToImage(
        cmd,
        buffer, image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // currently used image layout
        1, &region);
    
    EndSingleTimeCommands(cmd);
}

VkImageView Application::CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
{
    VkImageViewCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = image;
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = format;

    // leave color as is
    createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    createInfo.subresourceRange.aspectMask = aspectFlags;
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = 1;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;

    VulkanCheck(vkCreateImageView(m_Device, &createInfo, nullptr, &imageView), "Failed to create image view");

    return imageView;
}

VkCommandBuffer Application::BeginSingleTimeCommands()
{
    VkCommandBufferAllocateInfo allocateInfo = {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandPool = m_CommandPool;
    allocateInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(m_Device, &allocateInfo, &commandBuffer);

    VkCommandBufferBeginInfo bufferBeginInfo = {};
    bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &bufferBeginInfo);

    return commandBuffer;
}

void Application::EndSingleTimeCommands(VkCommandBuffer cmd)
{
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkDeviceWaitIdle(m_Device);

    vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &cmd);   
}

VkFormat Application::GetDepthFormat()
{
    return FindSupportedFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

VkFormat Application::FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling,
                                          VkFormatFeatureFlags features)
{
    for (auto candidate : candidates)
    {
        VkFormatProperties properties = {};
        vkGetPhysicalDeviceFormatProperties(m_PhysicalDevice, candidate, &properties);
        if (tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & features) == features)
            return  candidate;
        if (tiling == VK_IMAGE_TILING_OPTIMAL && (properties.optimalTilingFeatures & features) == features)
            return candidate;
    }
    ASSERT(false, "No supported format found")
    std::unreachable();
}

bool Application::HasStencilComponent(VkFormat format)
{
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}
