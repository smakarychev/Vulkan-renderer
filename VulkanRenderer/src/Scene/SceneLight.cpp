#include "rendererpch.h"

#include "SceneLight.h"

#include "FrameContext.h"
#include "ResourceUploader.h"
#include "Scene.h"
#include "ViewInfoGPU.h"
#include "Rendering/Buffer/BufferUtility.h"
#include "Vulkan/Device.h"

#include <AssetLib/Scenes/SceneAsset.h>

Transform3d CommonLight::GetTransform() const
{
    switch (Type)
    {
    case LightType::Directional:
        return Transform3d {
            .Orientation = glm::quatLookAt(PositionDirection, glm::vec3(0.0f, 1.0f, 0.0f)),
        };
    case LightType::Point:
        return Transform3d {
            .Position = PositionDirection,
        };
    case LightType::Spot:
        ASSERT(false, "Spot light is not supported")
        break;
    default:
        ASSERT(false, "Light type is not supported")
        break;
    }
    
    std::unreachable();
}

SceneLight SceneLight::CreateEmpty(DeletionQueue& deletionQueue)
{
    SceneLight light = {};

    light.m_Buffers.DirectionalLights = Device::CreateBuffer({
        .Description = {
            .SizeBytes = sizeof(DirectionalLight),
            .Usage = BufferUsage::Ordinary | BufferUsage::Uniform | BufferUsage::Storage | BufferUsage::Source
        },
    }, deletionQueue);
    light.m_Buffers.PointLights = Device::CreateBuffer({
        .Description = {
            .SizeBytes = sizeof(PointLight),
            .Usage = BufferUsage::Ordinary | BufferUsage::Storage | BufferUsage::Source
        },
    }, deletionQueue);

    return light;
}

void SceneLight::Add(SceneInstance instance)
{
    const SceneLightInfo& lightInfo = instance.m_SceneInfo->m_Lights;
    for (auto& light : lightInfo.Lights)
        m_Lights.push_back(light);
}

void SceneLight::OnUpdate(FrameContext& ctx)
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

    m_CachedLightsInfo = {
        .DirectionalLightCount = directionalLightIndex,
        .PointLightCount = pointLightIndex
    };
}

void SceneLight::UpdateDirectionalLight(CommonLight& light, u32 lightIndex, FrameContext& ctx)
{
    const DirectionalLight directionalLight = {{
        .Direction = light.PositionDirection,
        .Color = light.Color,
        .Intensity = light.Intensity,
        .Radius = light.Radius
    }};
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

void SceneLight::UpdatePointLight(CommonLight& light, u32 lightIndex, FrameContext& ctx)
{
    const PointLight pointLight = {{
        .Position = light.PositionDirection,
        .Color = light.Color,
        .Intensity = light.Intensity,
        .Radius = light.Radius
    }};
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
