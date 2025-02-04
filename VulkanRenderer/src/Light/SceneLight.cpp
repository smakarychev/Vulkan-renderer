#include "SceneLight.h"

#include <numeric>

#include "FrameContext.h"
#include "ResourceUploader.h"
#include "Rendering/Commands/RenderCommands.h"

SceneLight::SceneLight()
{
    m_DirectionalLight.Intensity = 0.0f;
}

SceneLight::~SceneLight()
{
    if (m_BufferedPointLightCount != 0)
        Device::Destroy(m_Buffers.PointLights);
    if (m_BufferedVisiblePointLightCount != 0)
        Device::Destroy(m_Buffers.VisiblePointLights);
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
    ASSERT(index < m_PointLights.size(), "No point light at index {}", index)
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
    
    ctx.ResourceUploader->SubmitUpload(ctx);
    ctx.CommandList.WaitOnBarrier({
        .DependencyInfo = Device::CreateDependencyInfo({
            .MemoryDependencyInfo = MemoryDependencyInfo{
                .SourceStage = PipelineStage::AllTransfer,
                .DestinationStage = PipelineStage::AllCommands,
                .SourceAccess = PipelineAccess::WriteAll,
                .DestinationAccess = PipelineAccess::ReadAll}},
            ctx.DeletionQueue)});
    
    m_IsDirty = false;
    m_IsVisiblePointLightsDirty = false;
    m_DirtyPointLights.clear();
}

void SceneLight::Initialize()
{
    m_IsInitialized = true;
    
    m_Buffers.DirectionalLight = Device::CreateBuffer({
        .SizeBytes = sizeof(DirectionalLight),
        .Usage = BufferUsage::Ordinary | BufferUsage::Uniform});
    
    m_Buffers.PointLights = Device::CreateBuffer({
        .SizeBytes = sizeof(PointLight),
        .Usage = BufferUsage::Ordinary | BufferUsage::Storage | BufferUsage::Source | BufferUsage::Mappable,
        .PersistentMapping = true},
        Device::DummyDeletionQueue());
    m_Buffers.VisiblePointLights = Device::CreateBuffer({
        .SizeBytes = sizeof(PointLight),
        .Usage = BufferUsage::Ordinary | BufferUsage::Storage | BufferUsage::Source | BufferUsage::Mappable,
        .PersistentMapping = true},
        Device::DummyDeletionQueue());

    m_Buffers.LightsInfo = Device::CreateBuffer({
            .SizeBytes = sizeof(LightsInfo),
            .Usage = BufferUsage::Ordinary | BufferUsage::Uniform});
}

void SceneLight::ResizePointLightsBuffer(FrameContext& ctx)
{
    m_BufferedPointLightCount = (u32)m_PointLights.size();

    if ((u32)m_PointLights.size() <= Device::GetBufferSizeBytes(m_Buffers.PointLights) / sizeof(PointLight))
        return;

    static constexpr bool COPY_OLD = true;
    Device::ResizeBuffer(m_Buffers.PointLights, sizeof(PointLight) * m_BufferedPointLightCount,
        ctx.CommandList, COPY_OLD);
}

void SceneLight::ResizeVisiblePointLightsBuffer(FrameContext& ctx)
{
    m_BufferedVisiblePointLightCount = (u32)m_VisiblePointLights.size();

    if ((u32)m_VisiblePointLights.size() <=
        Device::GetBufferSizeBytes(m_Buffers.VisiblePointLights) / sizeof(PointLight))
        return;

    static constexpr bool COPY_OLD = false;
    Device::ResizeBuffer(m_Buffers.VisiblePointLights, sizeof(PointLight) * m_BufferedVisiblePointLightCount,
        ctx.CommandList, COPY_OLD);
}
