#include "SlangGenerator.h"

#include "GeneratorUtils.h"
#include "SlangUniformTypeGenerator.h"
#include "Bakers/Shaders/SlangBaker.h"
#include "v2/Shaders/SlangShaderAsset.h"
#include "v2/Shaders/ShaderLoadInfo.h"

#include <ranges>
#include <unordered_set>

namespace
{
std::string_view accessToString(assetlib::ShaderBindingAccess access)
{
    switch (access)
    {
    case assetlib::ShaderBindingAccess::Read: return "Read";
    case assetlib::ShaderBindingAccess::Write: return "Write";
    case assetlib::ShaderBindingAccess::ReadWrite: return "ReadWrite";
    default:
        ASSERT(false)
        return "Read";
    }
}

std::string_view bindingTypeToString(assetlib::ShaderBindingType bindingType)
{
    switch (bindingType)
    {
    case assetlib::ShaderBindingType::Image:
    case assetlib::ShaderBindingType::ImageStorage:
        return "Image";
    case assetlib::ShaderBindingType::TexelUniform:
    case assetlib::ShaderBindingType::TexelStorage:
    case assetlib::ShaderBindingType::UniformBuffer:
    case assetlib::ShaderBindingType::UniformTexelBuffer:
    case assetlib::ShaderBindingType::StorageBuffer:
    case assetlib::ShaderBindingType::StorageTexelBuffer:
        return "Buffer";
    default:
        ASSERT(false)
        return "Buffer";
    }
}

std::string shaderStagesToGraphUsage(assetlib::ShaderStage stage)
{
    std::string stages = {};
    if (enumHasAny(stage, assetlib::ShaderStage::Vertex))
        stages += "Vertex";
    if (enumHasAny(stage, assetlib::ShaderStage::Pixel))
        stages += stages.empty() ? "Pixel" : " | Pixel";
    if (enumHasAny(stage, assetlib::ShaderStage::Compute))
        stages += stages.empty() ? "Compute" : " | Compute";

    return stages;
}

std::string_view bindingTypeToGraphUsage(assetlib::ShaderBindingType bindingType)
{
    switch (bindingType)
    {
    case assetlib::ShaderBindingType::Image:
    case assetlib::ShaderBindingType::TexelUniform:
        return "Sampled";
    case assetlib::ShaderBindingType::ImageStorage:
    case assetlib::ShaderBindingType::TexelStorage:
        return "Storage";
    case assetlib::ShaderBindingType::UniformBuffer:
        return "Uniform";
    case assetlib::ShaderBindingType::UniformTexelBuffer:
    case assetlib::ShaderBindingType::StorageBuffer:
    case assetlib::ShaderBindingType::StorageTexelBuffer:
        return "Storage";
    default:
        ASSERT(false)
        return "";
    }
}

std::string_view bindingTypeToDescriptorTypeString(assetlib::ShaderBindingType bindingType)
{
    switch (bindingType)
    {
    case assetlib::ShaderBindingType::Sampler: return "DescriptorType::Sampler";
    case assetlib::ShaderBindingType::Image: return "DescriptorType::Image";
    case assetlib::ShaderBindingType::ImageStorage: return "DescriptorType::ImageStorage";
    case assetlib::ShaderBindingType::TexelUniform: return "DescriptorType::TexelUniform";
    case assetlib::ShaderBindingType::TexelStorage: return "DescriptorType::TexelStorage";
    case assetlib::ShaderBindingType::UniformBuffer: return "DescriptorType::UniformBuffer";
    case assetlib::ShaderBindingType::StorageBuffer: return "DescriptorType::StorageBuffer";
    case assetlib::ShaderBindingType::UniformBufferDynamic: return "DescriptorType::UniformBufferDynamic";
    case assetlib::ShaderBindingType::StorageBufferDynamic: return "DescriptorType::StorageBufferDynamic";
    case assetlib::ShaderBindingType::Input: return "DescriptorType::Input";
    default:
        ASSERT(false)
        return "";
    }
}

std::string_view formatFromShaderImageFormat(assetlib::ShaderImageFormat format)
{
    switch (format)
    {
    case assetlib::ShaderImageFormat::Undefined: return "Format::Undefined";
    case assetlib::ShaderImageFormat::R8_UNORM: return "Format::R8_UNORM";
    case assetlib::ShaderImageFormat::R8_SNORM: return "Format::R8_SNORM";
    case assetlib::ShaderImageFormat::R8_UINT: return "Format::R8_UINT";
    case assetlib::ShaderImageFormat::R8_SINT: return "Format::R8_SINT";
    case assetlib::ShaderImageFormat::R8_SRGB: return "Format::R8_SRGB";
    case assetlib::ShaderImageFormat::RG8_UNORM: return "Format::RG8_UNORM";
    case assetlib::ShaderImageFormat::RG8_SNORM: return "Format::RG8_SNORM";
    case assetlib::ShaderImageFormat::RG8_UINT: return "Format::RG8_UINT";
    case assetlib::ShaderImageFormat::RG8_SINT: return "Format::RG8_SINT";
    case assetlib::ShaderImageFormat::RG8_SRGB: return "Format::RG8_SRGB";
    case assetlib::ShaderImageFormat::RGBA8_UNORM: return "Format::RGBA8_UNORM";
    case assetlib::ShaderImageFormat::RGBA8_SNORM: return "Format::RGBA8_SNORM";
    case assetlib::ShaderImageFormat::RGBA8_UINT: return "Format::RGBA8_UINT";
    case assetlib::ShaderImageFormat::RGBA8_SINT: return "Format::RGBA8_SINT";
    case assetlib::ShaderImageFormat::RGBA8_SRGB: return "Format::RGBA8_SRGB";
    case assetlib::ShaderImageFormat::R16_UNORM: return "Format::R16_UNORM";
    case assetlib::ShaderImageFormat::R16_SNORM: return "Format::R16_SNORM";
    case assetlib::ShaderImageFormat::R16_UINT: return "Format::R16_UINT";
    case assetlib::ShaderImageFormat::R16_SINT: return "Format::R16_SINT";
    case assetlib::ShaderImageFormat::R16_FLOAT: return "Format::R16_FLOAT";
    case assetlib::ShaderImageFormat::RG16_UNORM: return "Format::RG16_UNORM";
    case assetlib::ShaderImageFormat::RG16_SNORM: return "Format::RG16_SNORM";
    case assetlib::ShaderImageFormat::RG16_UINT: return "Format::RG16_UINT";
    case assetlib::ShaderImageFormat::RG16_SINT: return "Format::RG16_SINT";
    case assetlib::ShaderImageFormat::RG16_FLOAT: return "Format::RG16_FLOAT";
    case assetlib::ShaderImageFormat::RGBA16_UNORM: return "Format::RGBA16_UNORM";
    case assetlib::ShaderImageFormat::RGBA16_SNORM: return "Format::RGBA16_SNORM";
    case assetlib::ShaderImageFormat::RGBA16_UINT: return "Format::RGBA16_UINT";
    case assetlib::ShaderImageFormat::RGBA16_SINT: return "Format::RGBA16_SINT";
    case assetlib::ShaderImageFormat::RGBA16_FLOAT: return "Format::RGBA16_FLOAT";
    case assetlib::ShaderImageFormat::R32_UINT: return "Format::R32_UINT";
    case assetlib::ShaderImageFormat::R32_SINT: return "Format::R32_SINT";
    case assetlib::ShaderImageFormat::R32_FLOAT: return "Format::R32_FLOAT";
    case assetlib::ShaderImageFormat::RG32_UINT: return "Format::RG32_UINT";
    case assetlib::ShaderImageFormat::RG32_SINT: return "Format::RG32_SINT";
    case assetlib::ShaderImageFormat::RG32_FLOAT: return "Format::RG32_FLOAT";
    case assetlib::ShaderImageFormat::RGB32_UINT: return "Format::RGB32_UINT";
    case assetlib::ShaderImageFormat::RGB32_SINT: return "Format::RGB32_SINT";
    case assetlib::ShaderImageFormat::RGB32_FLOAT: return "Format::RGB32_FLOAT";
    case assetlib::ShaderImageFormat::RGBA32_UINT: return "Format::RGBA32_UINT";
    case assetlib::ShaderImageFormat::RGBA32_SINT: return "Format::RGBA32_SINT";
    case assetlib::ShaderImageFormat::RGBA32_FLOAT: return "Format::RGBA32_FLOAT";
    case assetlib::ShaderImageFormat::RGB10A2: return "Format::RGB10A2";
    case assetlib::ShaderImageFormat::R11G11B10: return "Format::R11G11B10";
    case assetlib::ShaderImageFormat::D32_FLOAT: return "Format::D32_FLOAT";
    case assetlib::ShaderImageFormat::D24_UNORM_S8_UINT: return "Format::D24_UNORM_S8_UINT";
    case assetlib::ShaderImageFormat::D32_FLOAT_S8_UINT: return "Format::D32_FLOAT_S8_UINT";
    default:
        ASSERT(false)
        return "Format::Undefined";
    }
}

struct Writer
{
    static constexpr u32 INDENT_SPACES = 4;

