#pragma once
#include <set>

#include "Light.h"
#include "Rendering/Buffer.h"

struct FrameContext;

class SceneLight
{
    struct Buffers
    {
        Buffer DirectionalLight{};
        Buffer PointLights{};
        Buffer VisiblePointLights{};
        Buffer LightsInfo{};
    };
public:
    SceneLight();
    ~SceneLight();

    void SetDirectionalLight(const DirectionalLight& light);
    const DirectionalLight& GetDirectionalLight() const { return m_DirectionalLight; }

    void AddPointLight(const PointLight& light);
    void UpdatePointLight(u32 index, const PointLight& light);
    void SetVisiblePointLights(const std::vector<PointLight>& lights);
    const std::vector<PointLight>& GetPointLights() const { return m_PointLights; }
    
    void UpdateBuffers(FrameContext& ctx);
    const Buffers& GetBuffers() const { return m_Buffers; }
    u32 GetPointLightCount() const { return m_BufferedPointLightCount; }
    u32 GetVisiblePointLightCount() const { return m_BufferedVisiblePointLightCount; }
private:
    void Initialize();
    void ResizePointLightsBuffer(FrameContext& ctx);
    void ResizeVisiblePointLightsBuffer(FrameContext& ctx);
    bool IsDirty() const { return m_IsDirty || !m_DirtyPointLights.empty() || m_IsVisiblePointLightsDirty; }
private:
    DirectionalLight m_DirectionalLight{};
    std::vector<PointLight> m_PointLights{};
    std::vector<PointLight> m_VisiblePointLights{};

    bool m_IsDirty{true};
    bool m_IsVisiblePointLightsDirty{false};
    bool m_IsInitialized{false};

    std::set<u32> m_DirtyPointLights{};
    
    Buffers m_Buffers{};
    u32 m_BufferedPointLightCount{0};
    u32 m_BufferedVisiblePointLightCount{0};
};
