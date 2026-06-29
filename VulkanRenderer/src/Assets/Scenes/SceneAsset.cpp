#include "rendererpch.h"
#include "SceneAsset.h"

#include "Light/Light.h"

namespace lux
{
void SceneLightInfo::SetSunLight(const DirectionalLight& light)
{
    Lights.push_back({
        .Type = LightType::Directional,
        .PositionDirection = light.Direction,
        .Color = light.Color,
        .Intensity = light.Intensity,
        .Radius = light.Radius,
        .IsSun = true,
    });
}

void SceneLightInfo::AddLight(const DirectionalLight& light)
{
    Lights.push_back({
        .Type = LightType::Directional,
        .PositionDirection = light.Direction,
        .Color = light.Color,
        .Intensity = light.Intensity,
        .Radius = light.Radius,
    });
}

void SceneLightInfo::AddLight(const PointLight& light)
{
    Lights.push_back({
        .Type = LightType::Point,
        .PositionDirection = light.Position,
        .Color = light.Color,
        .Intensity = light.Intensity,
        .Radius = light.Radius,
    });
}

void SceneAsset::SetSunLight(const DirectionalLight& light)
{
    const u32 lightIndex = (u32)Lights.Lights.size();
    Lights.SetSunLight(light);

    Hierarchy.Nodes.push_back(SceneHierarchyNode{
        .Type = SceneHierarchyNodeType::Light,
        .Depth = 0,
        .Parent = {SceneHierarchyHandle::INVALID},
        .LocalTransform = Lights.Lights.back().GetTransform(),
        .Payload = {.Light = {lightIndex}}
    });
}

void SceneAsset::AddLight(const DirectionalLight& light)
{
    const u32 lightIndex = (u32)Lights.Lights.size();
    Lights.AddLight(light);

    Hierarchy.Nodes.push_back(SceneHierarchyNode{
        .Type = SceneHierarchyNodeType::Light,
        .Depth = 0,
        .Parent = {SceneHierarchyHandle::INVALID},
        .LocalTransform = Lights.Lights.back().GetTransform(),
        .Payload = {.Light = {lightIndex}}
    });
}

void SceneAsset::AddLight(const PointLight& light)
{
    const u32 lightIndex = (u32)Lights.Lights.size();
    Lights.AddLight(light);

    Hierarchy.Nodes.push_back(SceneHierarchyNode{
        .Type = SceneHierarchyNodeType::Light,
        .Depth = 0,
        .Parent = {SceneHierarchyHandle::INVALID},
        .LocalTransform = Lights.Lights.back().GetTransform(),
        .Payload = {.Light = {lightIndex}}
    });
}

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

SceneHierarchyAnimationChannel::SceneHierarchyAnimationChannel(SceneHierarchyAnimationChannelType type,
    SceneHierarchyAnimationSamplerType samplerType, u32 elementCount)
    : m_Type(type), m_SamplerType(samplerType), m_KeyframeElementCount(elementCount)
{
    m_InterpolatedArray.resize(elementCount);
}

void SceneHierarchyAnimationChannel::Tick(f32 dt)
{
    Interpolate();
    UpdateTimestamp(dt);
}


const SceneHierarchyAnimationChannel::Keyframe& SceneHierarchyAnimationChannel::GetInterpolated() const
{
    return m_InterpolatedArray.front();
}

const SceneHierarchyAnimationChannel::Keyframe& SceneHierarchyAnimationChannel::GetInterpolated(u32 elementIndex) const
{
    return m_InterpolatedArray[elementIndex];
}

void SceneHierarchyAnimationChannel::Interpolate()
{
    if (m_Timestamps.size() < 2)
    {
        for (u32 i = 0; i < m_KeyframeElementCount; i++)
            m_InterpolatedArray[i] = GetKeyframe(i);
        return;
    }
    
    const u32 frame = m_Frame;
    const u32 nextFrame = m_Frame + 1;
    const f32 t = Math::ilerp(m_Timestamps[frame], m_Timestamps[nextFrame], m_Timestamp);
    
    if (t < 0)
    {
        for (u32 i = 0; i < m_KeyframeElementCount; i++)
            m_InterpolatedArray[i] = GetKeyframe(i);
        return;
    }

    switch (m_Type) 
    {
    case SceneHierarchyAnimationChannelType::Translation:
        InterpolateTranslation(t);
        break;
    case SceneHierarchyAnimationChannelType::Orientation:
        InterpolateOrientation(t);
        break;
    case SceneHierarchyAnimationChannelType::Scale:
        InterpolateScale(t);
        break;
    case SceneHierarchyAnimationChannelType::Weight:
        InterpolateWeight(t);
        break;
    }
}

void SceneHierarchyAnimationChannel::InterpolateTranslation(f32 t)
{
    ASSERT(m_KeyframeElementCount == 1)
    
    const glm::vec3 translation = GetKeyframe().Translation;
    const glm::vec3 translationNext = GetNextKeyframe().Translation;

    switch (m_SamplerType)
    {
    case SceneHierarchyAnimationSamplerType::Linear:
        m_InterpolatedArray.front().Translation = glm::mix(translation, translationNext, t);
        break;
    case SceneHierarchyAnimationSamplerType::Step:
        m_InterpolatedArray.front().Translation = translation;
        break;
    case SceneHierarchyAnimationSamplerType::CubicSpline:
        ASSERT(false)
        break;
    }
} 

void SceneHierarchyAnimationChannel::InterpolateOrientation(f32 t)
{
    ASSERT(m_KeyframeElementCount == 1)
    
    const glm::quat orientation = GetKeyframe().Orientation;
    const glm::quat orientationNext = GetNextKeyframe().Orientation;

    switch (m_SamplerType)
    {
    case SceneHierarchyAnimationSamplerType::Linear:
        m_InterpolatedArray.front().Orientation = glm::slerp(orientation, orientationNext, t);
        break;
    case SceneHierarchyAnimationSamplerType::Step:
        m_InterpolatedArray.front().Orientation = orientation;
        break;
    case SceneHierarchyAnimationSamplerType::CubicSpline:
        ASSERT(false)
        break;
    }
}

void SceneHierarchyAnimationChannel::InterpolateScale(f32 t)
{
    ASSERT(m_KeyframeElementCount == 1)
    
    const glm::vec3 scale = GetKeyframe().Scale;
    const glm::vec3 scaleNext = GetNextKeyframe().Scale;
    
    switch (m_SamplerType)
    {
    case SceneHierarchyAnimationSamplerType::Linear:
        m_InterpolatedArray.front().Scale = glm::mix(scale, scaleNext, t);
        break;
    case SceneHierarchyAnimationSamplerType::Step:
        m_InterpolatedArray.front().Scale = scale;
        break;
    case SceneHierarchyAnimationSamplerType::CubicSpline:
        ASSERT(false)
        break;
    }
}

void SceneHierarchyAnimationChannel::InterpolateWeight(f32 t)
{
    for (u32 i = 0; i < m_KeyframeElementCount; i++)
    {
        const f32 weight = GetKeyframe(i).Weight;
        const f32 weightNext = GetNextKeyframe(i).Weight;
    
        switch (m_SamplerType)
        {
        case SceneHierarchyAnimationSamplerType::Linear:
            m_InterpolatedArray[i].Weight = glm::mix(weight, weightNext, t);
            break;
        case SceneHierarchyAnimationSamplerType::Step:
            m_InterpolatedArray[i].Weight = weight;
            break;
        case SceneHierarchyAnimationSamplerType::CubicSpline:
            ASSERT(false)
            break;
        }
    }
}

void SceneHierarchyAnimationChannel::UpdateTimestamp(f32 dt)
{
    if (m_Timestamps.size() < 2)
        return;
    
    m_Timestamp += dt;
    
    if (dt >= 0.0f)
        UpdateTimestampPositive();
    else
        UpdateTimestampNegative();
}

void SceneHierarchyAnimationChannel::UpdateTimestampPositive()
{
    if (m_Timestamp < m_Timestamps[m_Frame + 1])
        return;
        
    m_Frame += 1;
    if (m_Frame >= m_Timestamps.size() - 1)
    {
        m_Frame = 0;
        m_Timestamp = 0;
    }
}

void SceneHierarchyAnimationChannel::UpdateTimestampNegative()
{
    if (m_Timestamp >= m_Timestamps[m_Frame])
        return;
    
    if (m_Frame == 0)
    {
        m_Frame = (u32)m_Timestamps.size() - 2;
        m_Timestamp = m_Timestamps.back();
    }
    else
    {
        m_Frame -= 1;
    }
}

const SceneHierarchyAnimationChannel::Keyframe& SceneHierarchyAnimationChannel::GetKeyframe() const
{
    return GetKeyframe(/*elementIndex*/0);
}

const SceneHierarchyAnimationChannel::Keyframe& SceneHierarchyAnimationChannel::GetNextKeyframe() const
{
    return GetNextKeyframe(/*elementIndex*/0);
}

const SceneHierarchyAnimationChannel::Keyframe& SceneHierarchyAnimationChannel::GetKeyframe(u32 elementIndex) const
{
    return m_Keyframes[(u32)(m_Frame * m_KeyframeElementCount) + elementIndex];
}

const SceneHierarchyAnimationChannel::Keyframe& SceneHierarchyAnimationChannel::GetNextKeyframe(u32 elementIndex) const
{
    return m_Keyframes[(u32)((m_Frame + 1) * m_KeyframeElementCount) + elementIndex];
}
}
