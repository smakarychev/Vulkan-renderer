#include "RGUtils.h"

#include "RenderGraph.h"
#include "Scene/SceneGeometry.h"
#include "Light/SceneLight.h"
#include "RenderGraph/RGDrawResources.h"

namespace RG::RgUtils
{
    Resource ensureResource(Resource resource, Graph& graph, const std::string& name,
        const GraphTextureDescription& fallback)
    {
        return resource.IsValid() ?
            resource :
            graph.CreateResource(name, fallback);
    }

    Resource ensureResource(Resource resource, Graph& graph, const std::string& name,
        ImageUtils::DefaultTexture fallback)
    {
        return resource.IsValid() ?
            resource :
            graph.AddExternal(name, fallback);
    }

    Resource ensureResource(Resource resource, Graph& graph, const std::string& name,
        const GraphBufferDescription& fallback)
    {
        return resource.IsValid() ?
            resource :
            graph.CreateResource(name, fallback);
    }

    DrawAttributeBuffers createDrawAttributes(const SceneGeometry& geometry, Graph& graph, const std::string& baseName)
    {
        DrawAttributeBuffers buffers = {};

        buffers.Positions = graph.AddExternal(baseName + ".Positions", geometry.GetAttributeBuffers().Positions);
        buffers.Normals = graph.AddExternal(baseName + ".Normals", geometry.GetAttributeBuffers().Normals);
        buffers.Tangents = graph.AddExternal(baseName + ".Tangents", geometry.GetAttributeBuffers().Tangents);
        buffers.UVs = graph.AddExternal(baseName + ".Uvs", geometry.GetAttributeBuffers().UVs);

        return buffers;
    }

    void readDrawAttributes(DrawAttributeBuffers& buffers, Graph& graph, ResourceAccessFlags shaderStage)
    {
        using enum ResourceAccessFlags;
        
        buffers.Positions = graph.Read(buffers.Positions, shaderStage | Storage);
        buffers.Normals = graph.Read(buffers.Normals, shaderStage | Storage);
        buffers.Tangents = graph.Read(buffers.Tangents, shaderStage | Storage);
        buffers.UVs = graph.Read(buffers.UVs, shaderStage | Storage);
    }

    DrawAttributeBuffers readDrawAttributes(const SceneGeometry& geometry, Graph& graph, const std::string& baseName,
        ResourceAccessFlags shaderStage)
    {
        DrawAttributeBuffers buffers = createDrawAttributes(geometry, graph, baseName);
        readDrawAttributes(buffers, graph, shaderStage);

        return buffers;
    }

    DrawAttachmentResources readWriteDrawAttachments(DrawAttachments& attachments, Graph& graph)
    {
        DrawAttachmentResources drawAttachmentResources = {};
        drawAttachmentResources.Colors.reserve(attachments.Colors.size());
        
        for (auto& attachment : attachments.Colors)
        {
            attachment.Resource = graph.RenderTarget(
                attachment.Resource, attachment.Description.Subresource,
                attachment.Description.OnLoad, attachment.Description.OnStore,
                attachment.Description.ClearColor.F);
            drawAttachmentResources.Colors.push_back(attachment.Resource);
        }
        if (attachments.Depth.has_value())
        {
            auto& attachment = *attachments.Depth;
            attachment.Resource = graph.DepthStencilTarget(
                attachment.Resource, attachment.Description.Subresource,
                attachment.Description.OnLoad, attachment.Description.OnStore,
                attachment.DepthBias,
                attachment.Description.ClearDepth,
                attachment.Description.ClearStencil);
            drawAttachmentResources.Depth = attachment.Resource;
        }

        return drawAttachmentResources;
    }

