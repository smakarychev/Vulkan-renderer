#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::ComputeSkinning
{
struct ExecutionInfo
{
    RG::BufferResource RenderObjects{};
    RG::BufferResource Meshlets{};
    RG::BufferResource Skins{};
    RG::BufferResource RenderObjectSkinnedInfos{};
    RG::BufferResource RenderObjectSkinnedInfoIndices{};
    RG::BufferResource Ugb{};
    RG::BufferResource JointMatrices{};
    RG::BufferResource BlendShapes{};
    u32 SkinnedRenderObjectCount{0};
    u32 SkinnedMeshletCount{0};
};

struct PassData
{
    RG::BufferResource RenderObjects{};
    RG::BufferResource Ugb{};
    RG::BufferResource Meshlets{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
