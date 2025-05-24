#pragma once

#include "ResourceUploader.h"
#include "RGResource.h"

class ResourceUploader;

namespace RG
{
    class Pass;
    /* the resources of render graph are virtual during setup phase,
     * this class is used to record and redirect uploads to physical resources once they're present
     */
    class ResourceUploader
    {
    public:
        template <typename T>
        void UpdateBuffer(const Pass& pass, Resource buffer, T&& data, u64 bufferOffset = 0);
        void Upload(const Pass& pass, const Graph& graph, ::ResourceUploader& uploader);
        bool HasUploads(const Pass& pass) const;
    private:
        struct PassUploadInfo
        {
            struct UploadInfo
            {
                Resource Resource{};
                u64 SizeBytes{};
                u64 SourceOffset{};
                u64 DestinationOffset{};
            };
            std::vector<UploadInfo> UploadInfos;
            std::vector<std::byte> UploadData;
        };

        std::unordered_map<const Pass*, PassUploadInfo> m_Uploads;
    };

    template <typename T>
    void ResourceUploader::UpdateBuffer(const Pass& pass, Resource buffer, T&& data, u64 bufferOffset)
    {
        if constexpr(std::is_pointer_v<T>)
            LOG("Warning: passing a pointer to `UpdateBuffer`");

        auto& upload = m_Uploads[&pass];
        
        u64 sourceOffset = upload.UploadData.size();
        auto&& [address, sizeBytes] = UploadUtils::getAddressAndSize(std::forward<T>(data));

        upload.UploadData.insert(upload.UploadData.end(),
                        (const std::byte*)address, (const std::byte*)address + sizeBytes);
        upload.UploadInfos.push_back({
            .Resource = buffer,
            .SizeBytes = sizeBytes,
            .SourceOffset = sourceOffset,
            .DestinationOffset = bufferOffset});
    }
}


