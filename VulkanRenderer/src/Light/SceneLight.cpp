#include "SceneLight.h"

#include <numeric>

#include "FrameContext.h"
#include "ResourceUploader.h"
#include "Vulkan/RenderCommand.h"

namespace
{
    Buffer resizeLightBuffer(u64 newSizeBytes, Buffer& old, FrameContext& ctx, bool copyOld)
    {
        Buffer newBuffer = Buffer::Builder({
                .SizeBytes = std::max(newSizeBytes, old.GetSizeBytes()),
                .Usage = BufferUsage::Ordinary | BufferUsage::Source | BufferUsage::Storage})
            .CreateMapped()
            .BuildManualLifetime();

        ctx.DeletionQueue.Enqueue(old);
        
        if (copyOld)
            RenderCommand::CopyBuffer(ctx.Cmd, old, newBuffer, {
                .SizeBytes = old.GetSizeBytes(),
                .SourceOffset = 0,
                .DestinationOffset = 0});

        return newBuffer;
    }
}

SceneLight::SceneLight()
{
    m_DirectionalLight.Intensity = 0.0f;
}

SceneLight::~SceneLight()
{
    if (m_BufferedPointLightCount != 0)
        Buffer::Destroy(m_Buffers.PointLights);
    if (m_BufferedVisiblePointLightCount != 0)
        Buffer::Destroy(m_Buffers.VisiblePointLights);
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
    
    m_DirtyPointLights.emplace((u32)m_PointLights.size() - 1);
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

    m_DirtyPointLights.emplace(index);
}

void SceneLight::SetVisiblePointLights(const std::vector<PointLight>& lights)
{
    m_VisiblePointLights = lights;
    m_IsVisiblePointLightsDirty = true;
}

void SceneLight::UpdateBuffers(FrameContext& ctx)
{
    if (!m_IsInitialized)
        Initialize();
    
    if (!IsDirty())
        return;

    ctx.ResourceUploader->UpdateBuffer(m_Buffers.DirectionalLight, m_DirectionalLight);

    ResizePointLightsBuffer(ctx);
    for (auto& light : m_DirtyPointLights)
        ctx.ResourceUploader->UpdateBuffer(m_Buffers.PointLights, m_PointLights[light], light * sizeof(PointLight));
    
    ResizeVisiblePointLightsBuffer(ctx);
    for (u32 light = 0; light < m_VisiblePointLights.size(); light++)
        ctx.ResourceUploader->UpdateBuffer(m_Buffers.VisiblePointLights, m_VisiblePointLights[light],
            light * sizeof(PointLight));
    
    LightsInfo info = {
        .PointLightCount = LIGHT_CULLING ? m_BufferedVisiblePointLightCount : m_BufferedPointLightCount};
    ctx.ResourceUploader->UpdateBuffer(m_Buffers.LightsInfo, info);
    
    ctx.ResourceUploader->SubmitUpload(ctx.Cmd);
    
    m_IsDirty = false;
    m_IsVisiblePointLightsDirty = false;
    m_DirtyPointLights.clear();
}

void SceneLight::Initialize()
{
    m_IsInitialized = true;
    
    m_Buffers.DirectionalLight = Buffer::Builder({
            .SizeBytes = sizeof(DirectionalLight),
            .Usage = BufferUsage::Ordinary | BufferUsage::Uniform})
        .Build();
    m_Buffers.PointLights = Buffer::Builder({
            .SizeBytes = sizeof(PointLight),
            .Usage = BufferUsage::Ordinary | BufferUsage::Storage | BufferUsage::Source})
        .CreateMapped()
        .BuildManualLifetime();
    m_Buffers.VisiblePointLights = Buffer::Builder({
            .SizeBytes = sizeof(PointLight),
            .Usage = BufferUsage::Ordinary | BufferUsage::Storage | BufferUsage::Source})
        .CreateMapped()
        .BuildManualLifetime();

    m_Buffers.LightsInfo = Buffer::Builder({
            .SizeBytes = sizeof(LightsInfo),
            .Usage = BufferUsage::Ordinary | BufferUsage::Uniform})
        .Build();
}

void SceneLight::ResizePointLightsBuffer(FrameContext& ctx)
{
    m_BufferedPointLightCount = (u32)m_PointLights.size();

    if ((u32)m_PointLights.size() <= m_Buffers.PointLights.GetSizeBytes() / sizeof(PointLight))
        return;

    static constexpr bool COPY_OLD = true;
    m_Buffers.PointLights = resizeLightBuffer(sizeof(PointLight) * m_BufferedPointLightCount,
        m_Buffers.PointLights, ctx, COPY_OLD);
}

void SceneLight::ResizeVisiblePointLightsBuffer(FrameContext& ctx)
{
    m_BufferedVisiblePointLightCount = (u32)m_VisiblePointLights.size();

    if ((u32)m_VisiblePointLights.size() <= m_Buffers.VisiblePointLights.GetSizeBytes() / sizeof(PointLight))
        return;

    static constexpr bool COPY_OLD = false;
    m_Buffers.VisiblePointLights = resizeLightBuffer(sizeof(PointLight) * m_BufferedVisiblePointLightCount,
        m_Buffers.VisiblePointLights, ctx, COPY_OLD);
}
