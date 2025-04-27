#pragma once

#include "SceneAsset.h"
#include "SceneInstance.h"
#include "Light/Light.h"
#include "Rendering/Buffer/Buffer.h"

#include <vector>
#include <glm/glm.hpp>

struct FrameContext;

enum class LightType : u8
{
    Directional, Point, Spot
};

struct SpotLightData
{
    /* quantized */
    u16 InnerAngle{};
    u16 OuterAngle{};

    auto operator<=>(const SpotLightData&) const = default;
};

struct CommonLight
{
    LightType Type{LightType::Point};
    glm::vec3 PositionDirection{0.0f};
    glm::vec3 Color{1.0f};
    f32 Intensity{1.0f};
    f32 Radius{1.0f};
    SpotLightData SpotLightData{};
};

// todo: remove 2 once ready

struct SceneLightInfo
{
    static SceneLightInfo FromAsset(assetLib::SceneInfo& sceneInfo);
    
    std::vector<CommonLight> Lights;
};

class SceneLight2
{
public:
    struct Buffers
    {
        Buffer DirectionalLights{};
        Buffer PointLights{};
        // todo: this should be removed in favour of one big 'View' UBO
        Buffer LightsInfo{};
    };
public:
    static SceneLight2 CreateEmpty(DeletionQueue& deletionQueue);
    void Add(SceneInstance instance);
    void OnUpdate(FrameContext& ctx);

    void SetVisibleLights(const std::vector<u32>& visibleLights) { m_VisibleLights = visibleLights; }

    u32 Count() const { return (u32)m_Lights.size(); }
    CommonLight& Get(u32 index) { return m_Lights[index]; }
    const CommonLight& Get(u32 index) const { return m_Lights[index]; }
    const Buffers& GetBuffers() const { return m_Buffers; }
    const std::vector<u32>& VisibleLights() const { return m_VisibleLights; }
private:
    void UpdateDirectionalLight(CommonLight& light, u32 lightIndex, FrameContext& ctx);
    void UpdatePointLight(CommonLight& light, u32 lightIndex, FrameContext& ctx);
private:
    std::vector<CommonLight> m_Lights;
    std::vector<u32> m_VisibleLights;
    std::vector<DirectionalLight> m_CachedDirectionalLights;
    std::vector<PointLight> m_CachedPointLights;
    LightsInfo m_CachedLightsInfo{};
    
    Buffers m_Buffers{};
};
