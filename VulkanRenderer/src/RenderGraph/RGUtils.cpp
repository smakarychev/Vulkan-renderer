#include "RGUtils.h"

#include "RenderGraph.h"
#include "RenderGraph/RGDrawResources.h"
#include "Scene/SceneLight.h"

namespace RG::RgUtils
{
    Resource ensureResource(Resource resource, Graph& graph, StringId name, const GraphTextureDescription& fallback)
    {
        return resource.IsValid() ?
            resource :
            graph.CreateResource(name, fallback);
    }

    Resource ensureResource(Resource resource, Graph& graph, StringId name, Images::DefaultKind fallback)
    {
        return resource.IsValid() ?
            resource :
            graph.AddExternal(name, fallback);
    }

    Resource ensureResource(Resource resource, Graph& graph, StringId name, const GraphBufferDescription& fallback)
    {
        return resource.IsValid() ?
            resource :
            graph.CreateResource(name, fallback);
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
                attachment.Description.ClearColor);
            drawAttachmentResources.Colors.push_back(attachment.Resource);
        }
        if (attachments.Depth.has_value())
        {
            auto& attachment = *attachments.Depth;
            attachment.Resource = graph.DepthStencilTarget(
                attachment.Resource, attachment.Description.Subresource,
                attachment.Description.OnLoad, attachment.Description.OnStore,
                attachment.DepthBias,
                attachment.Description.ClearDepthStencil.Depth,
                attachment.Description.ClearDepthStencil.Stencil);
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
                attachment.Description.ClearColor));
        }
        if (attachments.Depth.has_value())
        {
            auto& attachment = *attachments.Depth;
            Resource resource = attachment.Resource;
            drawAttachmentResources.Depth = graph.DepthStencilTarget(
                resource, attachment.Description.Subresource,
                attachment.Description.OnLoad, attachment.Description.OnStore,
                attachment.DepthBias,
                attachment.Description.ClearDepthStencil.Depth,
                attachment.Description.ClearDepthStencil.Stencil);
        }

        return drawAttachmentResources;
    }

    SceneLightResources readSceneLight(const SceneLight& light, Graph& graph, ResourceAccessFlags shaderStage)
    {
        using enum ResourceAccessFlags;

        SceneLightResources resources = {};

        resources.DirectionalLights = graph.AddExternal("Light.Directional"_hsv, light.GetBuffers().DirectionalLights);
        resources.DirectionalLights = graph.Read(resources.DirectionalLights, shaderStage | Storage);

        resources.LightsInfo = graph.AddExternal("Light.LightsInfo"_hsv, light.GetBuffers().LightsInfo);
        resources.LightsInfo = graph.Read(resources.LightsInfo, shaderStage | Uniform);

        resources.PointLights = graph.AddExternal("Light.PointLights"_hsv, light.GetBuffers().PointLights);
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

        Resource ssaoResource = RgUtils::ensureResource(ssao.SSAO, graph, "SSAO.Dummy"_hsv,
            Images::DefaultKind::White);

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
        csmData.CsmInfo = graph.Read(csm.CsmInfo, shaderStage | Uniform);

        return csmData;
    }
}
