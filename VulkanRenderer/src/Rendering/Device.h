#pragma once

#include "types.h"

#include <string_view>
#include <vector>

#include "DriverResourceHandle.h"
#include "Core/core.h"

class QueueInfo
{
    FRIEND_INTERNAL
    friend class Device;
public:
    // technically any family index is possible;
    // practically GPUs have only a few
    static constexpr u32 UNSET_FAMILY = std::numeric_limits<u32>::max();
    u32 Family{UNSET_FAMILY};
private:
    ResourceHandle<QueueInfo> Handle() const { return m_ResourceHandle; }
private:
    ResourceHandle<QueueInfo> m_ResourceHandle;
};

struct DeviceQueues
{
public:
    bool IsComplete() const
    {
        return Graphics.Family != QueueInfo::UNSET_FAMILY &&
            Presentation.Family != QueueInfo::UNSET_FAMILY &&
            Compute.Family != QueueInfo::UNSET_FAMILY;
    }
    std::vector<u32> AsFamilySet() const
    {
        std::vector<u32> familySet{Graphics.Family};
        if (Presentation.Family != Graphics.Family)
            familySet.push_back(Presentation.Family);
        if (Compute.Family != Graphics.Family && Compute.Family != Presentation.Family)
            familySet.push_back(Compute.Family);

        return familySet;
    }
    u32 GetFamilyByKind(QueueKind queueKind) const
    {
        switch (queueKind)
        {
        case QueueKind::Graphics:       return Graphics.Family;
        case QueueKind::Presentation:   return Presentation.Family;
        case QueueKind::Compute:        return Compute.Family;
        default:
            ASSERT(false, "Unrecognized queue kind")
            break;
        }
        std::unreachable();
    }
public:
    QueueInfo Graphics;
    QueueInfo Presentation;
    QueueInfo Compute;
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
            bool AsyncCompute{false};
        };
    public:
        Device Build();
        Builder& SetWindow(GLFWwindow* window);
        Builder& Defaults();
        Builder& AsyncCompute(bool isEnabled = true);
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static Device Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const Device& device);

    const DeviceQueues& GetQueues() const { return m_Queues; }
    void WaitIdle() const;
    
private:
    ResourceHandle<Device> Handle() const { return m_ResourceHandle; }
private:
    DeviceQueues m_Queues;
    GLFWwindow* m_Window{nullptr};
    ResourceHandle<Device> m_ResourceHandle;
};