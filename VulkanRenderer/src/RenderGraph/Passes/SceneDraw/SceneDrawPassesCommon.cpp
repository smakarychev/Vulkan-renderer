#include "SceneDrawPassesCommon.h"

#include "CameraGPU.h"
#include "RenderGraph/RGUtils.h"

void SceneDrawPassResources::CreateFrom(const SceneDrawPassExecutionInfo& info, RG::Graph& renderGraph)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    MaxDrawCount = (u32)(renderGraph.GetBufferDescription(info.Draws).SizeBytes / sizeof(IndirectDrawCommand));
    
    Camera = renderGraph.Create("Camera"_hsv,
        RGBufferDescription{.SizeBytes = sizeof(CameraGPU)});
    Camera = renderGraph.ReadBuffer(Camera, Vertex | Pixel | Uniform);
    CameraGPU cameraGPU = CameraGPU::FromCamera(*info.Camera, info.Resolution);
    renderGraph.Upload(Camera, cameraGPU);

    Draws = renderGraph.ReadBuffer(info.Draws, Vertex | Indirect);
    DrawInfo = renderGraph.ReadBuffer(info.DrawInfo, Vertex | Indirect);
            
    Attachments = RgUtils::readWriteDrawAttachments(info.Attachments, renderGraph);
}

const RG::DrawAttachments& SceneDrawPassViewAttachments::Get(StringId viewName, StringId passName) const
{
    return m_Attachments.at(viewName).at(passName);
}

RG::DrawAttachments& SceneDrawPassViewAttachments::Get(StringId viewName, StringId passName)
{
    return const_cast<RG::DrawAttachments&>(
        const_cast<const SceneDrawPassViewAttachments&>(*this).Get(viewName, passName));
}

RG::Resource SceneDrawPassViewAttachments::GetMinMaxDepthReduction(StringId viewName) const
{
    return m_MinMaxDepthReductions.at(viewName);
}

void SceneDrawPassViewAttachments::Add(StringId viewName, StringId passName, const RG::DrawAttachments& attachments)
{
    m_Attachments[viewName][passName] = attachments;
}

void SceneDrawPassViewAttachments::SetMinMaxDepthReduction(StringId viewName, RG::Resource minMaxDepthReduction)
{
    m_MinMaxDepthReductions[viewName] = minMaxDepthReduction;
}
