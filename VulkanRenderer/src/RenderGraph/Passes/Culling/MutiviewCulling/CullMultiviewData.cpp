#include "CullMultiviewData.h"

#include <algorithm>
#include <ranges>

CullViewDataGPU CullViewDataGPU::FromCullViewDescription(const CullViewDescription& description)
{
    u32 viewFlags = {};
    viewFlags |= (u32)(description.Dynamic.Camera->GetType() == CameraType::Orthographic) << IS_ORTHOGRAPHIC_BIT;
    viewFlags |= (u32)description.Dynamic.ClampDepth << CLAMP_DEPTH_BIT;
    viewFlags |= (u32)description.Static.CullTriangles << TRIANGLE_CULLING_BIT;
    return {
        .ViewMatrix = description.Dynamic.Camera->GetView(),
        .ViewProjectionMatrix = description.Dynamic.Camera->GetViewProjection(),
        .FrustumPlanes = description.Dynamic.Camera->GetFrustumPlanes(),
        .ProjectionData = description.Dynamic.Camera->GetProjectionData(),
        .Resolution = glm::vec2{description.Dynamic.Resolution},
        .HiZResolution = glm::vec2{description.Static.HiZContext->GetHiZResolution()}, 
        .ViewFlags = viewFlags};
}

u32 CullMultiviewData::AddView(const CullViewStaticDescription& description)
{
    ASSERT(!m_IsFinalized, "Call to `AddView` on already finalized data")

    u32 index = (u32)m_Views.size();
    m_Views.push_back({.Static = description});

    u32 triangleIndex = IndexData::INVALID_INDEX;
    if (description.CullTriangles)
    {
        triangleIndex = (u32)m_TriangleViews.size();
        m_TriangleViews.push_back({.Static = description});
    }

    m_IndexData.push_back({
        .ViewIndex = index,
        .TriangleViewIndex = triangleIndex});

    return index;
}

void CullMultiviewData::Finalize()
{
    ASSERT(!m_IsFinalized, "Call to `Finalize` on already finalized data")
    ASSERT(!m_Views.empty(), "CullMultiviewData must have at least one view")
    m_IsFinalized = true;

    auto sortAndCalculateViewSpans = [](std::vector<CullViewDescription>& views, std::vector<ViewSpan>& spans)
    {
        if (views.empty())
            return;
        
        std::ranges::sort(views, std::less{}, [](const CullViewDescription& view) { return view.Static.Geometry; });

        const SceneGeometry* lastGeometry = views.front().Static.Geometry;
        spans.push_back({.First = 0, .Count = 0});
        for (u32 i = 0; i < views.size(); i++)
        {
            if (views[i].Static.Geometry != lastGeometry)
            {
                lastGeometry = views[i].Static.Geometry;
                spans.push_back({.First = i, .Count = 1});
            }
            else
            {
                spans.back().Count++;
            }
        }
    };

    sortAndCalculateViewSpans(m_Views, m_ViewSpans);
    sortAndCalculateViewSpans(m_TriangleViews, m_TriangleViewSpans);

    m_Geometries.push_back(m_Views.front().Static.Geometry);
    for (u32 i = 0; i < m_Views.size(); i++)
        if (m_Views[i].Static.Geometry != m_Geometries.back())
            m_Geometries.push_back(m_Views[i].Static.Geometry);

    ASSERT(m_Geometries.size() <= MAX_CULL_GEOMETRIES,
        "CullMultiviewData must have no more than {} geometries", MAX_CULL_GEOMETRIES)
    ASSERT(m_Views.size() + m_Geometries.size() <= MAX_CULL_VIEWS,
        "CullMultiviewData must have no more than {} - {} ({}) user views, "
        "one view per geometry is resereved for internal use",
        MAX_CULL_VIEWS, m_Geometries.size(), MAX_CULL_VIEWS - m_Geometries.size())
    
    m_CullVisibilities.reserve(m_Views.size());
    for (auto& view : m_Views)
    {
        m_CullVisibilities.emplace_back(*view.Static.Geometry);
        /* this should be pointer safe, since we do reserve memory */
        if (view.Static.CullTriangles)
            m_CullTriangleVisibilities.emplace_back(&m_CullVisibilities.back());
    }
}

void CullMultiviewData::UpdateView(u32 viewIndex, const CullViewDynamicDescription& description)
{
    IndexData indexData = m_IndexData[viewIndex];
    
    m_Views[indexData.ViewIndex].Dynamic = description;
    if (indexData.TriangleViewIndex != IndexData::INVALID_INDEX)
        m_TriangleViews[indexData.TriangleViewIndex].Dynamic = description;
    
    ValidateViewRenderingAttachments(viewIndex);
}

void CullMultiviewData::UpdateViewHiZ(u32 viewIndex, std::shared_ptr<HiZPassContext> context)
{
    IndexData indexData = m_IndexData[viewIndex];
    
    m_Views[indexData.ViewIndex].Static.HiZContext = context;
    if (indexData.TriangleViewIndex != IndexData::INVALID_INDEX)
        m_TriangleViews[indexData.TriangleViewIndex].Static.HiZContext = context;
}

std::vector<CullViewDataGPU> CullMultiviewData::CreateMultiviewGPU() const
{
    std::vector<CullViewDataGPU> views;
    views.reserve(m_Views.size());
    for (auto& v : m_Views)
        views.emplace_back(CullViewDataGPU::FromCullViewDescription(v));

    return views;
}

std::vector<CullViewDataGPU> CullMultiviewData::CreateMultiviewGPUTriangles() const
{
    std::vector<CullViewDataGPU> views;
    views.reserve(m_TriangleViews.size());
    for (auto& v : m_TriangleViews)
        views.emplace_back(CullViewDataGPU::FromCullViewDescription(v));

    return views;
}

void CullMultiviewData::UpdateBatchIterationCount()
{
    for (auto& v : m_CullTriangleVisibilities)
        v.UpdateIterationCount();
}

u32 CullMultiviewData::GetBatchIterationCount() const
{
    u32 max = 0;
    for (auto& v: m_CullTriangleVisibilities)
        max = std::max(max, v.IterationCount());

    return max;
}

void CullMultiviewData::ValidateViewRenderingAttachments(u32 lastViewIndex) const
{
    auto& toCompareAttachments = m_Views[lastViewIndex].Dynamic.DrawInfo.Attachments;
    for (u32 viewIndex = 0; viewIndex < lastViewIndex; viewIndex++)
    {
        auto& attachments = m_Views[viewIndex].Dynamic.DrawInfo.Attachments;
        for (auto& color : toCompareAttachments.Colors)
        {
            auto colorIt = std::ranges::find(attachments.Colors, color.Resource,
                [](auto& other){ return other.Resource; });
            if (colorIt != attachments.Colors.end())
                ASSERT(colorIt->Description.Subresource != color.Description.Subresource,
                    "Using the same resource and the same subresource as a draw target")
        }
        if (toCompareAttachments.Depth.has_value())
        {
            if (attachments.Depth.has_value() && attachments.Depth->Resource == toCompareAttachments.Depth->Resource)
                ASSERT(attachments.Depth->Description.Subresource !=
                    toCompareAttachments.Depth->Description.Subresource,
                    "Using the same resource and the same subresource as a depth target")
        }
    }
}
