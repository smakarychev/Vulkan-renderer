#pragma once

#include "types.h"
#include "DriverResourceHandle.h"

#include <vector>

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
    ResourceHandle<PipelineLayout> m_ResourceHandle;
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
    ResourceHandle<Pipeline> m_ResourceHandle;
};
