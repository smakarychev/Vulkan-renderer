#pragma once

#include <functional>

#include "RGCommon.h"
#include "RGResource.h"
#include "Rendering/RenderingInfo.h"

/* Draw resources that are commonly used by different draw passes
 * Draw features control what resources are actually used 
 */

class Shader;
class Camera;
class SceneLight;

namespace RG
{
    struct DrawExecutionInfo;
    class Resources;

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

    struct CSMData
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
        // todo: remove this optional
        std::optional<Resource> Depth{};
    };

    struct SceneLightResources
    {
        Resource DirectionalLights{};
        Resource PointLights{};
        Resource LightsInfo{};
    };

    // todo: naming on these two please...
    struct GeometryDrawExecutionInfo
    {
        Resource Camera{};
        Resource Objects{};
        Resource Commands{};
        DrawAttributeBuffers DrawAttributes{};
        Resource Triangles{};
        /* each shader has just one set of bindings, but we need many
         * (because of batch drawing in the triangle culling),
         * so we need to have a way to create multiple shaders, `ExecutionId` serves for that
         */
        u32 ExecutionId{0};
    };
    
    using DrawPassSetupFn = std::function<void(Graph&)>;
    using DrawPassBindFn = std::function<const Shader&(RenderCommandList& cmdList, const Resources&,
        const GeometryDrawExecutionInfo&)>;

    struct DrawInitInfo
    {
        Pipeline DrawPipeline{};
        Descriptors MaterialDescriptors{};
    };
    
    struct DrawExecutionInfo
    {
        DrawPassSetupFn DrawSetup{};
        DrawPassBindFn DrawBind{};
        DrawAttachments Attachments{};
    };
}

