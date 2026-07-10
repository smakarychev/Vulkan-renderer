#pragma once

#include "RGDrawResources.h"

namespace RG::RgUtils
{
BufferResource ensureResource(BufferResource resource, Graph& graph, StringId name, const RGBufferDescription& fallback);
ImageResource ensureResource(ImageResource resource, Graph& graph, StringId name, const RGImageDescription& fallback);

DrawAttachmentResources readWriteDrawAttachments(const DrawAttachments& attachments, Graph& graph);
}
