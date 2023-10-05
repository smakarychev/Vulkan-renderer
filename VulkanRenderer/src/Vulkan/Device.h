#pragma once

#include "types.h"

#include <vulkan/vulkan_core.h>
#include <string_view>
#include <vector>

#include "Core/core.h"
#include "VulkanCommon.h"

struct DeviceQueues
{
public:
    bool IsComplete() const
    {
        return Graphics.Family != QueueInfo::UNSET_FAMILY && Presentation.Family != QueueInfo::UNSET_FAMILY;
    }
    std::vector<u32> AsFamilySet() const
    {
        if (Graphics.Family != Presentation.Family)
            return {Graphics.Family, Presentation.Family};
        return {Graphics.Family};
    }
    u32 GetFamilyByKind(QueueKind queueKind) const
    {
        switch (queueKind)
        {
        case QueueKind::Graphics:       return Graphics.Family;
        case QueueKind::Presentation:   return Presentation.Family;
        default:
            ASSERT(false, "Unrecognized queue kind")
            break;
        }
        std::unreachable();
    }
public:
    QueueInfo Graphics;
    QueueInfo Presentation;
};

struct GLFWwindow;

class Device
{
    FRIEND_INTERNAL
public:
    class Builder
    {
        friend class Device;
        FRIEND_INTERNAL
        struct CreateInfo
        {
            std::string_view AppName;
            u32 APIVersion;
            std::vector<const char*> InstanceExtensions;
            std::vector<const char*> InstanceValidationLayers;
            std::vector<const char*> DeviceExtensions;
            GLFWwindow* Window;
        };
    public:
        Device Build();
        Builder& SetWindow(GLFWwindow* window);
        Builder& Defaults();
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static Device Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const Device& device);

    SurfaceDetails GetSurfaceDetails() const;
    const DeviceQueues& GetQueues() const { return m_Queues; }
    
private:
    using CreateInfo = Builder::CreateInfo;
    void CreateInstance(const CreateInfo& createInfo);
    void CreateSurface(const CreateInfo& createInfo);
    void ChooseGPU(const CreateInfo& createInfo);
    void CreateDevice(const CreateInfo& createInfo);
    void RetrieveDeviceQueues();

    bool IsGPUSuitable(VkPhysicalDevice gpu, const CreateInfo& createInfo);
    DeviceQueues FindQueueFamilies(VkPhysicalDevice gpu) const;
    SurfaceDetails GetSurfaceDetails(VkPhysicalDevice gpu) const;
    bool CheckGPUFeatures(VkPhysicalDevice gpu) const;

    bool CheckInstanceExtensions(const CreateInfo& createInfo) const;
    bool CheckInstanceValidationLayers(const CreateInfo& createInfo) const;
    bool CheckGPUExtensions(VkPhysicalDevice gpu, const CreateInfo& createInfo) const;
private:
    VkInstance m_Instance{VK_NULL_HANDLE};
    VkSurfaceKHR m_Surface{VK_NULL_HANDLE};
    VkPhysicalDevice m_GPU{VK_NULL_HANDLE};
    VkDevice m_Device{VK_NULL_HANDLE};
    DeviceQueues m_Queues;
    GLFWwindow* m_Window{nullptr};

    VkPhysicalDeviceProperties m_GPUProperties;
};