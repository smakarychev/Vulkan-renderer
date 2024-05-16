#pragma once
#include "RGResource.h"
#include "Rendering/RenderingInfo.h"

/* Draw resources that are commonly used by different draw passes
 * Draw features control what resources are actually used 
 */

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
    
    enum class DrawFeatures
    {
        None        = 0,
        Positions   = BIT(1),
        Normals     = BIT(2),
        Tangents    = BIT(3),
        UV          = BIT(4),
        Materials   = BIT(5),
        Textures    = BIT(6),
        SSAO        = BIT(7),
        IBL         = BIT(8),

        // does graphics use 'u_triangles' buffer
        Triangles   = BIT(9),

        // positions, normals, uvs (tangents are not used)  
        MainAttributes = Positions | Normals | UV,

        // positions, normals, tangents, uvs
        AllAttributes = MainAttributes | Tangents,

        // positions and uvs for texture fetch
        AlphaTest = Positions | UV | Materials | Textures,
        
        // all attributes + materials and textures
        Shaded = AllAttributes | Materials | Textures,

        // materials + all ibl textures
        ShadedIBL = Shaded | IBL,
    };
    
    CREATE_ENUM_FLAGS_OPERATORS(DrawFeatures)

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
}