    DrawAttachmentResources readWriteDrawAttachments(const DrawAttachments& attachments, Graph& graph)
    {
        DrawAttachmentResources drawAttachmentResources = {};
        drawAttachmentResources.Colors.reserve(attachments.Colors.size());
        
        for (auto& attachment : attachments.Colors)
        {
            Resource resource = attachment.Resource;
            drawAttachmentResources.Colors.push_back(graph.RenderTarget(
                resource, attachment.Description.Subresource,
                attachment.Description.OnLoad, attachment.Description.OnStore,
                attachment.Description.ClearColor.F));
        }
        if (attachments.Depth.has_value())
        {
            auto& attachment = *attachments.Depth;
            Resource resource = attachment.Resource;
            drawAttachmentResources.Depth = graph.DepthStencilTarget(
                resource, attachment.Description.Subresource,
                attachment.Description.OnLoad, attachment.Description.OnStore,
                attachment.DepthBias,
                attachment.Description.ClearDepth,
                attachment.Description.ClearStencil);
        }

        return drawAttachmentResources;
    }

    SceneLightResources readSceneLight(const SceneLight& light, Graph& graph, ResourceAccessFlags shaderStage)
    {
        using enum ResourceAccessFlags;

        SceneLightResources resources = {};

        resources.DirectionalLight = graph.AddExternal("Light.Directional",
            light.GetBuffers().DirectionalLight);
        resources.DirectionalLight = graph.Read(resources.DirectionalLight, shaderStage | Uniform);

        resources.LightsInfo = graph.AddExternal("Light.LightsInfo", light.GetBuffers().LightsInfo);
        resources.LightsInfo = graph.Read(resources.LightsInfo, shaderStage | Uniform);

        resources.PointLights = graph.AddExternal("Light.PointLights", LIGHT_CULLING ?
                light.GetBuffers().VisiblePointLights : light.GetBuffers().PointLights);
        resources.PointLights = graph.Read(resources.PointLights, shaderStage | Storage);
        
        return resources;
    }

    IBLData readIBLData(const IBLData& ibl, Graph& graph, ResourceAccessFlags shaderStage)
    {
        using enum ResourceAccessFlags;
        
        ASSERT(ibl.IrradianceSH.IsValid(), "Must provide irradiance spherical harmonics")
        ASSERT(ibl.PrefilterEnvironment.IsValid(), "Must provide prefilter map")
        ASSERT(ibl.BRDF.IsValid(), "Must provide brdf")
        
        IBLData iblData = {};
        
        iblData.IrradianceSH = graph.Read(ibl.IrradianceSH, shaderStage | Uniform);
        iblData.PrefilterEnvironment = graph.Read(ibl.PrefilterEnvironment, shaderStage | Sampled);
        iblData.BRDF = graph.Read(ibl.BRDF, shaderStage | Sampled);

        return iblData;
    }

    SSAOData readSSAOData(const SSAOData& ssao, Graph& graph, ResourceAccessFlags shaderStage)
    {
        using enum ResourceAccessFlags;

        Resource ssaoResource = RgUtils::ensureResource(ssao.SSAO, graph, "SSAO.Dummy",
            ImageUtils::DefaultTexture::White);

        SSAOData ssaoData = {};
        ssaoData.SSAO = graph.Read(ssaoResource, shaderStage | Sampled);

        return ssaoData;
    }

    DirectionalShadowData readDirectionalShadowData(const DirectionalShadowData& shadow, Graph& graph,
        ResourceAccessFlags shaderStage)
    {
        using enum ResourceAccessFlags;

        DirectionalShadowData shadowData = {};
        
        shadowData.ShadowMap = graph.Read(shadow.ShadowMap, shaderStage | Sampled);
        shadowData.Shadow = graph.Read(shadow.Shadow, shaderStage | Uniform);

        return shadowData;
    }

    CSMData readCSMData(const CSMData& csm, Graph& graph, ResourceAccessFlags shaderStage)
    {
        using enum ResourceAccessFlags;

        CSMData csmData = {};

        csmData.ShadowMap = graph.Read(csm.ShadowMap, shaderStage | Sampled);
        csmData.CSM = graph.Read(csm.CSM, shaderStage | Uniform);

        return csmData;
    }

