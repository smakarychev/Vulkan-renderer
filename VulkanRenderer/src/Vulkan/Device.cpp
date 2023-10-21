#include "Device.h"

#include "Driver.h"
#include "VulkanUtils.h"
#include "GLFW/glfw3.h"
#include "utils/utils.h"

Device Device::Builder::Build()
{
    ASSERT(m_CreateInfo.Window, "Window is unset")
    Device device = Device::Create(m_CreateInfo);
    Driver::DeletionQueue().AddDeleter([device](){ Device::Destroy(device); });

    return device;
}

Device::Builder& Device::Builder::SetWindow(GLFWwindow* window)
{
    m_CreateInfo.Window = window;
    return *this;
}

Device::Builder& Device::Builder::Defaults()
{
    CreateInfo defaults = {};
    defaults.AppName = "Vulkan-app";
    defaults.APIVersion = VK_API_VERSION_1_3;
    u32 instanceExtensionsCount = 0;
    
    const char** instanceExtensions = glfwGetRequiredInstanceExtensions(&instanceExtensionsCount);
    defaults.InstanceExtensions = std::vector(instanceExtensions + 0, instanceExtensions + instanceExtensionsCount);
    defaults.InstanceExtensions.emplace_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    
#ifdef VULKAN_VAL_LAYERS
    defaults.InstanceExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

#ifdef VULKAN_VAL_LAYERS
    defaults.InstanceValidationLayers = {
        "VK_LAYER_KHRONOS_validation",
    };
#endif
    
    defaults.DeviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
        VK_KHR_MAINTENANCE3_EXTENSION_NAME,
        VK_KHR_MAINTENANCE1_EXTENSION_NAME
    };
    m_CreateInfo = defaults;
    return *this;
}

Device Device::Create(const Builder::CreateInfo& createInfo)
{
    Device device = {};
    device.CreateInstance(createInfo);
    device.CreateSurface(createInfo);
    device.ChooseGPU(createInfo);
    device.CreateDevice(createInfo);
    device.RetrieveDeviceQueues();
    return device;
}

void Device::Destroy(const Device& device)
{
    vkDestroyDevice(device.m_Device, nullptr);
    vkDestroySurfaceKHR(device.m_Instance, device.m_Surface, nullptr);
    vkDestroyInstance(device.m_Instance, nullptr);
}

SurfaceDetails Device::GetSurfaceDetails() const
{
    return vkUtils::getSurfaceDetails(m_GPU, m_Surface);
}

void Device::CreateInstance(const CreateInfo& createInfo)
{
    VkApplicationInfo applicationInfo = {};
    applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pApplicationName = createInfo.AppName.data();
    applicationInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    applicationInfo.pEngineName = "No engine";
    applicationInfo.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    applicationInfo.apiVersion = createInfo.APIVersion;

    VkInstanceCreateInfo instanceCreateInfo = {};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &applicationInfo;
    bool isEveryExtensionSupported = CheckInstanceExtensions(createInfo);
    instanceCreateInfo.enabledExtensionCount = (u32)createInfo.InstanceExtensions.size();
    instanceCreateInfo.ppEnabledExtensionNames = createInfo.InstanceExtensions.data();
#ifdef VULKAN_VAL_LAYERS
    bool isEveryValidationLayerSupported = CheckInstanceValidationLayers(createInfo);
    instanceCreateInfo.enabledLayerCount = (u32)createInfo.InstanceValidationLayers.size();
    instanceCreateInfo.ppEnabledLayerNames = createInfo.InstanceValidationLayers.data();
#else
    bool isEveryValidationLayerSupported = true;
    instanceCreateInfo.enabledLayerCount = 0;
#endif
    ASSERT(isEveryExtensionSupported && isEveryValidationLayerSupported,
        "Failed to create instance")
    VulkanCheck(vkCreateInstance(&instanceCreateInfo, nullptr, &m_Instance),
        "Failed to create instance\n");
}

void Device::CreateSurface(const CreateInfo& createInfo)
{
    ASSERT(createInfo.Window != nullptr, "Window pointer is unset")
    m_Window = createInfo.Window;
    VulkanCheck(glfwCreateWindowSurface(m_Instance, createInfo.Window, nullptr, &m_Surface),
        "Failed to create surface\n");
}

