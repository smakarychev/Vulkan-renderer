#include "SlangUniformTypeGenerator.h"

#include "GeneratorUtils.h"
#include "Utils/HashFileUtils.h"
#include "Utils/HashUtils.h"
#include "v2/Shaders/ShaderUniform.h"

#include <fstream>
#include <ranges>

namespace
{
template<class... Ts>
struct Overload : Ts... { using Ts::operator()...; };

std::string_view shaderScalarTypeToString(lux::assetlib::ShaderScalarType type)
{
    switch (type)
    {
    case lux::assetlib::ShaderScalarType::Bool: return "b32";
    case lux::assetlib::ShaderScalarType::I32: return "i32";
    case lux::assetlib::ShaderScalarType::U32: return "u32";
    case lux::assetlib::ShaderScalarType::I64: return "i64";
    case lux::assetlib::ShaderScalarType::U64: return "u64";
    case lux::assetlib::ShaderScalarType::F32: return "f32";
    case lux::assetlib::ShaderScalarType::F64: return "f64";
    case lux::assetlib::ShaderScalarType::I8: return "i8";
    case lux::assetlib::ShaderScalarType::U8: return "u8";
    case lux::assetlib::ShaderScalarType::I16: return "i16";
    case lux::assetlib::ShaderScalarType::U16: return "u16";
    default:
        ASSERT(false, "Unsupported scalar type")
        return "none";
    }
}

std::string shaderVectorTypeToString(lux::assetlib::ShaderScalarType scalar, u32 elements)
{
    switch (scalar)
    {
    case lux::assetlib::ShaderScalarType::Bool:
        return std::format("glm::bvec{}", elements);
    case lux::assetlib::ShaderScalarType::I32:
        return std::format("glm::ivec{}", elements);
    case lux::assetlib::ShaderScalarType::U32:
        return std::format("glm::uvec{}", elements);
    case lux::assetlib::ShaderScalarType::F32:
        return std::format("glm::vec{}", elements);
    case lux::assetlib::ShaderScalarType::F64:
        return std::format("glm::dvec{}", elements);
    default:
        ASSERT(false, "Unsupported vector layout type {}", (u32)scalar)
        return "vec";
    }
}

std::string shaderMatrixTypeToString(lux::assetlib::ShaderScalarType scalar, u32 rows, u32 cols)
{
    switch (scalar)
    {
    case lux::assetlib::ShaderScalarType::I32:
        return std::format("glm::imat{}x{}", cols, rows);
    case lux::assetlib::ShaderScalarType::U32:
        return std::format("glm::umat{}x{}", cols, rows);
    case lux::assetlib::ShaderScalarType::F32:
        return std::format("glm::mat{}x{}", cols, rows);
    case lux::assetlib::ShaderScalarType::F64:
        return std::format("glm::dmat{}x{}", cols, rows);
    default:
        ASSERT(false, "Unsupported matrix layout type {}", (u32)scalar)
        return "mat";
    }
}

constexpr std::string_view GEN_NAMESPACE_NAME = "gen";

struct UniformWriter
{
    static constexpr u32 INDENT_SPACES = 4;
    std::stringstream Stream{};
    u32 IndentLevel{0};
    std::vector<std::string> TypeReferences;
    std::unordered_map<lux::assetlib::AssetId, std::string> EmbeddedStructs;
    std::string TypeName{};
    std::string ParameterName{};
    bool IsStandalone{false};
    bool HasGlmDependency{false};
    bool HasArrayDependency{false};

    void Push()
    {
        IndentLevel += INDENT_SPACES;
    }
    void Pop()
    {
        ASSERT(IndentLevel >= INDENT_SPACES)
        IndentLevel -= INDENT_SPACES;
    }

    void WriteIndent()
    {
        Stream << std::string(IndentLevel, ' ');
    }
    void WriteName(const lux::assetlib::ShaderUniformVariable& variable)
    {
        Stream << std::format(" {}", utils::canonicalizeName(variable.Name));
    }
    void WriteNameEOL(const lux::assetlib::ShaderUniformVariable& variable)
    {
        WriteName(variable);
        if (variable.DefaultValue.has_value())
            Stream << "{" << *variable.DefaultValue << "};\n";
        else
            Stream << "{};\n";
    }

