#pragma once

#include <AssetLib/Io/AssetIo.h>

#include <filesystem>

namespace lux::assetlib::io
{
class AssetCompressor;
class AssetIoInterface;
}

namespace lux::import
{
using IoError = assetlib::io::IoError;
template <typename T>
using IoResult = assetlib::io::IoResult<T>;

struct Context
{
    std::filesystem::path InitialDirectory{};
    std::filesystem::path BakedDirectory{};
    assetlib::io::AssetIoInterface* Io{nullptr};
    assetlib::io::AssetCompressor* Compressor{nullptr};
};
}
