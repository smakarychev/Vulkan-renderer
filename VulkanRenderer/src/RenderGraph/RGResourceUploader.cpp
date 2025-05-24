#include "RGResourceUploader.h"

#include "RGGraph.h"

namespace RG
{
    void ResourceUploader::Upload(const Pass& pass, const Graph& graph, ::ResourceUploader& uploader)
    {
        /* at the time of submitting, we do not know if the pass will be culled,
         * so it is possible that some resources were not allocated
         */
        for (auto& upload : m_Uploads[&pass].UploadInfos)
        {
            if (!graph.IsBufferAllocated(upload.Resource))
                continue;

            uploader.UpdateBuffer(graph.GetBuffer(upload.Resource),
                Span(&m_Uploads[&pass].UploadData[upload.SourceOffset], upload.SizeBytes), upload.DestinationOffset);
        }

        m_Uploads.erase(&pass);
    }

    bool ResourceUploader::HasUploads(const Pass& pass) const
    {
        const auto it = m_Uploads.find(&pass);
        
        return it != m_Uploads.end() && !it->second.UploadInfos.empty();
    }
}