    struct CountInfo
    {
        u32 Buffers{0};
        u32 Images{0};
        u32 Samplers{0};
    };

    std::stringstream Stream;
    u32 IndentLevel{0};
    bool HasBindlessImages{false};
    bool HasBindlessBuffers{false};
    CountInfo Counts{};
    std::optional<assetlib::io::IoError> Error;

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

    void WriteLine(std::string_view line)
    {
        WriteIndent();
        Stream << line << "\n";
    }

    void AddError(const std::string& message)
    {
        if (!Error.has_value())
            Error = {.Code = assetlib::io::IoError::ErrorCode::GeneralError, .Message = {}};
        Error->Message.append(message).append("\n");
    }

    void BeginSetResourceFunction(const assetlib::ShaderBinding& binding)
    {
        std::string line = std::format("RG::Resource Set{}(RG::Resource resource",
            utils::canonicalizeName(binding.Name));
        if (binding.Count > 1)
            line.append(", u32 index");
        line.append(", RG::ResourceAccessFlags additionalAccess = RG::ResourceAccessFlags::None)");
        WriteLine(line);
        WriteLine("{");
        Push();
        WriteLine("using enum RG::ResourceAccessFlags;");
    }

    void BeginSetUniformFunction(const assetlib::ShaderBinding& binding, const std::string& uniformType,
        const std::string& uniformParameter)
    {
        std::string line = std::format("RG::Resource Set{}(const {}& {}",
            utils::canonicalizeName(binding.Name), uniformType, uniformParameter);
        if (binding.Count > 1)
            line.append(", u32 index");
        line.append(", RG::ResourceAccessFlags additionalAccess = RG::ResourceAccessFlags::None)");
        WriteLine(line);
        WriteLine("{");
        Push();
        WriteLine("using enum RG::ResourceAccessFlags;");
    }

