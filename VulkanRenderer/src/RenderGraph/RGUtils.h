#pragma once

#include "RGDrawResources.h"

namespace RG::RgUtils
{
    Resource ensureResource(Resource resource, Graph& graph, StringId name, const RGImageDescription& fallback);    
    Resource ensureResource(Resource resource, Graph& graph, StringId name, const RGBufferDescription& fallback);

    DrawAttachmentResources readWriteDrawAttachments(const DrawAttachments& attachments, Graph& graph);
}
