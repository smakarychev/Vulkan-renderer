#include "RGResourceUploader.h"

#include "RenderGraph.h"
#include "ResourceUploader.h"

namespace RG
{
    void ResourceUploader::Upload(const Pass* pass, const Resources& resources, ::ResourceUploader& uploader)
    {
        /* at the time of submitting we do not know if the pass will be culled,
         * so it is possible that some of the resources were not actually allocated
         */
        for (auto& upload : m_Uploads[pass].UploadInfos)
        {
            if (!resources.IsAllocated(upload.Resource))
                continue;

            uploader.UpdateBuffer(resources.GetBuffer(upload.Resource),
                Span(&m_Uploads[pass].UploadData[upload.CopyInfo.SourceOffset],
                    upload.CopyInfo.SizeBytes), upload.CopyInfo.DestinationOffset);
        }

        m_Uploads.erase(pass);
    }

    bool ResourceUploader::HasUploads(const Pass* pass) const
    {
        auto it = m_Uploads.find(pass);
        
        return it != m_Uploads.end() && !it->second.UploadInfos.empty();
    }
}