    void EndSetResourceFunction()
    {
        WriteLine("return resource;");
        Pop();
        WriteLine("}");
    }

    void WriteResourceAccess(const assetlib::ShaderBinding& binding)
    {
        const std::string shaderAccess = shaderStagesToGraphUsage(binding.ShaderStages);
        const std::string_view graphUsage = bindingTypeToGraphUsage(binding.Type);
        if (shaderAccess.empty() || graphUsage.empty())
        {
            AddError(std::format("Failed to infer usage for binding {}", binding.Name));
            return;
        }
        WriteLine(std::format("resource = Graph->{}{}(resource, {} | {} | additionalAccess);",
            accessToString(binding.Access), bindingTypeToString(binding.Type), shaderAccess, graphUsage));
    }

    static std::string GetBindingInfoContentString(const assetlib::ShaderBindingSet& set,
        const assetlib::ShaderBinding& binding)
    {
        return std::format(
            ".Set = {}, "
            ".Slot = {}, "
            ".DescriptorType = {}, "
            ".Resource = resource, "
            ".ArrayOffset = {}",
            set.Set,
            binding.Binding,
            bindingTypeToDescriptorTypeString(binding.Type),
            binding.Count > 1 ? "index" : "0");
    }

    void WriteSampler(const assetlib::ShaderBindingSet& set, const assetlib::ShaderBinding& sampler)
    {
        if (sampler.Count > 1)
        {
            AddError(std::format("Array of samplers are not supported: {}", sampler.Name));
            return;
        }
        if (enumHasAny(sampler.Attributes, assetlib::ShaderBindingAttributes::ImmutableSampler))
            return;

        WriteLine(std::format("void Set{}(Sampler sampler)", utils::canonicalizeName(sampler.Name)));
        WriteLine("{");
        Push();
        WriteLine(std::format("m_SamplerBindings[m_SamplerCount] = SamplerBindingInfoRG{{"
            ".Set = {}, "
            ".Slot = {}, "
            ".Sampler = sampler}};",
            set.Set,
            sampler.Binding,
            bindingTypeToDescriptorTypeString(sampler.Type)));
        WriteLine("m_SamplerCount += 1;");
        Pop();
        WriteLine("}");
        Counts.Samplers += sampler.Count;
    }

