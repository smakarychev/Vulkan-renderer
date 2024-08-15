#include "SceneLight.h"

#include "FrameContext.h"
#include "ResourceUploader.h"
#include "Vulkan/RenderCommand.h"

SceneLight::SceneLight()
{
    m_DirectionalLight.Intensity = 0.0f;
}

SceneLight::~SceneLight()
{
    if (m_BufferedPointLightCount != 0)
        Buffer::Destroy(m_Buffers.PointLights);
}

void SceneLight::SetDirectionalLight(const DirectionalLight& light)
{
    if (m_DirectionalLight.Direction == light.Direction &&
        m_DirectionalLight.Color == light.Color &&
        m_DirectionalLight.Intensity == light.Intensity &&
        m_DirectionalLight.Size == light.Size)
            return;
    
    m_DirectionalLight = light;
    
    m_IsDirty = true;
}

void SceneLight::AddPointLight(const PointLight& light)
{
    m_PointLights.push_back(light);
    
    m_DirtyPointLights.push_back((u32)m_PointLights.size() - 1);
}

void SceneLight::UpdatePointLight(u32 index, const PointLight& light)
{
    ASSERT(index < m_PointLights.size(), "No point light at index {}", index);
    auto& current = m_PointLights[index];
    if (current.Position == light.Position &&
        current.Color == light.Color &&
        current.Intensity == light.Intensity &&
        current.Radius == light.Radius)
            return;

    current = light;

    m_DirtyPointLights.push_back(index);
}

void SceneLight::UpdateBuffers(FrameContext& ctx)
{
    if (!m_IsInitialized)
        Initialize();
    
    if (!IsDirty())
        return;

    ctx.ResourceUploader->UpdateBuffer(m_Buffers.DirectionalLight, m_DirectionalLight);
    UpdatePointLightsBuffer(ctx);
    for (auto& light : m_DirtyPointLights)
        ctx.ResourceUploader->UpdateBuffer(m_Buffers.PointLights, m_PointLights[light], light * sizeof(PointLight));

    LightsInfo info = {
        .PointLightCount = m_BufferedPointLightCount};
    ctx.ResourceUploader->UpdateBuffer(m_Buffers.LightsInfo, info);
    
    m_IsDirty = false;
    m_DirtyPointLights.clear();
}

void SceneLight::Initialize()
{
    m_IsInitialized = true;
    
    m_Buffers.DirectionalLight = Buffer::Builder({
            .SizeBytes = sizeof(DirectionalLight),
            .Usage = BufferUsage::Uniform | BufferUsage::Upload | BufferUsage::DeviceAddress})
        .Build();

    m_Buffers.LightsInfo = Buffer::Builder({
            .SizeBytes = sizeof(LightsInfo),
            .Usage = BufferUsage::Uniform | BufferUsage::Upload | BufferUsage::DeviceAddress})
        .Build();
}

void SceneLight::UpdatePointLightsBuffer(FrameContext& ctx)
{
    if ((u32)m_PointLights.size() == m_BufferedPointLightCount)
        return;
    
    Buffer newBuffer = Buffer::Builder({
            .SizeBytes = sizeof(PointLight) * (u32)m_PointLights.size(),
            .Usage = BufferUsage::Storage | BufferUsage::Upload | BufferUsage::DeviceAddress})
        .BuildManualLifetime();

    if (m_BufferedPointLightCount != 0)
    {
        ctx.DeletionQueue.Enqueue(m_Buffers.PointLights);
        RenderCommand::CopyBuffer(ctx.Cmd, m_Buffers.PointLights, newBuffer, {
            .SizeBytes = m_Buffers.PointLights.GetSizeBytes()});
    }
    
    m_Buffers.PointLights = newBuffer;
    
    m_BufferedPointLightCount = (u32)m_PointLights.size();
}
