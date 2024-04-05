#pragma once
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGCommon.h"

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

    const Buffer& GetTraitsBuffer() const { return m_TraitsBuffer; }
    const Buffer& GetSlimeBuffer() const { return m_SlimeBuffer; }

    const Texture& GetSlimeMap() const { return m_SlimeMap; }

    void UpdateTraits(ResourceUploader& resourceUploader);
private:
    glm::uvec2 m_Resolution{};
    std::vector<Traits> m_Traits;
    std::vector<Slime> m_Slime;

    Buffer m_TraitsBuffer;
    Buffer m_SlimeBuffer;
    Texture m_SlimeMap;
};

enum class SlimeMoldPassStage
{
    UpdateSlimeMap, DiffuseSlimeMap, CopyDiffuse, Gradient,
};

class SlimeMoldPass
{
public:
    struct PushConstants
    {
        f32 Width;
        f32 Height;
        u32 SlimeCount;
        f32 Dt;
        f32 Time;
        f32 DiffuseRate{10.0f};
        f32 DecayRate{0.02f};
    };
    struct GradientUBO
    {
        glm::vec4 A{0.5f, 0.5f, 0.5f, 1.0f};
        glm::vec4 B{0.5f, 0.5f, 0.5f, 1.0f};
        glm::vec4 C{1.0f, 1.0f, 1.0f, 1.0f};
        glm::vec4 D{0.0f, 0.1f, 0.2f, 1.0f};
    };
    struct UpdateSlimeMapPassData
    {
        RG::Resource TraitsSsbo;
        RG::Resource SlimeSsbo;
        RG::Resource SlimeMap;
        
        RG::PipelineData* PipelineData{nullptr};
        
        PushConstants* PushConstants{nullptr};
        SlimeMoldContext* SlimeMoldContext{ nullptr };
    };
    struct DiffuseSlimeMapPassData
    {
        RG::Resource SlimeMap;
        RG::Resource DiffuseMap;

        RG::PipelineData* PipelineData{nullptr};
        
        PushConstants* PushConstants{nullptr};
        SlimeMoldContext* SlimeMoldContext{nullptr};
    };
    struct GradientPassData
    {
        RG::Resource DiffuseMap;
        RG::Resource GradientMap;
        RG::Resource GradientUbo;

        RG::PipelineData* PipelineData{nullptr};
        
        PushConstants* PushConstants{nullptr};
        SlimeMoldContext* SlimeMoldContext{ nullptr };
        GradientUBO* Gradient{nullptr};
    };
public:
    SlimeMoldPass(RG::Graph& renderGraph);
    void AddToGraph(RG::Graph& renderGraph, SlimeMoldPassStage stage, SlimeMoldContext& ctx);
private:
    void AddUpdateSlimeMapStage(RG::Graph& renderGraph, SlimeMoldContext& ctx);
    void AddDiffuseSlimeMapStage(RG::Graph& renderGraph, SlimeMoldContext& ctx);
    void AddCopyDiffuseSlimeMapStage(RG::Graph& renderGraph, SlimeMoldContext& ctx);
    void AddGradientStage(RG::Graph& renderGraph, SlimeMoldContext& ctx);
private:
    RG::Pass* m_UpdateSlimeMapPass{nullptr};
    RG::Pass* m_DiffuseSlimeMapPass{nullptr};
    std::shared_ptr<CopyTexturePass> m_CopyDiffuseToMapPass{nullptr};
    RG::Pass* m_GradientSlimeMapPass{nullptr};

    RG::PipelineData m_UpdateSlimeMapPipelineData;
    RG::PipelineData m_DiffuseSlimeMapPipelineData;
    RG::PipelineData m_GradientSlimeMapPipelineData;

    PushConstants m_PushConstants{};
    GradientUBO m_Gradient{};
};
