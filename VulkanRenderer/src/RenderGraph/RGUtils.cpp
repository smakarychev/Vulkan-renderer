#include "rendererpch.h"

#include "RGUtils.h"

#include "RGGraph.h"

namespace RG::RgUtils
{
    Resource ensureResource(Resource resource, Graph& graph, StringId name, const RGImageDescription& fallback)
    {
        return resource.IsValid() ?
            resource :
            graph.Create(name, fallback);
    }

    Resource ensureResource(Resource resource, Graph& graph, StringId name, const RGBufferDescription& fallback)
    {
        return resource.IsValid() ?
            resource :
            graph.Create(name, fallback);
    }

    DrawAttachmentResources readWriteDrawAttachments(const DrawAttachments& attachments, Graph& graph)
    {
        DrawAttachmentResources drawAttachmentResources = {};
        drawAttachmentResources.Colors.reserve(attachments.Colors.size());
        
        for (auto& attachment : attachments.Colors)
        {
            Resource resource = attachment.Resource;
            drawAttachmentResources.Colors.push_back(graph.RenderTarget(resource, attachment.Description));
        }
        if (attachments.Depth.has_value())
        {
            auto& attachment = *attachments.Depth;
            Resource resource = attachment.Resource;
            drawAttachmentResources.Depth = graph.DepthStencilTarget(
                resource, attachment.Description, attachment.DepthBias);
        }

        return drawAttachmentResources;
    }
}
