#pragma once

#include "Core/Camera.h"
#include "RenderGraph/Passes/HiZ/HiZPass.h"

#include <vector>

#include "CullContext.h"
#include "RenderHandleArray.h"
#include "Light/SceneLight.h"
#include "RenderGraph/RGDrawResources.h"

class SceneGeometry;

/* cull view description has 2 parts:
 *  - static part defines resources that are unchanged from frame to frame
 *    (such as geometry, hiz, etc.)
 *  - dynamic part defines resources that potentially change each frame 
 */

struct CullViewStaticDescription
{
    const SceneGeometry* Geometry{nullptr};
    /* the user is not expected to set HiZContext manually, but it is possible */
    std::shared_ptr<HiZPassContext> HiZContext{};
    RG::DrawFeatures DrawFeatures{RG::DrawFeatures::None};
    bool CullTriangles{false};
};

struct CullViewDynamicDescription
{
    glm::uvec2 Resolution{};
    const Camera* Camera{nullptr};
    bool ClampDepth{false};
    RG::DrawAttachments DrawAttachments{};
    std::optional<SceneLight> SceneLights{};
    std::optional<RG::IBLData> IBL{};
    std::optional<RG::SSAOData> SSAO{};
    std::optional<RG::CSMData> CSMData{};
};

struct CullViewDescription
{
    CullViewStaticDescription Static{};
    CullViewDynamicDescription Dynamic{};
};

struct CullViewDataGPU
{
    glm::mat4 ViewMatrix;
    glm::mat4 ViewProjectionMatrix;
    FrustumPlanes FrustumPlanes;
    ProjectionData ProjectionData;
    f32 HiZWidth;
    f32 HiZHeight;

    /* these are obviously bools, but gpu size for bool is 32 bit */
    u32 IsOrthographic{0};
    u32 ClampDepth{0};

    u32 Padding[2];

    static CullViewDataGPU FromCullViewDescription(const CullViewDescription& description);
};

class CullViewHandle
{
    friend class CullMultiviewData;
public:
    CullViewHandle() = default;
private:
    CullViewHandle(u32 index) : m_Index(index) {}
private:
    u32 m_Index{0};
};

class CullMultiviewData
{
public:
    struct ViewSpan
    {
        u32 First{0};
        u32 Count{0};
    };
public:
    u32 AddView(const CullViewStaticDescription& description);
    void Finalize();
    
    void UpdateView(u32 viewIndex, const CullViewDynamicDescription& description);

    const std::vector<CullViewDescription>& Views() const { return m_Views; }
    const std::vector<const SceneGeometry*>& Geometries() const { return m_Geometries; }
    const std::vector<ViewSpan>& ViewSpans() const { return m_ViewSpans; }
    const std::vector<CullViewVisibility>& Visibilities() const { return m_CullVisibilities; }
    const std::vector<CullViewTriangleVisibility>& TriangleVisibilities() const { return m_CullTriangleVisibilities; }
    
    std::vector<CullViewDataGPU> CreateMultiviewGPU() const;
private:
    std::vector<CullViewDescription> m_Views;

    bool m_IsFinalized{false};
    std::vector<const SceneGeometry*> m_Geometries;
    std::vector<ViewSpan> m_ViewSpans;
    std::vector<CullViewVisibility> m_CullVisibilities;
    std::vector<CullViewTriangleVisibility> m_CullTriangleVisibilities;
};