    void updateDrawAttributeBindings(const ShaderDescriptors& descriptors, const Resources& resources,
        const DrawAttributeBuffers& attributeBuffers, DrawFeatures features)
    {
        using enum DrawFeatures;

        const Buffer& positions = resources.GetBuffer(attributeBuffers.Positions);
        const Buffer& normals = resources.GetBuffer(attributeBuffers.Normals);
        const Buffer& tangents = resources.GetBuffer(attributeBuffers.Tangents);
        const Buffer& uv = resources.GetBuffer(attributeBuffers.UVs);

        if (enumHasAny(features, Positions))
            descriptors.UpdateBinding(UNIFORM_POSITIONS, positions.BindingInfo());
        if (enumHasAny(features, Normals))
            descriptors.UpdateBinding(UNIFORM_NORMALS, normals.BindingInfo());
        if (enumHasAny(features, Tangents))
            descriptors.UpdateBinding(UNIFORM_TANGENTS, tangents.BindingInfo());
        if (enumHasAny(features, UV))
            descriptors.UpdateBinding(UNIFORM_UV, uv.BindingInfo());
    }

    void updateSceneLightBindings(const ShaderDescriptors& descriptors, const Resources& resources,
        const SceneLightResources& lights)
    {
        const Buffer& directionalLight = resources.GetBuffer(lights.DirectionalLight);
        const Buffer& pointLights = resources.GetBuffer(lights.PointLights);
        const Buffer& lightsInfo = resources.GetBuffer(lights.LightsInfo);

        descriptors.UpdateBinding("u_directional_light", directionalLight.BindingInfo());
        descriptors.UpdateBinding("u_point_lights", pointLights.BindingInfo());
        descriptors.UpdateBinding("u_lights_info", lightsInfo.BindingInfo());
    }

    void updateIBLBindings(const ShaderDescriptors& descriptors, const Resources& resources, const IBLData& iblData)
    {
        const Buffer& irradianceSh = resources.GetBuffer(iblData.IrradianceSH);
        const Texture& prefilter = resources.GetTexture(iblData.PrefilterEnvironment);
        const Texture& brdf = resources.GetTexture(iblData.BRDF);

        descriptors.UpdateBinding(UNIFORM_IRRADIANCE_SH, irradianceSh.BindingInfo());
        descriptors.UpdateBinding(UNIFORM_PREFILTER_MAP, prefilter.BindingInfo(
            ImageFilter::Linear, ImageLayout::Readonly));
        descriptors.UpdateBinding(UNIFORM_BRDF, brdf.BindingInfo(
            ImageFilter::Linear, ImageLayout::Readonly));
    }

    void updateSSAOBindings(const ShaderDescriptors& descriptors, const Resources& resources, const SSAOData& ssaoData)
    {
        const Texture& ssao = resources.GetTexture(ssaoData.SSAO);

        descriptors.UpdateBinding(UNIFORM_SSAO_TEXTURE, ssao.BindingInfo(
            ImageFilter::Linear, ImageLayout::Readonly));
    }

    void updateShadowBindings(const ShaderDescriptors& descriptors, const Resources& resources,
        const DirectionalShadowData& shadowData)
    {
        const Texture& shadow = resources.GetTexture(shadowData.ShadowMap);
        const Buffer& shadowBuffer = resources.GetBuffer(shadowData.Shadow);

        descriptors.UpdateBinding("u_directional_shadow_map", shadow.BindingInfo(
            ImageFilter::Linear,
            shadow.Description().Format == Format::D32_FLOAT ?
                ImageLayout::DepthReadonly : ImageLayout::DepthStencilReadonly));

        descriptors.UpdateBinding("u_directional_shadow_transform", shadowBuffer.BindingInfo());
    }

    void updateCSMBindings(const ShaderDescriptors& descriptors, const Resources& resources, const CSMData& csmData)
    {
        const Texture& shadow = resources.GetTexture(csmData.ShadowMap);
        const Buffer& csm = resources.GetBuffer(csmData.CSM);

        descriptors.UpdateBinding("u_csm", shadow.BindingInfo(
            ImageFilter::Linear,
            shadow.Description().Format == Format::D32_FLOAT ?
                ImageLayout::DepthReadonly : ImageLayout::DepthStencilReadonly));

        descriptors.UpdateBinding("u_csm_data", csm.BindingInfo());
    }
}
