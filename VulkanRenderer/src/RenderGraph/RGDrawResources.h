#pragma once

#include "RGCommon.h"
#include "RGResource.h"
#include "Core/Camera.h"
#include "Rendering/RenderingInfo.h"

/* Draw resources that are commonly used by different draw passes
 * Draw features control what resources are actually used 
 */

class Camera;
class SceneLight;

namespace RG
{
    struct IBLData
    {
        Resource Irradiance{};
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

    struct CSMData
    {
        Resource ShadowMap{};
        Resource CSM{};
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
        ColorAttachmentDescription Description{};
    };
    struct DepthStencilAttachment
    {
        Resource Resource{};
        DepthStencilAttachmentDescription Description{};
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
        std::optional<Resource> Depth{};
    };

    struct SceneLightResources
    {
        Resource DirectionalLight{};
    };

    struct DrawInitInfo
    {
        DrawFeatures DrawFeatures{DrawFeatures::AllAttributes};
        ShaderPipeline DrawPipeline{};
        std::optional<const ShaderDescriptors*> MaterialDescriptors{};
    };
    
    struct DrawExecutionInfo
    {
        DrawAttachments Attachments{};
        const SceneLight* SceneLights{nullptr};
        std::optional<IBLData> IBL{};
        std::optional<SSAOData> SSAO{};
        std::optional<CSMData> CSMData{};
    };
}