void Device::ChooseGPU(const CreateInfo& createInfo)
{
    u32 availableGPUCount = 0;
    vkEnumeratePhysicalDevices(m_Instance, &availableGPUCount, nullptr);
    std::vector<VkPhysicalDevice> availableGPUs(availableGPUCount);
    vkEnumeratePhysicalDevices(m_Instance, &availableGPUCount, availableGPUs.data());

    for (auto candidate : availableGPUs)
    {
        if (IsGPUSuitable(candidate, createInfo))
        {
            m_GPU = candidate;
            m_Queues = FindQueueFamilies(candidate);
            break;
        }
    }
    
    ASSERT(m_GPU != VK_NULL_HANDLE, "Failed to find suitable gpu device")

    m_GPUDescriptorIndexingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES;
    VkPhysicalDeviceProperties2 deviceProperties2 = {};
    deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    deviceProperties2.pNext = &m_GPUDescriptorIndexingProperties;
    vkGetPhysicalDeviceProperties2(m_GPU, &deviceProperties2);
    m_GPUProperties = deviceProperties2.properties;
}

void Device::CreateDevice(const CreateInfo& createInfo)
{
    std::vector<u32> queueFamilies = m_Queues.AsFamilySet();
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos(queueFamilies.size());
    f32 queuePriority = 1.0f;
    for (u32 i = 0; i < queueFamilies.size(); i++)
    {
        queueCreateInfos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfos[i].queueFamilyIndex = queueFamilies[i];
        queueCreateInfos[i].queueCount = 1;
        queueCreateInfos[i].pQueuePriorities = &queuePriority; 
    }

    VkPhysicalDeviceVulkan11Features vulkan11Features = {};
    vulkan11Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    vulkan11Features.shaderDrawParameters = VK_TRUE;
    VkPhysicalDeviceFeatures deviceFeatures = {};
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    deviceFeatures.multiDrawIndirect = VK_TRUE;
    deviceFeatures.shaderSampledImageArrayDynamicIndexing = VK_TRUE;

    VkPhysicalDeviceVulkan12Features vulkan12Features = {};
    vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vulkan12Features.pNext = &vulkan11Features;
    vulkan12Features.descriptorIndexing = VK_TRUE;
    vulkan12Features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    vulkan12Features.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
    vulkan12Features.descriptorBindingPartiallyBound = VK_TRUE;
    vulkan12Features.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;
    vulkan12Features.descriptorBindingVariableDescriptorCount = VK_TRUE;
    vulkan12Features.runtimeDescriptorArray = VK_TRUE;
    vulkan12Features.samplerFilterMinmax = VK_TRUE;
    
    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = &vulkan12Features;
    deviceCreateInfo.queueCreateInfoCount = (u32)queueCreateInfos.size();
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceCreateInfo.enabledExtensionCount = (u32)createInfo.DeviceExtensions.size();
    deviceCreateInfo.ppEnabledExtensionNames = createInfo.DeviceExtensions.data();
    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

    VulkanCheck(vkCreateDevice(m_GPU, &deviceCreateInfo, nullptr, &m_Device),
        "Failed to create device\n");
}

void Device::RetrieveDeviceQueues()
{
    vkGetDeviceQueue(m_Device, m_Queues.Graphics.Family, 0, &m_Queues.Graphics.Queue);
    vkGetDeviceQueue(m_Device, m_Queues.Presentation.Family, 0, &m_Queues.Presentation.Queue);
    vkGetDeviceQueue(m_Device, m_Queues.Compute.Family, 0, &m_Queues.Compute.Queue);
}

bool Device::IsGPUSuitable(VkPhysicalDevice gpu, const CreateInfo& createInfo)
{
    DeviceQueues deviceQueues = FindQueueFamilies(gpu);
    if (!deviceQueues.IsComplete())
        return false;

    bool isEveryExtensionSupported = CheckGPUExtensions(gpu, createInfo);
    if (!isEveryExtensionSupported)
        return false;
    
    SurfaceDetails surfaceDetails = GetSurfaceDetails(gpu);
    if (!surfaceDetails.IsSufficient())
        return false;

    bool isEveryFeatureSupported = CheckGPUFeatures(gpu);
    if (!isEveryFeatureSupported)
        return false;
    
    return true;
}

