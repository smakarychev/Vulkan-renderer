#pragma once

#include "Containers/Span.h"
#include "v2/Io/AssetIo.h"

namespace lux::assetlib::io
{
class AssetIoInterface
{
public:
    virtual ~AssetIoInterface() = default;
    virtual IoResult<void> WriteHeader(const AssetFile& file) = 0;
    virtual IoResult<u64> WriteBinaryChunk(const AssetFile& file, Span<const std::byte> binaryDataChunk) = 0;

    virtual IoResult<AssetFile> ReadHeader(const std::filesystem::path& headerPath) = 0;
    virtual IoResult<void> ReadBinaryChunk(const AssetFile& file, std::byte* destination, u64 offsetBytes,
        u64 sizeBytes) = 0;

    virtual std::string GetHeaderExtension(std::string_view preferred) const = 0;
    virtual std::string GetBinariesExtension() const = 0;

    virtual const std::string& GetName() const = 0;
    virtual const Guid& GetGuid() const = 0;
};
}

#define DEFINE_ASSET_IO_INTERFACE_NAME(_x, _guid) \
    static const std::string& GetNameStatic() { static const std::string name = _x; return name; } \
    const std::string& GetName() const override { return GetNameStatic(); } \
    static const Guid& GetGuidStatic() { static constexpr Guid guid = _guid; return guid; } \
    const Guid& GetGuid() const override { return GetGuidStatic(); }