#pragma once
#include "Light.h"
#include "Rendering/Buffer.h"

struct FrameContext;

class SceneLight
{
    struct Buffers
    {
        Buffer DirectionalLight{};
        Buffer PointLights{};
        Buffer LightsInfo{};
    };
public:
    SceneLight();
    ~SceneLight();

    void SetDirectionalLight(const DirectionalLight& light);
    const DirectionalLight& GetDirectionalLight() const { return m_DirectionalLight; }

    void AddPointLight(const PointLight& light);
    void UpdatePointLight(u32 index, const PointLight& light);
    const std::vector<PointLight>& GetPointLights() const { return m_PointLights; }
    
    void UpdateBuffers(FrameContext& ctx);
    const Buffers& GetBuffers() const { return m_Buffers; }
    u32 GetPointLightCount() const { return m_BufferedPointLightCount; }
private:
    void Initialize();
    void UpdatePointLightsBuffer(FrameContext& ctx);
    bool IsDirty() const { return m_IsDirty || !m_DirtyPointLights.empty(); }
private:
    DirectionalLight m_DirectionalLight{};
    std::vector<PointLight> m_PointLights{};

    bool m_IsDirty{true};
    bool m_IsInitialized{false};

    std::vector<u32> m_DirtyPointLights{};
    
    Buffers m_Buffers{};
    u32 m_BufferedPointLightCount{0};
};
