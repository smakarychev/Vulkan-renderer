#pragma once

#include "RenderGraph.h"

namespace RenderGraph
{
    class Resources;
    struct IBLData;
    struct SSAOData;
}

class ShaderDescriptors;

namespace RenderGraph::RgUtils
{
    Resource ensureResource(Resource resource, Graph& graph, const std::string& name,
        const GraphTextureDescription& fallback);    
    Resource ensureResource(Resource resource, Graph& graph, const std::string& name,
        ImageUtils::DefaultTexture fallback);

    void updateIBLBindings(const ShaderDescriptors& descriptors, const Resources& resources, const IBLData& iblData);
    void updateSSAOBindings(const ShaderDescriptors& descriptors, const Resources& resources, const SSAOData& ssaoData);
}
