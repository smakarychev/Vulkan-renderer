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
    BufferResource IrradianceSH{};
    ImageResource PrefilterEnvironment{};
    ImageResource BRDF{};
};

struct SSAOData
{
    ImageResource SSAO{};
};

struct DirectionalShadowData
{
    ImageResource ShadowMap{};
    BufferResource Shadow{};
};

struct CsmData
{
    ImageResource ShadowMap{};
    BufferResource CsmInfo{};
};

struct DrawAttributeBuffers
{
    BufferResource Positions{};
    BufferResource Normals{};
    BufferResource Tangents{};
    BufferResource UVs{};
};

struct DrawAttachment
{
    ImageResource Resource{};
    RenderTargetAccessDescription Description{};
};

struct DepthStencilAttachment
{
    ImageResource Resource{};
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
    std::vector<ImageResource> Colors{};
    // todo: remove this optional
    std::optional<ImageResource> Depth{};
};

struct SceneLightResources
{
    BufferResource DirectionalLights{};
    BufferResource PointLights{};
    BufferResource LightsInfo{};
};
}
