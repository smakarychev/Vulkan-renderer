#pragma once

#include "ResourceHandle.h"

#include "Descriptors.h"
#include "DescriptorsTraits.h"
#include "Common/Span.h"

#include <vector>

class DeletionQueue;
struct ShaderModuleSource;
class CommandBuffer;

struct PushConstantDescription
{
    u32 SizeBytes{};
    u32 Offset{};
    ShaderStage StageFlags{};
};

struct PipelineLayoutCreateInfo
{
    Span<const PushConstantDescription> PushConstants;
    Span<const DescriptorsLayout> DescriptorSetLayouts;
};

class PipelineLayout
{
    FRIEND_INTERNAL
private:
    ResourceHandleType<PipelineLayout> Handle() const { return m_ResourceHandle; }
private:
    ResourceHandleType<PipelineLayout> m_ResourceHandle{};
};

struct PipelineSpecializationInfo
{
    struct ShaderSpecialization
    {
        u32 Id;
        u32 SizeBytes;
        u32 Offset;
        ShaderStage ShaderStages;
    };
    std::vector<ShaderSpecialization> ShaderSpecializations;
    std::vector<u8> Buffer;
};

enum class DynamicStates
{
    None = 0,
    Viewport    = BIT(1),
    Scissor     = BIT(2),
    DepthBias   = BIT(3),

    Default     = Viewport | Scissor,
};
CREATE_ENUM_FLAGS_OPERATORS(DynamicStates)

enum class DepthMode {Read, ReadWrite, None};
enum class FaceCullMode {Front, Back, None};

enum class PrimitiveKind {Triangle, Point};


struct VertexInputDescription
{
    struct Binding
    {
        u32 Index;
        u32 StrideBytes;
    };
    struct Attribute
    {
        u32 Index;
        u32 BindingIndex;
        Format Format;
        u32 OffsetBytes;
    };
    std::vector<Binding> Bindings;
    std::vector<Attribute> Attributes;
};

class Pipeline
{
    FRIEND_INTERNAL
public:
    class Builder
    {
        friend class Pipeline;
        FRIEND_INTERNAL
        struct CreateInfo
        {
            PipelineLayout PipelineLayout;
            RenderingDetails RenderingDetails;
            bool IsComputePipeline{false};
            bool UseDescriptorBuffer{false};
            std::vector<ShaderModuleSource> Shaders;
            VertexInputDescription VertexDescription;
            DynamicStates DynamicStates{DynamicStates::Default};
            bool ClampDepth{false};
            DepthMode DepthMode{DepthMode::ReadWrite};
            FaceCullMode CullMode{FaceCullMode::None};
            PrimitiveKind PrimitiveKind{PrimitiveKind::Triangle};
            AlphaBlending AlphaBlending{AlphaBlending::Over};
            PipelineSpecializationInfo ShaderSpecialization;
        };
    public:
        Pipeline Build();
        Pipeline Build(DeletionQueue& deletionQueue);
        Pipeline BuildManualLifetime();
        Builder& SetLayout(PipelineLayout layout);
        Builder& SetRenderingDetails(const RenderingDetails& renderingDetails);
        Builder& IsComputePipeline(bool isCompute);
        Builder& AddShader(const ShaderModuleSource& shader);
        Builder& SetVertexDescription(const VertexInputDescription& vertexDescription);
        Builder& DynamicStates(DynamicStates states);
        Builder& ClampDepth(bool enable = true);
        Builder& DepthMode(DepthMode depthMode);
        Builder& FaceCullMode(FaceCullMode cullMode);
        Builder& PrimitiveKind(PrimitiveKind primitiveKind);
        Builder& AlphaBlending(AlphaBlending alphaBlending);
        Builder& UseSpecialization(const PipelineSpecializationInfo& pipelineSpecializationInfo);
        Builder& UseDescriptorBuffer();
    private:
        void PreBuild();
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static Pipeline Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const Pipeline& pipeline);
    void BindGraphics(const CommandBuffer& commandBuffer) const;
    void BindCompute(const CommandBuffer& commandBuffer) const;

    bool operator==(Pipeline other) const { return m_ResourceHandle == other.m_ResourceHandle; }
    bool operator!=(Pipeline other) const { return !(*this == other); }
private:
    ResourceHandleType<Pipeline> Handle() const { return m_ResourceHandle; }
private:
    ResourceHandleType<Pipeline> m_ResourceHandle{};
};
