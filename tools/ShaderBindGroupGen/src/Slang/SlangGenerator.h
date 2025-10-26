#pragma once

#include "v2/Shaders/SlangShaderAsset.h"

class SlangUniformTypeGenerator;

struct SlangGeneratorResult
{
    std::string Generated{};
    std::string FileName{};
};

class SlangGenerator
{
public:
    SlangGenerator(SlangUniformTypeGenerator& uniformTypeGenerator);
    std::string GenerateCommonFile() const;
    std::filesystem::path GetCommonFilePath(const std::filesystem::path& generationPath) const;
    assetlib::io::IoResult<SlangGeneratorResult> Generate(const assetlib::ShaderHeader& shader) const;
private:
    SlangUniformTypeGenerator* m_UniformTypeGenerator{nullptr};
};
