#pragma once
#include "RenderGraph/RenderGraph.h"

class CopyTexturePass;

class SlimeMoldContext
{
public:
    struct Traits
    {
        f32 MovementSpeed;
        f32 TurningSpeed;
        f32 SensorAngle;
        f32 SensorOffset;
        glm::vec3 Color;
        f32 ContagionThreshold;
        u32 ContagionSteps;
    };
    struct Slime
    {
        glm::vec2 Position;
        f32 Angle;
        u32 TraitsIndex;
        u32 ContagionStepsLeft;
    };
public:
    static SlimeMoldContext RandomIn(const glm::uvec2& bounds, u32 traitCount, u32 slimeCount,
        ResourceUploader& resourceUploader);
    static Traits RandomTrait();
    
    const glm::uvec2& GetBounds() const { return m_Resolution; }
    const std::vector<Traits>& GetTraits() const { return m_Traits; }
    const std::vector<Slime>& GetSlime() const { return m_Slime; }
    std::vector<Traits>& GetTraits() { return m_Traits; }
    std::vector<Slime>& GetSlime() { return m_Slime; }

    f32& GetDiffuseRate() { return m_DiffuseRate; }
    f32& GetDecayRate() { return m_DecayRate; }

    Buffer GetTraitsBuffer() const { return m_TraitsBuffer; }
    Buffer GetSlimeBuffer() const { return m_SlimeBuffer; }

    Texture GetSlimeMap() const { return m_SlimeMap; }
private:
    glm::uvec2 m_Resolution{};
    std::vector<Traits> m_Traits;
    std::vector<Slime> m_Slime;
    f32 m_DiffuseRate{10.0f};
    f32 m_DecayRate{0.02f};

    Buffer m_TraitsBuffer;
    Buffer m_SlimeBuffer;
    Texture m_SlimeMap;
};

namespace Passes::SlimeMold
{
    struct PassData
    {
        RG::Resource ColorOut{};
    };
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, SlimeMoldContext& ctx);
}
