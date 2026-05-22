#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::ComputeSkinning
{
struct ExecutionInfo
{
    RG::Resource RenderObjects{};
    RG::Resource Meshlets{};
    RG::Resource Skins{};
    RG::Resource RenderObjectSkinnedInfos{};
    RG::Resource Ugb{};
    RG::Resource JointMatrices{};
    u32 SkinnedRenderObjectCount{0};
    u32 SkinnedMeshletCount{0};
};

struct PassData
{
    RG::Resource RenderObjects{};
    RG::Resource Ugb{};
    RG::Resource Meshlets{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
