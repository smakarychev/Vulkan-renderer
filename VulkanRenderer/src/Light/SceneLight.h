#pragma once
#include "Light.h"
#include "Rendering/Buffer.h"

class ResourceUploader;

class SceneLight
{
    struct Buffers
    {
        Buffer DirectionalLight{};
    };
public:
    SceneLight();

    void SetDirectionalLight(const DirectionalLight& light);
    const DirectionalLight& GetDirectionalLight() const { return m_DirectionalLight; }

    void UpdateBuffers(ResourceUploader& resourceUploader);
    const Buffers& GetBuffers() const { return m_Buffers; }
private:
    void Initialize();
private:
    DirectionalLight m_DirectionalLight{};

    bool m_IsDirty{true};
    bool m_IsInitialized{false};
    
    Buffers m_Buffers{};
};