    void WriteUniform(const lux::assetlib::ShaderUniform& uniform)
    {
        WriteEmbeddedStructs(uniform);
        
        const lux::assetlib::ShaderUniformTypeVariant& rootType = uniform.Root.Type.Type;
        ASSERT(std::holds_alternative<lux::assetlib::ShaderUniformTypeStructReference>(rootType),
            "It is expected that root variable of a uniform is always a struct")
        WriteVariable(uniform.Root, uniform.Root.Type);

        const lux::assetlib::ShaderUniformTypeStructReference& rootStruct =
            std::get<lux::assetlib::ShaderUniformTypeStructReference>(rootType);
        IsStandalone = !rootStruct.IsEmbedded;
        TypeName = utils::canonicalizeName(rootStruct.TypeName);
        ParameterName = utils::canonicalizeParameterName(rootStruct.TypeName);
    }
    void WriteEmbeddedStructs(const lux::assetlib::ShaderUniform& uniform)
    {
        EmbeddedStructs.clear();
        for (auto& embedded : uniform.EmbeddedStructs)
        {
            WriteStructType(embedded.Struct);
            EmbeddedStructs.emplace(embedded.Id, Stream.str());
            Stream.str("");
        }
    }
    void WriteVariable(const lux::assetlib::ShaderUniformVariable& variable,
        const lux::assetlib::ShaderUniformType& type)
    {
        WriteIndent();
        WriteType(variable, type);
        WriteNameEOL(variable);
    }
    void WriteType(const lux::assetlib::ShaderUniformVariable& variable, const lux::assetlib::ShaderUniformType& type)
    {
        std::visit(Overload{
            [this](const lux::assetlib::ShaderUniformTypeScalar& scalar)
            {
                WriteScalarType(scalar);
            },
            [this](const lux::assetlib::ShaderUniformTypeVector& vector)
            {
                HasGlmDependency = true;
                WriteVectorType(vector);
            },
            [this](const lux::assetlib::ShaderUniformTypeMatrix& matrix)
            {
                HasGlmDependency = true;
                WriteMatrixType(matrix);
            },
            [&variable, this](const std::shared_ptr<lux::assetlib::ShaderUniformTypeArray>& array)
            {
                HasArrayDependency = true;
                WriteArrayPrefix();
                WriteType(variable, array->Element);
                WriteArraySuffix(*array);
            },
            [this](const std::shared_ptr<lux::assetlib::ShaderUniformTypeStruct>& structType)
            {
                WriteStructType(*structType);
            },
            [this](const lux::assetlib::ShaderUniformTypeStructReference& structReference)
            {
                if (!structReference.IsEmbedded &&
                    std::ranges::find(TypeReferences, structReference.Target) == TypeReferences.end())
                    TypeReferences.push_back(structReference.Target);
                if (!structReference.IsEmbedded)
                    Stream << std::format("::{}::", GEN_NAMESPACE_NAME);
                Stream << utils::canonicalizeName(structReference.TypeName);
            },
        }, type.Type);
    }
    void WriteScalarType(const lux::assetlib::ShaderUniformTypeScalar& scalar)
    {
        Stream << shaderScalarTypeToString(scalar.Scalar);
    }
    void WriteVectorType(const lux::assetlib::ShaderUniformTypeVector& vector)
    {
        ASSERT(vector.Elements > 1 && vector.Elements <= 4)
        Stream << shaderVectorTypeToString(vector.Scalar, vector.Elements);
    }
    void WriteMatrixType(const lux::assetlib::ShaderUniformTypeMatrix& matrix)
    {
        ASSERT(matrix.Rows > 1 && matrix.Rows <= 4)
        ASSERT(matrix.Columns > 1 && matrix.Columns <= 4)
        Stream << shaderMatrixTypeToString(matrix.Scalar, matrix.Rows, matrix.Columns);
    }
    void WriteArrayPrefix()
    {
        Stream << "std::array<";
    }
    void WriteArraySuffix(const lux::assetlib::ShaderUniformTypeArray& array)
    {
        Stream << std::format(", {}>", array.Size);
    }
    void WriteStructType(const lux::assetlib::ShaderUniformTypeStruct& structType)
    {
        WriteIndent();
        Stream << std::format("struct {}\n", utils::canonicalizeName(structType.TypeName));
        Stream << "{\n";
        {
            Push();
            for (auto& field : structType.Fields)
                WriteVariable(field, field.Type);

            Stream << "\n";
            WriteIndent();
            Stream <<  std::format("auto operator<=>(const {}&) const = default;\n",
                utils::canonicalizeName(structType.TypeName));
            Pop();
        }
        WriteIndent();
        Stream << "};\n";
    }
};

std::filesystem::path getReflectionPath(
    const std::filesystem::path& outputDirectory,
    const std::filesystem::path& searchDirectory,
    const std::filesystem::path& path)
{
    namespace fs = std::filesystem;
    fs::path reflectionPath = fs::relative(path, searchDirectory);
    if (reflectionPath == std::filesystem::path())
        return reflectionPath;
        
    reflectionPath = utils::canonicalizePath(reflectionPath, "Uniform.generated.h");
    reflectionPath = outputDirectory / reflectionPath;

    return reflectionPath;
}

std::string getIncludePathString(const std::filesystem::path& include, const std::filesystem::path& initialPath) {
    return std::filesystem::proximate(include, initialPath).generic_string();
}
}

