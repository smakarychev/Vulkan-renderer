#pragma once

#include "Containers/Span.h"
#include "Containers/Guid.h"

#include <string>

namespace lux::assetlib::io
{
class AssetCompressor
{
public:
    virtual ~AssetCompressor() = default;
    virtual std::vector<std::byte> Compress(Span<const std::byte> data) = 0;
    virtual std::vector<std::byte> Decompress(Span<const std::byte> data, u64 decompressedSize) = 0;

    virtual const std::string& GetName() const = 0;
    virtual const Guid& GetGuid() const = 0;
};
}

#define DEFINE_ASSET_COMPRESSOR_NAME(_x, _guid) \
    static const std::string& GetNameStatic() { static const std::string name = _x; return name; } \
    const std::string& GetName() const override { return GetNameStatic(); } \
    static const Guid& GetGuidStatic() { static constexpr Guid guid = _guid; return guid; } \
    const Guid& GetGuid() const override { return GetGuidStatic(); }