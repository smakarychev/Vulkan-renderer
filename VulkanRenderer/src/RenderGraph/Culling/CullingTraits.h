#pragma once
#include <string>

// `Single` is primarily used for translucency rendering,
// it assumes that HiZ exists and contains full info about opaque geometry
enum class CullStage {Cull, Reocclusion, Single};

inline std::string cullStageToString(CullStage stage)
{
    switch (stage)
    {
    case CullStage::Cull:   
        return "Cull";
    case CullStage::Reocclusion:
        return "Reocclusion";
    case CullStage::Single:
        return "Single";
    default:
        return "";
    }
}