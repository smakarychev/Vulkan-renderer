#pragma once

#include "RenderGraph.h"
#include "RGDrawResources.h"

class SceneLight;

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
    Resource ensureResource(Resource resource, Graph& graph, const std::string& name,
        const GraphBufferDescription& fallback);

    DrawAttributeBuffers readDrawAttributes(const Geometry& geometry, Graph& graph, const std::string& baseName,
        ResourceAccessFlags shaderStage);
    DrawAttachmentResources readWriteDrawAttachments(const DrawAttachments& attachments, Graph& graph);
    SceneLightResources readSceneLight(const SceneLight& light, Graph& graph, const std::string& baseName,
        ResourceAccessFlags shaderStage);
    IBLData readIBLData(const IBLData& ibl, Graph& graph, ResourceAccessFlags shaderStage);
    SSAOData readSSAOData(const SSAOData& ssao, Graph& graph, ResourceAccessFlags shaderStage);
    DirectionalShadowData readDirectionalShadowData(const DirectionalShadowData& shadow, Graph& graph,
        ResourceAccessFlags shaderStage);
    CSMData readCSMData(const CSMData& csm, Graph& graph, ResourceAccessFlags shaderStage);

    void updateDrawAttributeBindings(const ShaderDescriptors& descriptors, const Resources& resources,
        const DrawAttributeBuffers& attributeBuffers, DrawFeatures features);

    void updateSceneLightBindings(const ShaderDescriptors& descriptors, const Resources& resources,
        const SceneLightResources& lights);
    void updateIBLBindings(const ShaderDescriptors& descriptors, const Resources& resources, const IBLData& iblData);
    void updateSSAOBindings(const ShaderDescriptors& descriptors, const Resources& resources, const SSAOData& ssaoData);
    void updateShadowBindings(const ShaderDescriptors& descriptors, const Resources& resources,
        const DirectionalShadowData& shadowData);
    void updateCSMBindings(const ShaderDescriptors& descriptors, const Resources& resources,
        const CSMData& csmData);
}
