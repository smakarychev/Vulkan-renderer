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
        Resource SSAOTexture{};
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

        // is graphics use 'u_triangles' buffer
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
        Resource PositionsSsbo{};
        Resource NormalsSsbo{};
        Resource TangentsSsbo{};
        Resource UVsSsbo{};
    };

    struct DrawAttachment
    {
        Resource Resource{};
        RenderingAttachmentDescription Description{};
    };

    struct DrawAttachments
    {
        std::vector<DrawAttachment> ColorAttachments{};
        std::optional<DrawAttachment> DepthAttachment{};
    };

    struct DrawAttachmentResources
    {
        std::vector<Resource> RenderTargets{};
        std::optional<Resource> DepthTarget{};
    };
}

