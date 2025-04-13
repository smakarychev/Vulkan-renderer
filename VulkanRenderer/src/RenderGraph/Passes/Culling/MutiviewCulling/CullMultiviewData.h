#pragma once

#include "Core/Camera.h"

#include <vector>

#include "CullContext.h"
#include "RenderHandleArray.h"
#include "Light/SceneLight.h"
#include "RenderGraph/RGDrawResources.h"

class Shader;
class SceneGeometry;

// todo: why this exists?

/* cull view description has 2 parts:
 *  - static part defines resources that are unchanged from frame to frame
 *    (such as geometry, hiz, etc.)
 *  - dynamic part defines resources that potentially change each frame 
 */

struct CullViewStaticDescription
{
    const SceneGeometry* Geometry{nullptr};
    bool CullTriangles{false};
};

struct CullViewDynamicDescription
{
    glm::uvec2 Resolution{};
    const Camera* Camera{nullptr};
    bool ClampDepth{false};
    RG::DrawExecutionInfo DrawInfo{};
};

struct CullViewDescription
{
    CullViewStaticDescription Static{};
    CullViewDynamicDescription Dynamic{};
};

struct CullViewDataGPU
{
    static constexpr u32 IS_ORTHOGRAPHIC_BIT = 0;
    static constexpr u32 CLAMP_DEPTH_BIT = 1;
    static constexpr u32 TRIANGLE_CULLING_BIT = 2;
    
    glm::mat4 ViewMatrix;
    glm::mat4 ViewProjectionMatrix;
    FrustumPlanes FrustumPlanes;
    ProjectionData ProjectionData;
    glm::vec2 Resolution;

    u32 ViewFlags{0};

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
    void SetPrimaryView(u32 index) { m_PrimaryView = index; }
    void Finalize();
    
    void NextFrame();
    void UpdateView(u32 viewIndex, const CullViewDynamicDescription& description);

    u32 ViewCount() const { return (u32)m_Views.size(); }
    u32 TriangleViewCount() const { return (u32)m_TriangleViews.size(); }

    const CullViewDescription& View(u32 index) const { return m_Views[index]; }
    CullViewDescription& View(u32 index) { return m_Views[index]; }
    
    const CullViewDescription& TriangleView(u32 index) const { return m_Views[m_TriangleViews[index]]; }
    CullViewDescription& TriangleView(u32 index) { return m_Views[m_TriangleViews[index]]; }
    
    const std::vector<const SceneGeometry*>& Geometries() const { return m_Geometries; }
    const std::vector<ViewSpan>& ViewSpans() const { return m_ViewSpans; }
    const std::vector<ViewSpan>& TriangleViewSpans() const { return m_TriangleViewSpans; }
    const std::vector<CullViewVisibility>& Visibilities() const { return m_CullVisibilities; }
    const std::vector<CullViewTriangleVisibility>& TriangleVisibilities() const { return m_CullTriangleVisibilities; }

    
    std::vector<CullViewDataGPU> CreateMultiviewGPU() const;
    std::vector<CullViewDataGPU> CreateMultiviewGPUTriangles() const;

    bool IsPrimaryView(u32 index) const { return index == m_PrimaryView; }
    bool IsPrimaryTriangleView(u32 index) const { return m_TriangleViews[index] == m_PrimaryView; }

    void UpdateBatchIterationCount();
    u32 GetBatchIterationCount() const;
private:
    void ValidateViewRenderingAttachments(u32 lastViewIndex) const;
private:
    u32 m_PrimaryView{std::numeric_limits<u32>::max()};
    std::vector<CullViewDescription> m_Views;
    /* indices into m_Views */
    std::vector<u32> m_TriangleViews;

    bool m_IsFinalized{false};
    std::vector<const SceneGeometry*> m_Geometries;
    std::vector<ViewSpan> m_ViewSpans;
    std::vector<ViewSpan> m_TriangleViewSpans;
    std::vector<CullViewVisibility> m_CullVisibilities;
    std::vector<CullViewTriangleVisibility> m_CullTriangleVisibilities;
};