    void WriteOrdinaryImage(const assetlib::ShaderBindingSet& set, const assetlib::ShaderBinding& image)
    {
        const std::string line = std::format("m_ImageBindings[m_ImageCount] = ImageBindingInfoRG{{{}}};",
            GetBindingInfoContentString(set, image));
        WriteLine(line);
        WriteLine("m_ImageCount += 1;");
        Counts.Images += image.Count;
    }

    void WriteBindlessImage(const assetlib::ShaderBindingSet& set, const assetlib::ShaderBinding& image)
    {
        HasBindlessImages = true;
        const std::string line = std::format("m_ImageBindlessBindings.push_back(ImageBindingInfoRG{{{}}});",
            GetBindingInfoContentString(set, image));
        WriteLine(line);
    }

    void WriteOrdinaryBuffer(const assetlib::ShaderBindingSet& set, const assetlib::ShaderBinding& buffer)
    {
        const std::string line = std::format("m_BufferBindings[m_BufferCount] = BufferBindingInfoRG{{{}}};",
            GetBindingInfoContentString(set, buffer));
        WriteLine(line);
        WriteLine("m_BufferCount += 1;");
        Counts.Buffers += buffer.Count;
    }

    void WriteBindlessBuffer(const assetlib::ShaderBindingSet& set, const assetlib::ShaderBinding& buffer)
    {
        HasBindlessBuffers = true;
        const std::string line = std::format("m_BufferBindlessBindings.push_back(BufferBindingInfoRG{{{}}});",
            GetBindingInfoContentString(set, buffer));
        WriteLine(line);
    }

    void WriteImage(const assetlib::ShaderBindingSet& set, const assetlib::ShaderBinding& image)
    {
        BeginSetResourceFunction(image);
        WriteResourceAccess(image);
        if (enumHasAny(image.Attributes, assetlib::ShaderBindingAttributes::Bindless))
            WriteBindlessImage(set, image);
        else
            WriteOrdinaryImage(set, image);
        EndSetResourceFunction();
    }

    void WriteBuffer(const assetlib::ShaderBindingSet& set, const assetlib::ShaderBinding& buffer)
    {
        BeginSetResourceFunction(buffer);
        WriteResourceAccess(buffer);
        if (enumHasAny(buffer.Attributes, assetlib::ShaderBindingAttributes::Bindless))
            WriteBindlessBuffer(set, buffer);
        else
            WriteOrdinaryBuffer(set, buffer);
        EndSetResourceFunction();
    }

    void WriteEmbeddedUniformTypes(const std::string& uniform)
    {
        std::stringstream ss(uniform);
        std::string line;
        while (std::getline(ss, line))
            WriteLine(line);
    }

    void WriteUniformBinding(const assetlib::ShaderBindingSet& set, const assetlib::ShaderBinding& uniformBinding,
        const std::string& uniformType, const std::string& uniformParameter)
    {
        BeginSetUniformFunction(uniformBinding, uniformType, uniformParameter);
        WriteLine(std::format("RG::Resource resource = Graph->Create(\"{}\"_hsv, RG::RGBufferDescription{{"
            ".SizeBytes = sizeof({})}});", uniformType, uniformType));
        WriteLine(std::format("resource = Graph->Upload(resource, {});", uniformParameter));
        WriteResourceAccess(uniformBinding);
        if (enumHasAny(uniformBinding.Attributes, assetlib::ShaderBindingAttributes::Bindless))
            WriteBindlessBuffer(set, uniformBinding);
        else
            WriteOrdinaryBuffer(set, uniformBinding);
        EndSetResourceFunction();
    }

    void WriteRasterizationInfo(const assetlib::ShaderLoadRasterizationInfo& rasterization)
    {
        for (auto& color : rasterization.Colors)
            WriteLine(std::format("static Format Get{}AttachmentFormat() {{ return {}; }}",
                utils::canonicalizeName(color.Name), formatFromShaderImageFormat(color.Format)));
        if (rasterization.Depth.has_value())
            WriteLine(std::format("static Format GetDepthFormat() {{ return {}; }}",
                formatFromShaderImageFormat(*rasterization.Depth)));
    }

