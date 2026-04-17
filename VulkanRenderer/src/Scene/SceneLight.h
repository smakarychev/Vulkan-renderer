#pragma once

#include "Assets/Scenes/SceneAsset.h"
#include "Light/Light.h"
#include "Rendering/Buffer/Buffer.h"

#include <CoreLib/Containers/SlotMapType.h>

struct FrameContext;

class SceneLight
{
    friend class Scene;
public:
    struct Buffers
    {
        Buffer DirectionalLights{};
        Buffer PointLights{};
    };
public:
    static SceneLight CreateEmpty(DeletionQueue& deletionQueue);
    u32 Add(lux::CommonLight light);
    void Delete(u32 light);
    
    void SetScene(Scene& scene) { m_Scene = &scene; }

    void SetVisibleLights(const std::vector<lux::CommonLight>& visibleLights) { m_VisibleLights = visibleLights; }

    u32 Count() const { return m_Lights.size(); }
    lux::CommonLight& Get(u32 index) { return m_Lights[index]; }
    const lux::CommonLight& Get(u32 index) const { return m_Lights[index]; }
    const Buffers& GetBuffers() const { return m_Buffers; }
    const std::vector<lux::CommonLight>& VisibleLights() const { return m_VisibleLights; }

    u32 DirectionalLightCount() const { return m_CachedLightsInfo.DirectionalLightCount; }
    u32 PointLightCount() const { return m_CachedLightsInfo.PointLightCount; }
    
    auto begin() const { return m_Lights.begin(); }
    auto end() const { return m_Lights.end(); }
private:
    void OnUpdate(FrameContext& ctx);
    void UpdateDirectionalLight(lux::CommonLight& light, u32 lightIndex, FrameContext& ctx);
    void UpdatePointLight(lux::CommonLight& light, u32 lightIndex, FrameContext& ctx);
private:
    lux::SlotMap<lux::CommonLight> m_Lights;
    
    std::vector<lux::CommonLight> m_VisibleLights;
    std::vector<DirectionalLight> m_CachedDirectionalLights;
    std::vector<PointLight> m_CachedPointLights;
    LightsInfo m_CachedLightsInfo{};

    Buffers m_Buffers{};
    
    Scene* m_Scene{nullptr};
};

