#pragma once
#include <unordered_map>

#include "RenderGraph/RGResource.h"

namespace RG
{
    struct DrawAttachmentResources;
    struct DrawAttachments;
}

namespace utils
{
    using AttachmentsRenames = std::unordered_map<RG::Resource, RG::Resource>;
    
    void recordUpdatedAttachmentResources(const RG::DrawAttachments& old,
        const RG::DrawAttachmentResources& updated, AttachmentsRenames& renames);
    void updateRecordedAttachmentResources(RG::DrawAttachments& attachments,
        AttachmentsRenames& renames);
}
