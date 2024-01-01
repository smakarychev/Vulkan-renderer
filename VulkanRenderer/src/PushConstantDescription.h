#pragma once

#include "Vulkan/VulkanCommon.h"

class PushConstantDescription
{
    FRIEND_INTERNAL
public:
    class Builder
    {
        friend class PushConstantDescription;
        struct CreateInfo
        {
            VkShaderStageFlags Stages;
            u32 SizeBytes;
            u32 Offset{0};
        };
    public:
        PushConstantDescription Build();
        Builder& SetSizeBytes(u32 sizeBytes);
        Builder& SetOffset(u32 offset);
        Builder& SetStages(VkShaderStageFlags stages);
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static PushConstantDescription Create(const Builder::CreateInfo& createInfo);

    u32 GetSizeBytes() const { return m_SizeBytes; }
private:
    u32 m_SizeBytes{};
    u32 m_Offset{};
    VkShaderStageFlags m_StageFlags{};
};
