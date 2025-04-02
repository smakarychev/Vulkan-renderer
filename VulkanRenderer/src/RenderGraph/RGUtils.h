#pragma once

#include "RenderGraph.h"
#include "RGDrawResources.h"

class SceneLight2;
class SceneLight;
class SceneGeometry;

namespace RG
{
    class Resources;
    struct IBLData;
    struct SSAOData;
}

namespace RG::RgUtils
{
    Resource ensureResource(Resource resource, Graph& graph, StringId name, const GraphTextureDescription& fallback);    
    Resource ensureResource(Resource resource, Graph& graph, StringId name, ImageUtils::DefaultTexture fallback);    
    Resource ensureResource(Resource resource, Graph& graph, StringId name, const GraphBufferDescription& fallback);

    DrawAttributeBuffers createDrawAttributes(const SceneGeometry& geometry, Graph& graph);
    void readDrawAttributes(DrawAttributeBuffers& buffers, Graph& graph, ResourceAccessFlags shaderStage);
    DrawAttributeBuffers readDrawAttributes(const SceneGeometry& geometry, Graph& graph,
        ResourceAccessFlags shaderStage);
    
    DrawAttachmentResources readWriteDrawAttachments(DrawAttachments& attachments, Graph& graph);
    DrawAttachmentResources readWriteDrawAttachments(const DrawAttachments& attachments, Graph& graph);

    SceneLightResources readSceneLight(const SceneLight& light, Graph& graph, ResourceAccessFlags shaderStage);
    SceneLightResources readSceneLight(const SceneLight2& light, Graph& graph, ResourceAccessFlags shaderStage);
    
    IBLData readIBLData(const IBLData& ibl, Graph& graph, ResourceAccessFlags shaderStage);
    SSAOData readSSAOData(const SSAOData& ssao, Graph& graph, ResourceAccessFlags shaderStage);
    DirectionalShadowData readDirectionalShadowData(const DirectionalShadowData& shadow, Graph& graph,
        ResourceAccessFlags shaderStage);
    CSMData readCSMData(const CSMData& csm, Graph& graph, ResourceAccessFlags shaderStage);

    template <typename BindGroup>
    void updateDrawAttributeBindings(BindGroup& bindGroup, const Resources& resources,
        const DrawAttributeBuffers& attributeBuffers);
    template <typename BindGroup>
    void updateSceneLightBindings(BindGroup& bindGroup, const Resources& resources,
        const SceneLightResources& lights);
    template <typename BindGroup>
    void updateIBLBindings(BindGroup& bindGroup, const Resources& resources, const IBLData& iblData);
    template <typename BindGroup>
    void updateSSAOBindings(BindGroup& bindGroup, const Resources& resources, const SSAOData& ssaoData);
    template <typename BindGroup>
    void updateShadowBindings(BindGroup& bindGroup, const Resources& resources,
        const DirectionalShadowData& shadowData);
    template <typename BindGroup>
    void updateCSMBindings(BindGroup& bindGroup, const Resources& resources,
        const CSMData& csmData);

    template <typename BindGroup>
    void updateDrawAttributeBindings(BindGroup& bindGroup, const Resources& resources,
        const DrawAttributeBuffers& attributeBuffers)
    {
        auto&& [positions, normals, tangents, uvs] = attributeBuffers;
        if constexpr (requires { bindGroup.SetPositions({.Buffer = resources.GetBuffer(positions)}); })
            bindGroup.SetPositions({.Buffer = resources.GetBuffer(positions)});

        if constexpr (requires { bindGroup.SetNormals({.Buffer = resources.GetBuffer(normals)}); })
            bindGroup.SetNormals({.Buffer = resources.GetBuffer(normals)});

        if constexpr (requires { bindGroup.SetTangents({.Buffer = resources.GetBuffer(tangents)}); })
            bindGroup.SetTangents({.Buffer = resources.GetBuffer(tangents)});

        if constexpr (requires { bindGroup.SetUv({.Buffer = resources.GetBuffer(uvs)}); })
            bindGroup.SetUv({.Buffer = resources.GetBuffer(uvs)});
    }

    template <typename BindGroup>
    void updateSceneLightBindings(BindGroup& bindGroup, const Resources& resources, const SceneLightResources& lights)
    {
        Buffer directionalLight = resources.GetBuffer(lights.DirectionalLights);
        Buffer pointLights = resources.GetBuffer(lights.PointLights);
        Buffer lightsInfo = resources.GetBuffer(lights.LightsInfo);

        bindGroup.SetDirectionalLight({.Buffer = directionalLight});
        bindGroup.SetPointLights({.Buffer = pointLights});
        bindGroup.SetLightsInfo({.Buffer = lightsInfo});
    }

    template <typename BindGroup>
    void updateIBLBindings(BindGroup& bindGroup, const Resources& resources, const IBLData& iblData)
    {
        Buffer irradianceSh = resources.GetBuffer(iblData.IrradianceSH);
        Texture prefilter = resources.GetTexture(iblData.PrefilterEnvironment);
        Texture brdf = resources.GetTexture(iblData.BRDF);

        // todo: fix me in SHADERS!
        if constexpr (requires { bindGroup.SetIrradianceSH({.Buffer = irradianceSh}); })
            bindGroup.SetIrradianceSH({.Buffer = irradianceSh});
        bindGroup.SetPrefilterMap({.Image = prefilter}, ImageLayout::Readonly);
        bindGroup.SetBrdf({.Image = brdf}, ImageLayout::Readonly);
    }

    template <typename BindGroup>
    void updateSSAOBindings(BindGroup& bindGroup, const Resources& resources, const SSAOData& ssaoData)
    {
        Texture ssao = resources.GetTexture(ssaoData.SSAO);
        bindGroup.SetSsaoTexture({.Image = ssao}, ImageLayout::Readonly);
    }

    template <typename BindGroup>
    void updateShadowBindings(BindGroup& bindGroup, const Resources& resources, const DirectionalShadowData& shadowData)
    {
        Texture shadow = resources.GetTexture(shadowData.ShadowMap);
        Buffer shadowBuffer = resources.GetBuffer(shadowData.Shadow);

        bindGroup.SetDirectionalShadowMap({.Image = shadow},
            Device::GetImageDescription(shadow).Format == Format::D32_FLOAT ?
                ImageLayout::DepthReadonly : ImageLayout::DepthStencilReadonly);

        bindGroup.SetDirectionalShadowTransform({.Buffer = shadowBuffer});
    }

    template <typename BindGroup>
    void updateCSMBindings(BindGroup& bindGroup, const Resources& resources, const CSMData& csmData)
    {
        Texture shadow = resources.GetTexture(csmData.ShadowMap);
        Buffer csm = resources.GetBuffer(csmData.CSM);

        bindGroup.SetCsm({.Image = shadow},
            Device::GetImageDescription(shadow).Format == Format::D32_FLOAT ?
                ImageLayout::DepthReadonly : ImageLayout::DepthStencilReadonly);
        bindGroup.SetCsmData({.Buffer = csm});
    }
}
