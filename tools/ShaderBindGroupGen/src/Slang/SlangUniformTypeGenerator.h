#pragma once

#include "v2/AssetLibV2.h"

#include <filesystem>
#include <string>
#include <unordered_map>

struct SlangUniformTypeGeneratorInitInfo
{
    std::filesystem::path SearchPath{};
    std::filesystem::path GenerationPath{};
    std::string TypesDirectoryName{};
};

struct SlangUniformTypeGeneratorResult
{
    std::unordered_map<assetlib::AssetId, std::string> EmbeddedStructs{};
    std::string TypeName{};
    std::string ParameterName{};
    std::vector<std::filesystem::path> Includes;
    bool IsStandalone{false};
};

class SlangUniformTypeGenerator
{
public:
    assetlib::io::IoResult<void> GenerateStandaloneUniforms(const SlangUniformTypeGeneratorInitInfo& info);
    assetlib::io::IoResult<SlangUniformTypeGeneratorResult> Generate(const std::string& uniform) const;
private:
    assetlib::io::IoResult<void> WriteStandaloneUniformType(const std::filesystem::path& path,
        const std::filesystem::path& outputPath) const;
private:
    std::filesystem::path m_TypeSearchPath{};
    std::filesystem::path m_GenerationPath{};
    std::filesystem::path m_TypesOutputPath{};
    struct FileInfo
    {
        std::filesystem::path UniformPath{};
        std::filesystem::path OutputPath{};
    };
    /* this cache stores the path to processed uniform structs, that are annotated with `ReflectType` in a shader */
    std::unordered_map<std::string, FileInfo> m_UniformTypesCache{};
};