lux::assetlib::io::IoResult<void> SlangUniformTypeGenerator::GenerateStandaloneUniforms(
    const SlangUniformTypeGeneratorInitInfo& info)
{
    m_TypeSearchPath = info.SearchPath;
    m_GenerationPath = info.GenerationPath;
    m_TypesOutputPath = info.GenerationPath / info.TypesDirectoryName;

    namespace fs = std::filesystem;

    std::unordered_map<fs::path, fs::path> processedTypeToFileMap;
    
    for (const auto& file : fs::recursive_directory_iterator(m_TypeSearchPath))
    {
        if (file.is_directory())
            continue;

        const fs::path& path = file.path();
        
        if (path.extension() != lux::assetlib::SHADER_UNIFORM_TYPE_EXTENSION)
            continue;

        m_UniformTypesCache.emplace(path.filename().string(), FileInfo{
            .UniformPath = path,
            .OutputPath = getReflectionPath(m_TypesOutputPath, m_TypeSearchPath, path)
        });
    }

    for (auto& paths : m_UniformTypesCache | std::views::values)
    {
        const auto writeResult = WriteStandaloneUniformType(paths.UniformPath, paths.OutputPath);
        if (!writeResult.has_value())
            return std::unexpected(writeResult.error());
    }

    return {};
}

lux::assetlib::io::IoResult<SlangUniformTypeGeneratorResult> SlangUniformTypeGenerator::Generate(
    const std::string& uniform) const
{
    const auto result = lux::assetlib::shader::unpackUniform(uniform);
    if (!result.has_value())
        return std::unexpected(result.error());

    const lux::assetlib::ShaderUniform& variable = *result;
    UniformWriter writer;
    writer.WriteUniform(variable);

    std::vector<std::filesystem::path> includes;
    includes.reserve(writer.TypeReferences.size());
    for (auto& reference : writer.TypeReferences)
        includes.emplace_back(getIncludePathString(m_UniformTypesCache.at(reference).OutputPath, m_GenerationPath));

    return SlangUniformTypeGeneratorResult{
        .EmbeddedStructs = writer.EmbeddedStructs,
        .TypeName = writer.TypeName,
        .ParameterName = writer.ParameterName,
        .Includes = std::move(includes),
        .IsStandalone = writer.IsStandalone
    };
}

lux::assetlib::io::IoResult<void> SlangUniformTypeGenerator::WriteStandaloneUniformType(
    const std::filesystem::path& path, const std::filesystem::path& outputPath) const
{
    std::ifstream in(path.string(), std::ios::binary | std::ios::ate);
    ASSETLIB_CHECK_RETURN_IO_ERROR(in.good(), lux::assetlib::io::IoError::ErrorCode::FailedToOpen,
        "Failed to open uniform file: {}", path.string())
    const isize size = in.tellg();
    in.seekg(0, std::ios::beg);
    std::string content(size, 0);
    in.read(content.data(), size);
    in.close();

    const auto unpackResult = lux::assetlib::shader::unpackUniformStruct(content);
    ASSETLIB_CHECK_RETURN_IO_ERROR(unpackResult.has_value(), unpackResult.error().Code,
        "Failed to open unpack uniform file: {} ({})", unpackResult.error().Message, path.string())

    UniformWriter writer;
    writer.WriteStructType(*unpackResult);

    content.clear();
    content.append(utils::getPreamble()).append("\n");
    for (auto& reference : writer.TypeReferences)
    {
        ASSETLIB_CHECK_RETURN_IO_ERROR(m_UniformTypesCache.contains(reference),
            lux::assetlib::io::IoError::ErrorCode::GeneralError,
            "Failed to write unpack uniform file, the referenced file not found: {} ({})", reference, path.string())

        content.append(
            utils::getIncludeString(
                getIncludePathString(m_UniformTypesCache.at(reference).OutputPath, m_TypesOutputPath))).append("\n");
    }

    if (writer.HasGlmDependency)
        content.append("#include <glm/glm.hpp>\n");
    if (writer.HasArrayDependency)
        content.append("#include <array>\n");

    content.append(std::format("\nnamespace {}\n{{\n", GEN_NAMESPACE_NAME));
    content.append(writer.Stream.str());
    content.append("}\n");
    
    if (!std::filesystem::exists(outputPath.parent_path()))
        std::filesystem::create_directories(outputPath.parent_path());

    if (std::filesystem::exists(outputPath))
    {
        /* avoid touching unchanged files, as some build system do not bother doing it themselves and will recompile */
        const bool hashesAreEqual =
            Hash::murmur3b32File(outputPath) == Hash::murmur3b32((u8*)content.data(), content.size());
        if (hashesAreEqual)
            return {};
    }
    
    std::ofstream out(outputPath, std::ios::binary);
    out.write(content.data(), (isize)content.size());
    
    return {};
}