    void WriteGroupSizeInfo(const std::vector<assetlib::ShaderEntryPoint>& entryPoints)
    {
        for (auto& entry : entryPoints)
        {
            if (entry.ShaderStage != assetlib::ShaderStage::Compute)
                continue;

            WriteLine(std::format("static glm::uvec3 Get{}GroupSize() {{ return glm::uvec3{{{}, {}, {}}}; }}",
               utils::canonicalizeName(entry.Name),
               entry.ThreadGroupSize.x, entry.ThreadGroupSize.y, entry.ThreadGroupSize.z));
        }
    }

    void BeginForLoop(std::string_view count)
    {
        WriteLine(std::format("for (u32 i = 0; i < {}; i++)", count));
        WriteLine("{");
        Push();
    }

    void BeginForEachLoop(std::string_view range)
    {
        WriteLine(std::format("for (auto& binding : {})", range));
        WriteLine("{");
        Push();
    }

    void EndForLoop()
    {
        Pop();
        WriteLine("}");
    }

    void WriteBeginPrivate()
    {
        Pop();
        WriteLine("");
        WriteLine("private:");
        Push();
    }

    void WriteBindDescriptorsType(const assetlib::ShaderHeader& shader, const std::string& type)
    {
        WriteLine(std::format(
            "void Bind{}Descriptors(RenderCommandList& cmdList, const DescriptorArenaAllocators& allocators) const",
            type));
        WriteLine("{");
        Push();
        if (Counts.Samplers > 0)
        {
            BeginForLoop("m_SamplerCount");
            WriteLine("const auto& binding = m_SamplerBindings[i];");
            WriteLine("Device::UpdateDescriptors(Shader->Descriptors(binding.Set), "
                "DescriptorSlotInfo{.Slot = binding.Slot, .Type = DescriptorType::Sampler}, sampler);");
            EndForLoop();
        }
        if (Counts.Buffers > 0)
        {
            BeginForLoop("m_BufferCount");
            WriteLine("const auto& binding = m_BufferBindings[i];");
            WriteLine("Device::UpdateDescriptors(Shader->Descriptors(binding.Set), "
                "DescriptorSlotInfo{.Slot = binding.Slot, .Type = binding.DescriptorType}, "
                "Graph->GetBufferBinding(binding.Resource), binding.ArrayOffset);");
            EndForLoop();
        }
        if (HasBindlessBuffers)
        {
            BeginForEachLoop("m_BufferBindlessBindings");
            WriteLine("Device::UpdateDescriptors(Shader->Descriptors(binding.Set), "
                "DescriptorSlotInfo{.Slot = binding.Slot, .Type = binding.DescriptorType}, "
                "Graph->GetBufferBinding(binding.Resource), binding.ArrayOffset);");
            EndForLoop();
        }
        if (Counts.Images > 0)
        {
            BeginForLoop("m_ImageCount");
            WriteLine("const auto& binding = m_ImageBindings[i];");
            WriteLine("const auto& image = Graph->GetImageBinding(binding.Resource);");
            WriteLine("Device::UpdateDescriptors(Shader->Descriptors(binding.Set), "
                "DescriptorSlotInfo{.Slot = binding.Slot, .Type = binding.DescriptorType}, "
                "image.Subresource, image.Layout, binding.ArrayOffset);");
            EndForLoop();
        }
        if (HasBindlessImages)
        {
            BeginForEachLoop("m_ImageBindlessBindings");
            WriteLine("const auto& image = Graph->GetImageBinding(binding.Resource);");
            WriteLine("Device::UpdateDescriptors(Shader->Descriptors(binding.Set), "
                "DescriptorSlotInfo{.Slot = binding.Slot, .Type = binding.DescriptorType}, "
                "image.Subresource, image.Layout, binding.ArrayOffset);");
            EndForLoop();
        }
        for (auto&& [i, set] : std::views::enumerate(shader.BindingSets))
        {
            const bool hasImmutableSampler = std::ranges::any_of(set.Bindings, 
                [](const assetlib::ShaderBinding& binding) {
                    return enumHasAny(binding.Attributes, assetlib::ShaderBindingAttributes::ImmutableSampler);
                });
            if (hasImmutableSampler)
                WriteLine(std::format(
                    "cmdList.BindImmutableSamplers{}({{"
                    ".Descriptors = Shader->Descriptors({}), .PipelineLayout = Shader->GetLayout(), .Set = {}}});",
                    type, set.Set, set.Set));
            else
                WriteLine(std::format(
                    "cmdList.BindDescriptors{}({{"
                    ".Descriptors = Shader->Descriptors({}), .Allocators = &allocators, "
                    ".PipelineLayout = Shader->GetLayout(), .Set = {}}});",
                    type, set.Set, set.Set));
        }
        Pop();
        WriteLine("}");
    }

