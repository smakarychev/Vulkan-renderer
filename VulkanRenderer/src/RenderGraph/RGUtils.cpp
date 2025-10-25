#include "rendererpch.h"

#include "RGUtils.h"

#include "RGGraph.h"
#include "RenderGraph/RGDrawResources.h"
#include "Scene/SceneLight.h"

namespace RG::RgUtils
{
    Resource ensureResource(Resource resource, Graph& graph, StringId name, const RGImageDescription& fallback)
    {
        return resource.IsValid() ?
            resource :
            graph.Create(name, fallback);
    }

    Resource ensureResource(Resource resource, Graph& graph, StringId name, const RGBufferDescription& fallback)
    {
        return resource.IsValid() ?
            resource :
            graph.Create(name, fallback);
    }

    DrawAttachmentResources readWriteDrawAttachments(DrawAttachments& attachments, Graph& graph)
    {
        DrawAttachmentResources drawAttachmentResources = {};
        drawAttachmentResources.Colors.reserve(attachments.Colors.size());
        
        for (auto& attachment : attachments.Colors)
        {
            attachment.Resource = graph.RenderTarget(
                attachment.Resource, attachment.Description);
            drawAttachmentResources.Colors.push_back(attachment.Resource);
        }
        if (attachments.Depth.has_value())
        {
            auto& attachment = *attachments.Depth;
            attachment.Resource = graph.DepthStencilTarget(
                attachment.Resource, attachment.Description, attachment.DepthBias);
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
            drawAttachmentResources.Colors.push_back(graph.RenderTarget(resource, attachment.Description));
        }
        if (attachments.Depth.has_value())
        {
            auto& attachment = *attachments.Depth;
            Resource resource = attachment.Resource;
            drawAttachmentResources.Depth = graph.DepthStencilTarget(
                resource, attachment.Description, attachment.DepthBias);
        }

        return drawAttachmentResources;
    }

    SceneLightResources readSceneLight(const SceneLight& light, Graph& graph, ResourceAccessFlags shaderStage)
    {
        using enum ResourceAccessFlags;

        SceneLightResources resources = {};

        resources.DirectionalLights = graph.Import("Light.Directional"_hsv, light.GetBuffers().DirectionalLights);
        resources.DirectionalLights = graph.ReadBuffer(resources.DirectionalLights, shaderStage | Storage);

        resources.LightsInfo = graph.Import("Light.LightsInfo"_hsv, light.GetBuffers().LightsInfo);
        resources.LightsInfo = graph.ReadBuffer(resources.LightsInfo, shaderStage | Uniform);

        resources.PointLights = graph.Import("Light.PointLights"_hsv, light.GetBuffers().PointLights);
        resources.PointLights = graph.ReadBuffer(resources.PointLights, shaderStage | Storage);
        
        return resources;
    }

    IBLData readIBLData(const IBLData& ibl, Graph& graph, ResourceAccessFlags shaderStage)
    {
        using enum ResourceAccessFlags;
        
        ASSERT(ibl.IrradianceSH.IsValid(), "Must provide irradiance spherical harmonics")
        ASSERT(ibl.PrefilterEnvironment.IsValid(), "Must provide prefilter map")
        ASSERT(ibl.BRDF.IsValid(), "Must provide brdf")
        
        IBLData iblData = {};
        
        iblData.IrradianceSH = graph.ReadBuffer(ibl.IrradianceSH, shaderStage | Uniform);
        iblData.PrefilterEnvironment = graph.ReadImage(ibl.PrefilterEnvironment, shaderStage | Sampled);
        iblData.BRDF = graph.ReadImage(ibl.BRDF, shaderStage | Sampled);

        return iblData;
    }

    SSAOData readSSAOData(const SSAOData& ssao, Graph& graph, ResourceAccessFlags shaderStage)
    {
        using enum ResourceAccessFlags;

        Resource ssaoResource = ssao.SSAO;

        SSAOData ssaoData = {};
        ssaoData.SSAO = graph.ReadImage(ssaoResource, shaderStage | Sampled);

        return ssaoData;
    }

    DirectionalShadowData readDirectionalShadowData(const DirectionalShadowData& shadow, Graph& graph,
        ResourceAccessFlags shaderStage)
    {
        using enum ResourceAccessFlags;

        DirectionalShadowData shadowData = {};
        
        shadowData.ShadowMap = graph.ReadImage(shadow.ShadowMap, shaderStage | Sampled);
        shadowData.Shadow = graph.ReadBuffer(shadow.Shadow, shaderStage | Uniform);

        return shadowData;
    }

    CsmData readCsmData(const CsmData& csm, Graph& graph, ResourceAccessFlags shaderStage)
    {
        using enum ResourceAccessFlags;

        CsmData csmData = {};

        csmData.ShadowMap = graph.ReadImage(csm.ShadowMap, shaderStage | Sampled);
        csmData.CsmInfo = graph.ReadBuffer(csm.CsmInfo, shaderStage | Uniform);

        return csmData;
    }
}
