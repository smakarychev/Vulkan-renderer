#pragma once
#include "RenderGraph/RenderGraph.h"

class BlitPass
{
public:
    struct PassData
    {
        RG::Resource TextureIn;
        RG::Resource TextureOut;
    };
public:
    BlitPass(std::string_view name);
    void AddToGraph(RG::Graph& renderGraph, RG::Resource textureIn, RG::Resource textureOut,
        const glm::vec3& offset, f32 relativeSize);
    Utils::StringHasher GetNameHash() const { return m_Name.Hash(); }
private:
    RG::Pass* m_Pass{nullptr};

    RG::PassName m_Name;
};
