#include "SceneLight2.h"

#include "FrameContext.h"
#include "ResourceUploader.h"
#include "Scene.h"
#include "Light/Light.h"
#include "Rendering/Buffer/BufferUtility.h"
#include "Vulkan/Device.h"

namespace
{
    LightType lightTypeFromString(const std::string& lightType)
    {
        if (lightType == "directional")
            return LightType::Directional;
        if (lightType == "point")
            return LightType::Point;
        if (lightType == "spot")
            return LightType::Spot;

        std::unreachable();
    }
}

SceneLightInfo SceneLightInfo::FromAsset(assetLib::SceneInfo& sceneInfo)
{
    SceneLightInfo sceneLightInfo = {};
    sceneLightInfo.Lights.reserve(sceneInfo.Scene.lights.size());

    for (auto& light : sceneInfo.Scene.lights)
        sceneLightInfo.Lights.push_back({
            .Type = lightTypeFromString(light.type),
            /* this value is irrelevant, because the transform will be set by SceneHierarchy */
            .PositionDirection = glm::vec3{0.0},
            .Color = *(glm::dvec3*)light.color.data(),
            .Intensity = (f32)light.intensity,
            .Radius = light.range > 0 ? (f32)light.range : std::numeric_limits<f32>::infinity(),
            // todo:
            .SpotLightData = {}});

    return sceneLightInfo;
}

SceneLight2 SceneLight2::CreateEmpty(DeletionQueue& deletionQueue)
{
    SceneLight2 light = {};

    light.m_Buffers.DirectionalLights = Device::CreateBuffer({
        .SizeBytes = sizeof(DirectionalLight),
        .Usage = BufferUsage::Ordinary | BufferUsage::Storage | BufferUsage::Source},
        deletionQueue);
    light.m_Buffers.PointLights = Device::CreateBuffer({
        .SizeBytes = sizeof(PointLight),
        .Usage = BufferUsage::Ordinary | BufferUsage::Storage | BufferUsage::Source},
        deletionQueue);
    light.m_Buffers.LightsInfo = Device::CreateBuffer({
        .SizeBytes = sizeof(LightsInfo),
        .Usage = BufferUsage::Ordinary | BufferUsage::Uniform | BufferUsage::Source},
        deletionQueue);

    return light;
}

void SceneLight2::Add(SceneInstance instance)
{
    const SceneLightInfo& lightInfo = instance.m_SceneInfo->m_Lights;
    for (auto& light : lightInfo.Lights)
        m_Lights.push_back(light);
}

void SceneLight2::OnUpdate(FrameContext& ctx)
{
    u32 directionalLightIndex = 0;
    u32 pointLightIndex = 0;
    for (u32 lightIndex : m_VisibleLights)
    {
        auto& light = m_Lights[lightIndex];
        switch (light.Type)
        {
        case LightType::Directional:
            UpdateDirectionalLight(light, directionalLightIndex, ctx);
            directionalLightIndex++;
            break;
        case LightType::Point:
            UpdatePointLight(light, pointLightIndex, ctx);
            pointLightIndex++;
            break;
        case LightType::Spot:
            ASSERT(false, "Spot light is not supported")
            break;
        }
    }

    const LightsInfo lightsInfo = {
        .DirectionalLightCount = directionalLightIndex,
        .PointLightCount = pointLightIndex};
    if (m_CachedLightsInfo != lightsInfo)
        ctx.ResourceUploader->UpdateBuffer(m_Buffers.LightsInfo, lightsInfo);
    m_CachedLightsInfo = lightsInfo;
}

void SceneLight2::UpdateDirectionalLight(CommonLight& light, u32 lightIndex, FrameContext& ctx)
{
    const DirectionalLight directionalLight = {
        .Direction = light.PositionDirection,
        .Color = light.Color,
        .Intensity = light.Intensity,
        .Size = light.Radius};
    ::Buffers::grow(m_Buffers.DirectionalLights, sizeof(directionalLight) * (lightIndex + 1), ctx.CommandList);
    if (m_CachedDirectionalLights.size() <= lightIndex)
        m_CachedDirectionalLights.resize(lightIndex + 1);
    else if (m_CachedDirectionalLights[lightIndex] == directionalLight)
        return;
    m_CachedDirectionalLights[lightIndex] = directionalLight;
    ctx.ResourceUploader->UpdateBuffer(m_Buffers.DirectionalLights,
        directionalLight,
        lightIndex * sizeof(directionalLight));
}

void SceneLight2::UpdatePointLight(CommonLight& light, u32 lightIndex, FrameContext& ctx)
{
    const PointLight pointLight = {
        .Position = light.PositionDirection,
        .Color = light.Color,
        .Intensity = light.Intensity,
        .Radius = light.Radius};
    ::Buffers::grow(m_Buffers.PointLights, sizeof(pointLight) * (lightIndex + 1), ctx.CommandList);
    if (m_CachedPointLights.size() <= lightIndex)
        m_CachedPointLights.resize(lightIndex + 1);
    else if (m_CachedPointLights[lightIndex] == pointLight)
        return;
    m_CachedPointLights[lightIndex] = pointLight;
    ctx.ResourceUploader->UpdateBuffer(m_Buffers.PointLights,
        pointLight,
        lightIndex * sizeof(pointLight));
}
