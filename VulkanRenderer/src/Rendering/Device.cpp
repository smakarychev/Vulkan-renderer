#include "Device.h"

#include "Vulkan/Driver.h"
#include "GLFW/glfw3.h"
#include "utils/utils.h"

Device Device::Builder::Build()
{
    ASSERT(m_CreateInfo.Window, "Window is unset")
    Device device = Driver::Create(m_CreateInfo);
    Driver::DeletionQueue().Enqueue(device);

    return device;
}

Device::Builder& Device::Builder::SetWindow(GLFWwindow* window)
{
    m_CreateInfo.Window = window;
    return *this;
}

Device::Builder& Device::Builder::Defaults()
{
    Driver::DeviceBuilderDefaults(m_CreateInfo);
    
    return *this;
}

Device::Builder& Device::Builder::AsyncCompute(bool isEnabled)
{
    m_CreateInfo.AsyncCompute = isEnabled;

    return *this;
}

Device Device::Create(const Builder::CreateInfo& createInfo)
{
    Device device = Driver::Create(createInfo);
    
    return device;
}

void Device::Destroy(const Device& device)
{
    Driver::Destroy(device.Handle());
}

void Device::WaitIdle() const
{
    Driver::WaitIdle();
}

