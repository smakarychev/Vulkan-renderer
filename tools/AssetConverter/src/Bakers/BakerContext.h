#pragma once

#include "v2/AssetLibV2.h"

#include <filesystem>

namespace bakers
{

using IoError = ::assetlib::io::IoError;
template <typename T>
using IoResult = ::assetlib::io::IoResult<T>;

struct Context
{
    std::filesystem::path InitialDirectory{};
    assetlib::CompressionMode CompressionMode{assetlib::CompressionMode::LZ4};
    assetlib::AssetFileIoType IoType{assetlib::AssetFileIoType::Separate};
};

}

