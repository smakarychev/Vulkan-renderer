#pragma once

#include "RenderGraph.h"
#include "RGDrawResources.h"

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
    Resource ensureResource(Resource resource, Graph& graph, const std::string& name,
        const GraphTextureDescription& fallback);    
    Resource ensureResource(Resource resource, Graph& graph, const std::string& name,
        ImageUtils::DefaultTexture fallback);    
    Resource ensureResource(Resource resource, Graph& graph, const std::string& name,
        const GraphBufferDescription& fallback);

    DrawAttributeBuffers createDrawAttributes(const SceneGeometry& geometry, Graph& graph, const std::string& baseName);
    void readDrawAttributes(DrawAttributeBuffers& buffers, Graph& graph, ResourceAccessFlags shaderStage);
    DrawAttributeBuffers readDrawAttributes(const SceneGeometry& geometry, Graph& graph, const std::string& baseName,
        ResourceAccessFlags shaderStage);
    
    DrawAttachmentResources readWriteDrawAttachments(DrawAttachments& attachments, Graph& graph);
    DrawAttachmentResources readWriteDrawAttachments(const DrawAttachments& attachments, Graph& graph);

    SceneLightResources readSceneLight(const SceneLight& light, Graph& graph, ResourceAccessFlags shaderStage);
    
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
        if constexpr (requires { bindGroup.SetPositions(resources.GetBuffer(positions).BindingInfo()); })
            bindGroup.SetPositions(resources.GetBuffer(positions).BindingInfo());

        if constexpr (requires { bindGroup.SetNormals(resources.GetBuffer(normals).BindingInfo()); })
            bindGroup.SetNormals(resources.GetBuffer(normals).BindingInfo());

        if constexpr (requires { bindGroup.SetTangents(resources.GetBuffer(tangents).BindingInfo()); })
            bindGroup.SetTangents(resources.GetBuffer(tangents).BindingInfo());

        if constexpr (requires { bindGroup.SetUv(resources.GetBuffer(uvs).BindingInfo()); })
            bindGroup.SetUv(resources.GetBuffer(uvs).BindingInfo());
    }

    template <typename BindGroup>
    void updateSceneLightBindings(BindGroup& bindGroup, const Resources& resources, const SceneLightResources& lights)
    {
        const Buffer& directionalLight = resources.GetBuffer(lights.DirectionalLight);
        const Buffer& pointLights = resources.GetBuffer(lights.PointLights);
        const Buffer& lightsInfo = resources.GetBuffer(lights.LightsInfo);

        bindGroup.SetDirectionalLight(directionalLight.BindingInfo());
        bindGroup.SetPointLights(pointLights.BindingInfo());
        bindGroup.SetLightsInfo(lightsInfo.BindingInfo());
    }

    template <typename BindGroup>
    void updateIBLBindings(BindGroup& bindGroup, const Resources& resources, const IBLData& iblData)
    {
        const Buffer& irradianceSh = resources.GetBuffer(iblData.IrradianceSH);
        const Texture& prefilter = resources.GetTexture(iblData.PrefilterEnvironment);
        const Texture& brdf = resources.GetTexture(iblData.BRDF);

        // todo: fix me in SHADERS!
        if constexpr (requires { bindGroup.SetIrradianceSH(irradianceSh.BindingInfo()); })
            bindGroup.SetIrradianceSH(irradianceSh.BindingInfo());
        bindGroup.SetPrefilterMap(prefilter.BindingInfo(ImageFilter::Linear, ImageLayout::Readonly));
        bindGroup.SetBrdf(brdf.BindingInfo(ImageFilter::Linear, ImageLayout::Readonly));
    }

    template <typename BindGroup>
    void updateSSAOBindings(BindGroup& bindGroup, const Resources& resources, const SSAOData& ssaoData)
    {
        const Texture& ssao = resources.GetTexture(ssaoData.SSAO);

        bindGroup.SetSsaoTexture(ssao.BindingInfo(ImageFilter::Linear, ImageLayout::Readonly));
    }

    template <typename BindGroup>
    void updateShadowBindings(BindGroup& bindGroup, const Resources& resources, const DirectionalShadowData& shadowData)
    {
        const Texture& shadow = resources.GetTexture(shadowData.ShadowMap);
        const Buffer& shadowBuffer = resources.GetBuffer(shadowData.Shadow);

        bindGroup.SetDirectionalShadowMap(shadow.BindingInfo(
            ImageFilter::Linear,
            shadow.Description().Format == Format::D32_FLOAT ?
                ImageLayout::DepthReadonly : ImageLayout::DepthStencilReadonly));

        bindGroup.SetDirectionalShadowTransform(shadowBuffer.BindingInfo());
    }

    template <typename BindGroup>
    void updateCSMBindings(BindGroup& bindGroup, const Resources& resources, const CSMData& csmData)
    {
        const Texture& shadow = resources.GetTexture(csmData.ShadowMap);
        const Buffer& csm = resources.GetBuffer(csmData.CSM);

        bindGroup.SetCsm(shadow.BindingInfo(
            ImageFilter::Linear,
            shadow.Description().Format == Format::D32_FLOAT ?
                ImageLayout::DepthReadonly : ImageLayout::DepthStencilReadonly));
        bindGroup.SetCsmData(csm.BindingInfo());
    }
}
