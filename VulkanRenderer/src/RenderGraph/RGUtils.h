#pragma once

#include "RGGraph.h"
#include "RGDrawResources.h"

class SceneLight;

namespace RG
{
    struct IBLData;
    struct SSAOData;
}

namespace RG::RgUtils
{
    Resource ensureResource(Resource resource, Graph& graph, StringId name, const RGImageDescription& fallback);    
    Resource ensureResource(Resource resource, Graph& graph, StringId name, const RGBufferDescription& fallback);

    DrawAttachmentResources readWriteDrawAttachments(DrawAttachments& attachments, Graph& graph);
    DrawAttachmentResources readWriteDrawAttachments(const DrawAttachments& attachments, Graph& graph);

    SceneLightResources readSceneLight(const SceneLight& light, Graph& graph, ResourceAccessFlags shaderStage);
    
    IBLData readIBLData(const IBLData& ibl, Graph& graph, ResourceAccessFlags shaderStage);
    SSAOData readSSAOData(const SSAOData& ssao, Graph& graph, ResourceAccessFlags shaderStage);
    DirectionalShadowData readDirectionalShadowData(const DirectionalShadowData& shadow, Graph& graph,
        ResourceAccessFlags shaderStage);
    CsmData readCsmData(const CsmData& csm, Graph& graph, ResourceAccessFlags shaderStage);

    template <typename BindGroup>
    void updateDrawAttributeBindings(BindGroup& bindGroup, const Graph& graph,
        const DrawAttributeBuffers& attributeBuffers);
    template <typename BindGroup>
    void updateSceneLightBindings(BindGroup& bindGroup, const Graph& graph,
        const SceneLightResources& lights);
    template <typename BindGroup>
    void updateIBLBindings(BindGroup& bindGroup, const Graph& graph, const IBLData& iblData);
    template <typename BindGroup>
    void updateSSAOBindings(BindGroup& bindGroup, const Graph& graph, const SSAOData& ssaoData);
    template <typename BindGroup>
    void updateShadowBindings(BindGroup& bindGroup, const Graph& graph,
        const DirectionalShadowData& shadowData);
    template <typename BindGroup>
    void updateCsmBindings(BindGroup& bindGroup, const Graph& graph,
        const CsmData& csmData);

    template <typename BindGroup>
    void updateDrawAttributeBindings(BindGroup& bindGroup, const Graph& graph,
        const DrawAttributeBuffers& attributeBuffers)
    {
        auto&& [positions, normals, tangents, uvs] = attributeBuffers;
        if constexpr (requires { bindGroup.SetPositions(graph.GetBufferBinding(positions)); })
            bindGroup.SetPositions(graph.GetBufferBinding(positions));

        if constexpr (requires { bindGroup.SetNormals(graph.GetBufferBinding(normals)); })
            bindGroup.SetNormals(graph.GetBufferBinding(normals));

        if constexpr (requires { bindGroup.SetTangents(graph.GetBufferBinding(tangents)); })
            bindGroup.SetTangents(graph.GetBufferBinding(tangents));

        if constexpr (requires { bindGroup.SetUv(graph.GetBufferBinding(uvs)); })
            bindGroup.SetUv(graph.GetBufferBinding(uvs));
    }

    template <typename BindGroup>
    void updateSceneLightBindings(BindGroup& bindGroup, const Graph& graph, const SceneLightResources& lights)
    {
        bindGroup.SetDirectionalLights(graph.GetBufferBinding(lights.DirectionalLights));
        bindGroup.SetPointLights(graph.GetBufferBinding(lights.PointLights));
        bindGroup.SetLightsInfo(graph.GetBufferBinding(lights.LightsInfo));
    }

    template <typename BindGroup>
    void updateIBLBindings(BindGroup& bindGroup, const Graph& graph, const IBLData& iblData)
    {
        // todo: fix me in SHADERS!
        if constexpr (requires { bindGroup.SetIrradianceSH(graph.GetBufferBinding(iblData.IrradianceSH)); })
            bindGroup.SetIrradianceSH(graph.GetBufferBinding(iblData.IrradianceSH));
        bindGroup.SetPrefilterMap(graph.GetImageBinding(iblData.PrefilterEnvironment));
        bindGroup.SetBrdf(graph.GetImageBinding(iblData.BRDF));
    }

    template <typename BindGroup>
    void updateSSAOBindings(BindGroup& bindGroup, const Graph& graph, const SSAOData& ssaoData)
    {
        bindGroup.SetSsaoTexture(graph.GetImageBinding(ssaoData.SSAO));
    }

    template <typename BindGroup>
    void updateShadowBindings(BindGroup& bindGroup, const Graph& graph, const DirectionalShadowData& shadowData)
    {
        bindGroup.SetDirectionalShadowMap(graph.GetImageBinding(shadowData.ShadowMap));
        bindGroup.SetDirectionalShadowTransform(graph.GetBufferBinding(shadowData.Shadow));
    }

    template <typename BindGroup>
    void updateCsmBindings(BindGroup& bindGroup, const Graph& graph, const CsmData& csmData)
    {
        bindGroup.SetCsm(graph.GetImageBinding(csmData.ShadowMap));
        bindGroup.SetCsmData(graph.GetBufferBinding(csmData.CsmInfo));
    }
}
