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

void SceneDrawPassViewAttachments::UpdateResources(const RG::DrawAttachments& old,
    const RG::DrawAttachmentResources& resources)
{
    for (auto&& [view, passes] : m_Attachments)
    {
        for (auto&& [pass, attachments] : passes)
        {
            for (u32 i = 0; i < attachments.Colors.size(); i++)
            {
                auto colorIt = std::ranges::find_if(old.Colors, [&](const RG::DrawAttachment& color) {
                    return color.Resource == attachments.Colors[i].Resource;
                });
                if (colorIt != old.Colors.end())
                    attachments.Colors[i].Resource = resources.Colors[colorIt - old.Colors.begin()];
            }
            if (old.Depth && attachments.Depth && old.Depth->Resource == attachments.Depth->Resource)
                attachments.Depth->Resource = resources.Depth.value_or(RG::Resource{});
        }
    }
}
