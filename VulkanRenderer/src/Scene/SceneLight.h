#pragma once

#include "SceneInfo.h"
#include "Light/Light.h"

#include <CoreLib/Containers/FreeList.h>

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
    u32 Add(CommonLight light);
    void Delete(u32 light);
    
    void SetScene(Scene& scene) { m_Scene = &scene; }

    void SetVisibleLights(const std::vector<u32>& visibleLights) { m_VisibleLights = visibleLights; }

    u32 Count() const { return m_Lights.size(); }
    CommonLight& Get(u32 index) { return m_Lights[index]; }
    const CommonLight& Get(u32 index) const { return m_Lights[index]; }
    const Buffers& GetBuffers() const { return m_Buffers; }
    const std::vector<u32>& VisibleLights() const { return m_VisibleLights; }

    u32 DirectionalLightCount() const { return m_CachedLightsInfo.DirectionalLightCount; }
    u32 PointLightCount() const { return m_CachedLightsInfo.PointLightCount; }
private:
    void OnUpdate(FrameContext& ctx);
    void UpdateDirectionalLight(CommonLight& light, u32 lightIndex, FrameContext& ctx);
    void UpdatePointLight(CommonLight& light, u32 lightIndex, FrameContext& ctx);
private:
    lux::FreeList<CommonLight> m_Lights;
    
    std::vector<u32> m_VisibleLights;
    std::vector<DirectionalLight> m_CachedDirectionalLights;
    std::vector<PointLight> m_CachedPointLights;
    LightsInfo m_CachedLightsInfo{};

    Buffers m_Buffers{};
    
    Scene* m_Scene{nullptr};
};

