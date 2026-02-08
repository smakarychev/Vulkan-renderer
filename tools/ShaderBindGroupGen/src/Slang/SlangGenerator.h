#pragma once

#include "v2/Io/AssetIo.h"

namespace lux::assetlib::io
{
class AssetIoInterface;
}

class SlangUniformTypeGenerator;

struct SlangGeneratorResult
{
    std::string Generated{};
    std::string FileName{};
};

class SlangGenerator
{
public:
    SlangGenerator(SlangUniformTypeGenerator& uniformTypeGenerator, const std::filesystem::path& initialDirectory,
        lux::assetlib::io::AssetIoInterface& io);
    std::string GenerateCommonFile() const;
    std::filesystem::path GetCommonFilePath(const std::filesystem::path& generationPath) const;
    lux::assetlib::io::IoResult<SlangGeneratorResult> Generate(const std::filesystem::path& path) const;
private:
    SlangUniformTypeGenerator* m_UniformTypeGenerator{nullptr};
    std::filesystem::path m_InitialDirectory{};
    lux::assetlib::io::AssetIoInterface* m_Io{nullptr};
};
