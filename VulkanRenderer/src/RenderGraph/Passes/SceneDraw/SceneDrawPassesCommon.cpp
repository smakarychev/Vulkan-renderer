#include "SceneDrawPassesCommon.h"

#include "CameraGPU.h"
#include "RenderGraph/RGUtils.h"

void SceneDrawPassResources::CreateFrom(const SceneDrawPassExecutionInfo& info, RG::Graph& renderGraph)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    MaxDrawCount = (u32)(renderGraph.GetBufferDescription(info.Draws).SizeBytes / sizeof(IndirectDrawCommand));
    
    Camera = renderGraph.CreateResource("Camera"_hsv,
        GraphBufferDescription{.SizeBytes = sizeof(CameraGPU)});
    Camera = renderGraph.Read(Camera, Vertex | Pixel | Uniform);
    CameraGPU cameraGPU = CameraGPU::FromCamera(*info.Camera, info.Resolution);
    renderGraph.Upload(Camera, cameraGPU);

    Draws = renderGraph.Read(info.Draws, Vertex | Indirect);
    DrawInfo = renderGraph.Read(info.DrawInfo, Vertex | Indirect);
            
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

void SceneDrawPassViewAttachments::Add(StringId viewName, StringId passName, const RG::DrawAttachments& attachments)
{
    m_Attachments[viewName][passName] = attachments;
}
