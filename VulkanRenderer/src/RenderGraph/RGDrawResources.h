#pragma once

#include "RGAccess.h"
#include "RGResource.h"

/* Draw resources that are commonly used by different draw passes
 * Draw features control what resources are actually used 
 */

class Camera;

namespace RG
{
    struct DrawExecutionInfo;

    struct IBLData
    {
        Resource IrradianceSH{};
        Resource PrefilterEnvironment{};
        Resource BRDF{};
    };

    struct SSAOData
    {
        Resource SSAO{};
    };

    struct DirectionalShadowData
    {
        Resource ShadowMap{};
        Resource Shadow{};
    };

    struct CsmData
    {
        Resource ShadowMap{};
        Resource CsmInfo{};
    };
    
    struct DrawAttributeBuffers
    {
        Resource Positions{};
        Resource Normals{};
        Resource Tangents{};
        Resource UVs{};
    };

    struct DrawAttachment
    {
        Resource Resource{};
        RenderTargetAccessDescription Description{};
    };
    struct DepthStencilAttachment
    {
        Resource Resource{};
        DepthStencilTargetAccessDescription Description{};
        std::optional<DepthBias> DepthBias{};
    };

    struct DrawAttachments
    {
        std::vector<DrawAttachment> Colors{};
        std::optional<DepthStencilAttachment> Depth{};
    };

    struct DrawAttachmentResources
    {
        std::vector<Resource> Colors{};
        // todo: remove this optional
        std::optional<Resource> Depth{};
    };

    struct SceneLightResources
    {
        Resource DirectionalLights{};
        Resource PointLights{};
        Resource LightsInfo{};
    };
}

