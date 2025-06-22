#include "ShaderOverrides.h"

#include "ShaderPipelineTemplate.h"

PipelineSpecializationsView ShaderSpecializationsView::ToPipelineSpecializationsView(
    const ShaderPipelineTemplate& shaderTemplate)
{
    for (u32 i = 0; i < Descriptions.size(); i++)
    {
        auto spec = std::ranges::find(shaderTemplate.GetReflection().SpecializationConstants(), Names[i].AsStringView(),
            [](auto& constant) { return constant.Name; });
        ASSERT(spec != shaderTemplate.GetReflection().SpecializationConstants().end(),
            "Unrecognized specialization name")
        Descriptions[i].Id = spec->Id;
        Descriptions[i].ShaderStages = spec->ShaderStages;
    }

    return PipelineSpecializationsView(Data, Descriptions);
}