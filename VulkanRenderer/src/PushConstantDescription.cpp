#include "PushConstantDescription.h"

PushConstantDescription PushConstantDescription::Builder::Build()
{
    return PushConstantDescription::Create(m_CreateInfo);
}

PushConstantDescription::Builder& PushConstantDescription::Builder::SetSizeBytes(u32 sizeBytes)
{
    m_CreateInfo.SizeBytes = sizeBytes;

    return *this;
}

PushConstantDescription::Builder& PushConstantDescription::Builder::SetOffset(u32 offset)
{
    m_CreateInfo.Offset = offset;

    return *this;
}

PushConstantDescription::Builder& PushConstantDescription::Builder::SetStages(VkShaderStageFlags stages)
{
    m_CreateInfo.Stages = stages;

    return *this;
}

PushConstantDescription PushConstantDescription::Create(const Builder::CreateInfo& createInfo)
{
    PushConstantDescription pushConstantDescription = {};

    pushConstantDescription.m_SizeBytes = createInfo.SizeBytes;
    pushConstantDescription.m_Offset = createInfo.Offset;
    pushConstantDescription.m_StageFlags = createInfo.Stages;

    return  pushConstantDescription;
}
