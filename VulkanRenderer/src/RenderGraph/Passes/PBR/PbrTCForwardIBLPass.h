#pragma once

#include "RenderGraph/RenderGraph.h"

#include <memory>

#include "RenderGraph/RGDrawResources.h"

namespace RG
{
    class Geometry;
}

class CullMetaPass;
class HiZPassContext;
class ShaderDescriptors;

struct PbrForwardIBLPassInitInfo
{
    const ShaderDescriptors* MaterialDescriptors{nullptr};
    const RG::Geometry* Geometry{nullptr};
};

struct PbrForwardIBLPassExecutionInfo
{
    glm::uvec2 Resolution{};
    RG::Resource ColorIn{};
    RG::Resource DepthIn{};

    RG::IBLData IBL{};
};

/* Main forward pbr ibl pass
 * 'TC' stands for triangle culling
 * This pass is suitable to draw opaque surfaces only
 */
class PbrTCForwardIBLPass
{
public:
    struct PassData
    {
        RG::Resource ColorOut{};
        RG::Resource DepthOut{};
    };
public:
    PbrTCForwardIBLPass(RG::Graph& renderGraph, const PbrForwardIBLPassInitInfo& info, std::string_view name);
    void AddToGraph(RG::Graph& renderGraph, const PbrForwardIBLPassExecutionInfo& info);
    utils::StringHasher GetNameHash() const { return m_Name.Hash(); }
private:
    RG::PassName m_Name;
    std::shared_ptr<CullMetaPass> m_Pass{};
};
