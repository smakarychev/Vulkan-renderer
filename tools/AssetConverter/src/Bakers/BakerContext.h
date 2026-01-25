#pragma once

#include "v2/Io/AssetIo.h"

#include <filesystem>

namespace lux::assetlib::io
{
class AssetCompressor;
class AssetIoInterface;
}

namespace lux::bakers
{
using IoError = assetlib::io::IoError;
template <typename T>
using IoResult = assetlib::io::IoResult<T>;

struct Context
{
    std::filesystem::path InitialDirectory{};
    assetlib::io::AssetIoInterface* Io{nullptr};
    assetlib::io::AssetCompressor* Compressor{nullptr};
};
}