    void WriteBindPipelineType(const std::string& type)
    {
        WriteLine(std::format(
            "void Bind{}(RenderCommandList& cmdList, const DescriptorArenaAllocators& allocators) const",
            type));
        WriteLine("{");
        Push();
        WriteLine(std::format("cmdList.BindPipeline{}({{.Pipeline = Shader->Pipeline()}});", type));
        WriteLine(std::format("Bind{}Descriptors(cmdList, allocators);", type));
        Pop();
        WriteLine("}");
    }

    void WriteResourceContainers()
    {
        if (Counts.Samplers > 0)
        {
            WriteLine(std::format("std::array<SamplerBindingInfoRG, {}> m_SamplerBindings{{}};", Counts.Samplers));
            WriteLine("u32 m_SamplerCount{};");
        }
        if (Counts.Buffers > 0)
        {
            WriteLine(std::format("std::array<BufferBindingInfoRG, {}> m_BufferBindings{{}};", Counts.Buffers));
            WriteLine("u32 m_BufferCount{};");
        }
        if (Counts.Images > 0)
        {
            WriteLine(std::format("std::array<ImageBindingInfoRG, {}> m_ImageBindings{{}};", Counts.Images));
            WriteLine("u32 m_ImageCount{};");
        }
        if (HasBindlessBuffers)
            WriteLine("std::vector<BufferBindingInfoRG> m_BufferBindlessBindings{{}};");
        if (HasBindlessImages)
            WriteLine("std::vector<ImageBindingInfoRG> m_ImageBindlessBindings{{}};");
    }
};
}

SlangGenerator::SlangGenerator(SlangUniformTypeGenerator& uniformTypeGenerator,
    const std::filesystem::path& shadersDirectory)
    : m_UniformTypeGenerator(&uniformTypeGenerator), m_ShadersDirectory(shadersDirectory)
{
}

std::string SlangGenerator::GenerateCommonFile() const
{
    std::string commonFile = std::string(utils::getPreamble()).append("\n");
    commonFile += R"(
#include "Rendering/Shader/ShaderCache.h"
#include "RenderGraph/RGGraph.h"
#include "Rendering/Commands/RenderCommandList.h"

struct BindGroupBaseRG
{
    struct SamplerBindingInfoRG
    {
        u32 Set{0};
        u32 Slot{0};
        Sampler Sampler{};
    };
    struct BindingInfoRG
    {
        u32 Set{0};
        u32 Slot{0};
        DescriptorType DescriptorType{};
        RG::Resource Resource{};
        u32 ArrayOffset{0};
    };
    using BufferBindingInfoRG = BindingInfoRG;
    using ImageBindingInfoRG = BindingInfoRG;

    BindGroupBaseRG() = default;
    BindGroupBaseRG(RG::Graph& graph, const Shader& shader)
        : Graph(&graph), Shader(&shader) {}

    RG::Graph* Graph{nullptr};
    const ::Shader* Shader{nullptr};
};
)";

    return commonFile;
}

namespace
{
constexpr std::string_view GENERATED_COMMON_FILE_NAME = "BindGroupBaseRG.generated.h";
}

std::filesystem::path SlangGenerator::GetCommonFilePath(const std::filesystem::path& generationPath) const
{
    return generationPath / GENERATED_COMMON_FILE_NAME;
}

