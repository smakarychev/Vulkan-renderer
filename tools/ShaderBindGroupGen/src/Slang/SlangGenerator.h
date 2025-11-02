#pragma once

#include "v2/AssetLibV2.h"

class SlangUniformTypeGenerator;

struct SlangGeneratorResult
{
    std::string Generated{};
    std::string FileName{};
};

class SlangGenerator
{
public:
    SlangGenerator(SlangUniformTypeGenerator& uniformTypeGenerator, const std::filesystem::path& shadersDirectory);
    std::string GenerateCommonFile() const;
    std::filesystem::path GetCommonFilePath(const std::filesystem::path& generationPath) const;
    assetlib::io::IoResult<SlangGeneratorResult> Generate(const std::filesystem::path& path) const;
private:
    SlangUniformTypeGenerator* m_UniformTypeGenerator{nullptr};
    std::filesystem::path m_ShadersDirectory{};
};
