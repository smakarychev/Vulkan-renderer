#include "CullMultiviewUtils.h"

#include "RenderGraph/RGDrawResources.h"

namespace utils
{
    void recordUpdatedAttachmentResources(const RG::DrawAttachments& old, const RG::DrawAttachmentResources& updated,
        AttachmentsRenames& renames)
    {
        for (u32 i = 0; i < old.Colors.size(); i++)
            if (old.Colors[i].Resource != updated.Colors[i])
                renames[old.Colors[i].Resource] = updated.Colors[i];
        if (old.Depth.has_value())
            if (old.Depth->Resource != *updated.Depth)
                renames[old.Depth->Resource] = *updated.Depth;
    }

    void updateRecordedAttachmentResources(RG::DrawAttachments& attachments,
        AttachmentsRenames& renames)
    {
        for (auto& color : attachments.Colors)
            while (renames.contains(color.Resource))
                color.Resource = renames.at(color.Resource);
        if (attachments.Depth.has_value())
            while (renames.contains(attachments.Depth->Resource))
                attachments.Depth->Resource = renames.at(attachments.Depth->Resource);
    }
}