assetlib::io::IoResult<SlangGeneratorResult> SlangGenerator::Generate(const std::filesystem::path& path) const
{
    auto shaderLoadInfo = assetlib::shader::readLoadInfo(path);
    if (!shaderLoadInfo.has_value())
        return std::unexpected(shaderLoadInfo.error());

    const std::filesystem::path bakedPath =
        bakers::Slang::GetBakedPath(path, {}, {.InitialDirectory = m_ShadersDirectory});
    auto assetFileResult = assetlib::io::loadAssetFileHeader(bakedPath);
    if (!assetFileResult.has_value())
        return std::unexpected(assetFileResult.error());

    auto shaderUnpack = assetlib::shader::unpackHeader(*assetFileResult);
    if (!shaderUnpack.has_value())
        return std::unexpected(shaderUnpack.error());

    const assetlib::ShaderHeader& shader = *shaderUnpack;

    std::unordered_set<std::filesystem::path> standaloneUniforms;

    std::unordered_map<assetlib::AssetId, std::string> embeddedStructs;
    std::vector<std::pair<std::string, std::string>> uniformsTypeAndParameterNamesForSet(shader.BindingSets.size());
    for (auto& set : shader.BindingSets)
    {
        if (set.UniformType.empty())
            continue;
        auto uniformResult = m_UniformTypeGenerator->Generate(set.UniformType);
        if (!uniformResult.has_value())
            return std::unexpected(uniformResult.error());

        auto& uniform = *uniformResult;
        for (auto&& [id, embedded] : uniform.EmbeddedStructs)
            embeddedStructs.emplace(id, embedded);

        if (uniform.IsStandalone)
            for (auto& include : uniform.Includes)
                standaloneUniforms.insert(std::move(include));
        uniformsTypeAndParameterNamesForSet[set.Set] = std::make_pair(uniform.TypeName, uniform.ParameterName);
    }

    std::string generatedStructName = std::format("{}BindGroupRG", utils::canonicalizeName(shader.Name));
    std::string generatedFileName = std::format("{}.generated.h", generatedStructName);

    Writer writer;

    writer.WriteLine(utils::getPreamble());
    writer.WriteLine(std::format("#include \"{}\"", GENERATED_COMMON_FILE_NAME));
    for (auto& standaloneUniform : standaloneUniforms)
        writer.WriteLine(std::format("#include \"{}\"", standaloneUniform.string()));
    writer.WriteLine("#include <array>");
    writer.WriteLine("#include <vector>");
    writer.WriteLine("");
    writer.WriteLine(std::format("struct {} : BindGroupBaseRG", generatedStructName));
    writer.WriteLine("{");
    writer.Push();
    writer.WriteLine("using BindGroupBaseRG::BindGroupBaseRG;");
    for (auto& embedded : embeddedStructs | std::views::values)
        writer.WriteEmbeddedUniformTypes(embedded);
    for (auto& set : shader.BindingSets)
    {
        /* auto-uniform binding is always the first one */
        if (!uniformsTypeAndParameterNamesForSet[set.Set].first.empty())
            writer.WriteUniformBinding(set, set.Bindings.front(),
                uniformsTypeAndParameterNamesForSet[set.Set].first,
                uniformsTypeAndParameterNamesForSet[set.Set].second);
        for (auto& binding : set.Bindings)
        {
            switch (binding.Type)
            {
            case assetlib::ShaderBindingType::Sampler:
                writer.WriteSampler(set, binding);
                break;
            case assetlib::ShaderBindingType::Image:
            case assetlib::ShaderBindingType::ImageStorage:
                writer.WriteImage(set, binding);
                break;
            case assetlib::ShaderBindingType::TexelUniform:
            case assetlib::ShaderBindingType::TexelStorage:
            case assetlib::ShaderBindingType::UniformBuffer:
            case assetlib::ShaderBindingType::UniformTexelBuffer:
            case assetlib::ShaderBindingType::StorageBuffer:
            case assetlib::ShaderBindingType::StorageTexelBuffer:
                writer.WriteBuffer(set, binding);
                break;
            default:
                break;
            }
        }
    }
    if (shaderLoadInfo->RasterizationInfo.has_value())
        writer.WriteRasterizationInfo(*shaderLoadInfo->RasterizationInfo);
    writer.WriteGroupSizeInfo(shader.EntryPoints);
    
    bool hasGraphics = false;
    bool hasCompute = false;
    for (auto& entry : shader.EntryPoints)
    {
        hasGraphics = hasGraphics || entry.ShaderStage != assetlib::ShaderStage::Compute;
        hasCompute = hasCompute || entry.ShaderStage == assetlib::ShaderStage::Compute;
    }

    if (hasGraphics)
        writer.WriteBindPipelineType("Graphics");
    if (hasCompute)
        writer.WriteBindPipelineType("Compute");
    
    writer.WriteBeginPrivate();
    if (hasGraphics)
        writer.WriteBindDescriptorsType(shader, "Graphics");
    if (hasCompute)
        writer.WriteBindDescriptorsType(shader, "Compute");
    writer.WriteResourceContainers();
    writer.Pop();
    writer.WriteLine("};");

    return SlangGeneratorResult{
        .Generated = writer.Stream.str(),
        .FileName = generatedFileName
    };
}
