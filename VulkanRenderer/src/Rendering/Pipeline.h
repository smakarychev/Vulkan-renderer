#pragma once

#include "types.h"
#include "DriverResourceHandle.h"

#include <vector>

#include "DescriptorsTraits.h"

class DeletionQueue;
class ShaderModule;
class DescriptorsLayout;
struct ShaderPushConstantDescription;
class CommandBuffer;

class PipelineLayout
{
    FRIEND_INTERNAL
public:
    class Builder
    {
        friend class PipelineLayout;
        FRIEND_INTERNAL
        struct CreateInfo
        {
            std::vector<ShaderPushConstantDescription> PushConstants;
            std::vector<DescriptorsLayout> DescriptorSetLayouts;
        };
    public:
        PipelineLayout Build();
        Builder& SetPushConstants(const std::vector<ShaderPushConstantDescription>& pushConstants);
        Builder& SetDescriptorLayouts(const std::vector<DescriptorsLayout>& layouts);
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static PipelineLayout Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const PipelineLayout& pipelineLayout);
private:
    ResourceHandle<PipelineLayout> Handle() const { return m_ResourceHandle; }
private:
    ResourceHandle<PipelineLayout> m_ResourceHandle{};
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
            std::vector<ShaderModule> Shaders;
            VertexInputDescription VertexDescription;
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
        Builder& AddShader(const ShaderModule& shaderModule);
        Builder& SetVertexDescription(const VertexInputDescription& vertexDescription);
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
    ResourceHandle<Pipeline> Handle() const { return m_ResourceHandle; }
private:
    ResourceHandle<Pipeline> m_ResourceHandle{};
};
