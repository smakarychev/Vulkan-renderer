#include "RGUtils.h"

#include "RenderGraph.h"
#include "RGGeometry.h"
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

    DrawAttributeBuffers readDrawAttributes(const Geometry& geometry, Graph& graph, const std::string& baseName,
        ResourceAccessFlags shaderStage)
    {
        using enum ResourceAccessFlags;
        
        DrawAttributeBuffers buffers = {};

        buffers.PositionsSsbo = graph.AddExternal(baseName + ".Positions", geometry.GetAttributeBuffers().Positions);
        buffers.NormalsSsbo = graph.AddExternal(baseName + ".Normals", geometry.GetAttributeBuffers().Normals);
        buffers.TangentsSsbo = graph.AddExternal(baseName + ".Tangents", geometry.GetAttributeBuffers().Tangents);
        buffers.UVsSsbo = graph.AddExternal(baseName + ".Uvs", geometry.GetAttributeBuffers().UVs);
        
        buffers.PositionsSsbo = graph.Read(buffers.PositionsSsbo, shaderStage | Storage);
        buffers.NormalsSsbo = graph.Read(buffers.NormalsSsbo, shaderStage | Storage);
        buffers.TangentsSsbo = graph.Read(buffers.TangentsSsbo, shaderStage | Storage);
        buffers.UVsSsbo = graph.Read(buffers.UVsSsbo, shaderStage | Storage);

        return buffers;
    }

    DrawAttachmentResources readWriteDrawAttachments(const DrawAttachments& attachments, Graph& graph)
    {
        DrawAttachmentResources drawAttachmentResources = {};
        
        for (auto& attachment : attachments.ColorAttachments)
        {
            Resource resource = attachment.Resource;
            drawAttachmentResources.RenderTargets.push_back(graph.RenderTarget(
                resource,
                attachment.Description.OnLoad, attachment.Description.OnStore,
                attachment.Description.Clear.Color.F));
        }
        if (attachments.DepthAttachment.has_value())
        {
            auto& attachment = *attachments.DepthAttachment;
            Resource resource = attachment.Resource;
            drawAttachmentResources.DepthTarget = graph.DepthStencilTarget(
                resource,
                attachment.Description.OnLoad, attachment.Description.OnStore,
                attachment.Description.Clear.DepthStencil.Depth,
                attachment.Description.Clear.DepthStencil.Stencil);
        }

        return drawAttachmentResources;
    }

    void updateDrawAttributeBindings(const ShaderDescriptors& descriptors, const Resources& resources,
        const DrawAttributeBuffers& attributeBuffers, DrawFeatures features)
    {
        using enum DrawFeatures;

        const Buffer& positionsSsbo = resources.GetBuffer(attributeBuffers.PositionsSsbo);
        const Buffer& normalsSsbo = resources.GetBuffer(attributeBuffers.NormalsSsbo);
        const Buffer& tangentsSsbo = resources.GetBuffer(attributeBuffers.TangentsSsbo);
        const Buffer& uvSsbo = resources.GetBuffer(attributeBuffers.UVsSsbo);

        if (enumHasAny(features, Positions))
            descriptors.UpdateBinding("u_positions", positionsSsbo.BindingInfo());
        if (enumHasAny(features, Normals))
            descriptors.UpdateBinding("u_normals", normalsSsbo.BindingInfo());
        if (enumHasAny(features, Tangents))
            descriptors.UpdateBinding("u_tangents", tangentsSsbo.BindingInfo());
        if (enumHasAny(features, UV))
            descriptors.UpdateBinding("u_uv", uvSsbo.BindingInfo());
    }

    void updateIBLBindings(const ShaderDescriptors& descriptors, const Resources& resources, const IBLData& iblData)
    {
        const Texture& irradiance = resources.GetTexture(iblData.Irradiance);
        const Texture& prefilter = resources.GetTexture(iblData.PrefilterEnvironment);
        const Texture& brdf = resources.GetTexture(iblData.BRDF);

        descriptors.UpdateBinding("u_irradiance_map", irradiance.BindingInfo(
            ImageFilter::Linear, ImageLayout::Readonly));
        descriptors.UpdateBinding("u_prefilter_map", prefilter.BindingInfo(
            ImageFilter::Linear, ImageLayout::Readonly));
        descriptors.UpdateBinding("u_brdf", brdf.BindingInfo(
            ImageFilter::Linear, ImageLayout::Readonly));
    }

    void updateSSAOBindings(const ShaderDescriptors& descriptors, const Resources& resources, const SSAOData& ssaoData)
    {
        const Texture& ssao = resources.GetTexture(ssaoData.SSAOTexture);

        descriptors.UpdateBinding("u_ssao_texture", ssao.BindingInfo(
            ImageFilter::Linear, ImageLayout::Readonly));
    }

    void updateShadowBindings(const ShaderDescriptors& descriptors, const Resources& resources,
        const DirectionalShadowData& shadowData, ResourceUploader& resourceUploader)
    {
        const Texture& shadow = resources.GetTexture(shadowData.ShadowMap);
        const Buffer& view = resources.GetBuffer(shadowData.ViewProjectionResource, shadowData.ViewProjection,
            resourceUploader);

        descriptors.UpdateBinding("u_directional_shadow_map", shadow.BindingInfo(
            ImageFilter::Linear,
            shadow.Description().Format == Format::D32_FLOAT ?
                ImageLayout::DepthReadonly : ImageLayout::DepthStencilReadonly));

        descriptors.UpdateBinding("u_directional_shadow_transform", view.BindingInfo());
    }
}
