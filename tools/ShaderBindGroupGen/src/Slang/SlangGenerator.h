#pragma once

#include <AssetLib/Io/AssetIo.h>

namespace lux::import
{
struct Context;
}

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
    SlangGenerator(SlangUniformTypeGenerator& uniformTypeGenerator, const std::shared_ptr<lux::import::Context>& ctx);
    std::string GenerateCommonFile() const;
    std::filesystem::path GetCommonFilePath(const std::filesystem::path& generationPath) const;
    lux::assetlib::io::IoResult<SlangGeneratorResult> Generate(const std::filesystem::path& loadInfoPath);
private:
    SlangUniformTypeGenerator* m_UniformTypeGenerator{nullptr};
    std::shared_ptr<lux::import::Context> m_Ctx{nullptr};
};
