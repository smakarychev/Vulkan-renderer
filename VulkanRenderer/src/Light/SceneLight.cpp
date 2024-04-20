#include "SceneLight.h"

#include "ResourceUploader.h"

SceneLight::SceneLight()
{
    m_DirectionalLight.Intensity = 0.0f;
}

void SceneLight::SetDirectionalLight(const DirectionalLight& light)
{
    if (m_DirectionalLight.Direction == light.Direction &&
        m_DirectionalLight.Color == light.Color &&
        m_DirectionalLight.Intensity == light.Intensity)
            return;
    
    m_DirectionalLight = light;
    
    m_IsDirty = true;
}

void SceneLight::UpdateBuffers(ResourceUploader& resourceUploader)
{
    if (!m_IsInitialized)
        Initialize();
    
    if (!m_IsDirty)
        return;

    resourceUploader.UpdateBuffer(m_Buffers.DirectionalLight, m_DirectionalLight);
    
    m_IsDirty = false;
}

void SceneLight::Initialize()
{
    m_IsInitialized = true;
    
    m_Buffers.DirectionalLight = Buffer::Builder({
            .SizeBytes = sizeof(DirectionalLight),
            .Usage = BufferUsage::Uniform | BufferUsage::Upload | BufferUsage::DeviceAddress})
        .Build();
}
