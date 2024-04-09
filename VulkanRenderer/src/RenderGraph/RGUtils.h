#pragma once

#include "RenderGraph.h"
#include "RGDrawResources.h"

namespace RG
{
    class Geometry;
    class Resources;
    struct IBLData;
    struct SSAOData;
}

class ShaderDescriptors;

namespace RG::RgUtils
{
    Resource ensureResource(Resource resource, Graph& graph, const std::string& name,
        const GraphTextureDescription& fallback);    
    Resource ensureResource(Resource resource, Graph& graph, const std::string& name,
        ImageUtils::DefaultTexture fallback);

    DrawAttributeBuffers readDrawAttributes(const Geometry& geometry, Graph& graph, const std::string& baseName,
        ResourceAccessFlags shaderStage);  

    void updateDrawAttributeBindings(const ShaderDescriptors& descriptors, const Resources& resources,
        const DrawAttributeBuffers& attributeBuffers, DrawFeatures features);
    
    void updateIBLBindings(const ShaderDescriptors& descriptors, const Resources& resources, const IBLData& iblData);
    void updateSSAOBindings(const ShaderDescriptors& descriptors, const Resources& resources, const SSAOData& ssaoData);
}