DeviceQueues Device::FindQueueFamilies(VkPhysicalDevice gpu) const
{
    u32 queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, queueFamilies.data());
    
    DeviceQueues queues = {};
    
    for (u32 i = 0; i < queueFamilyCount; i++)
    {
        const VkQueueFamilyProperties& queueFamily = queueFamilies[i];

        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            queues.Graphics.Family = i;

        if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)
            queues.Compute.Family = i;
        
        VkBool32 isPresentationSupported = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(gpu, i, m_Surface, &isPresentationSupported);
        if (isPresentationSupported)
            queues.Presentation.Family = i;

        if (queues.IsComplete())
            break;
    }

    return queues;
}

SurfaceDetails Device::GetSurfaceDetails(VkPhysicalDevice gpu) const
{
    return vkUtils::getSurfaceDetails(gpu, m_Surface);
}

bool Device::CheckGPUFeatures(VkPhysicalDevice gpu) const
{
    VkPhysicalDeviceShaderDrawParametersFeatures shaderFeatures = {};
    shaderFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETER_FEATURES;
    
    VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures = {};
    descriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
    descriptorIndexingFeatures.pNext = &shaderFeatures;

    VkPhysicalDeviceVulkan12Features deviceVulkan12Features = {};
    deviceVulkan12Features.sType  = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    deviceVulkan12Features.pNext = &descriptorIndexingFeatures;
    
    VkPhysicalDeviceFeatures2 features = {};
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features.pNext = &deviceVulkan12Features;
    
    vkGetPhysicalDeviceFeatures2(gpu, &features);
    
    return features.features.samplerAnisotropy == VK_TRUE &&
        features.features.multiDrawIndirect == VK_TRUE &&
        features.features.shaderSampledImageArrayDynamicIndexing == VK_TRUE &&
        descriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing == VK_TRUE &&
        descriptorIndexingFeatures.descriptorBindingSampledImageUpdateAfterBind == VK_TRUE &&
        descriptorIndexingFeatures.descriptorBindingPartiallyBound == VK_TRUE &&
        descriptorIndexingFeatures.descriptorBindingUpdateUnusedWhilePending == VK_TRUE &&
        descriptorIndexingFeatures.descriptorBindingVariableDescriptorCount == VK_TRUE &&
        descriptorIndexingFeatures.runtimeDescriptorArray == VK_TRUE &&
        shaderFeatures.shaderDrawParameters == VK_TRUE &&
        deviceVulkan12Features.samplerFilterMinmax == VK_TRUE;
}

bool Device::CheckInstanceExtensions(const CreateInfo& createInfo) const
{
    u32 availableExtensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(availableExtensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, availableExtensions.data());

    return utils::checkArrayContainsSubArray(createInfo.InstanceExtensions, availableExtensions,
        [](const char* req, const VkExtensionProperties& avail) { return std::strcmp(req, avail.extensionName); },
        [this](const char* req) { LOG("Unsupported instance extension: {}\n", req); });
}

bool Device::CheckInstanceValidationLayers(const CreateInfo& createInfo) const
{
    u32 availableValidationLayerCount = 0;
    vkEnumerateInstanceLayerProperties(&availableValidationLayerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(availableValidationLayerCount);
    vkEnumerateInstanceLayerProperties(&availableValidationLayerCount, availableLayers.data());

    return utils::checkArrayContainsSubArray(createInfo.InstanceValidationLayers, availableLayers,
        [](const char* req, const VkLayerProperties& avail) { return std::strcmp(req, avail.layerName); },
        [this](const char* req) { LOG("Unsupported validation layer: {}\n", req); });
}

bool Device::CheckGPUExtensions(VkPhysicalDevice gpu, const CreateInfo& createInfo) const
{
    u32 availableExtensionCount = 0;
    vkEnumerateDeviceExtensionProperties(gpu, nullptr, &availableExtensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(availableExtensionCount);
    vkEnumerateDeviceExtensionProperties(gpu, nullptr, &availableExtensionCount, availableExtensions.data());

    return utils::checkArrayContainsSubArray(createInfo.DeviceExtensions, availableExtensions,
        [](const char* req, const VkExtensionProperties& avail) { return std::strcmp(req, avail.extensionName); },
        [this](const char* req) { LOG("Unsupported device extension: {}\n", req); });
}
