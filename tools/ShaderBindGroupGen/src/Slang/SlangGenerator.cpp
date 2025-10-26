#include "SlangGenerator.h"

#include "GeneratorUtils.h"
#include "SlangUniformTypeGenerator.h"

#include <ranges>
#include <unordered_set>

namespace 
{
std::string accessToString(assetlib::ShaderBindingAccess access)
{
    switch (access) {
    case assetlib::ShaderBindingAccess::Read: return "Read";
    case assetlib::ShaderBindingAccess::Write: return "Write";
    case assetlib::ShaderBindingAccess::ReadWrite: return "ReadWrite";
    default:
        ASSERT(false)
        return "Read";
    }
}

std::string bindingTypeToString(assetlib::ShaderBindingType bindingType)
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

std::string bindingTypeToGraphUsage(assetlib::ShaderBindingType bindingType)
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
        return "Buffer";
    default:
        ASSERT(false)
        return "";
    }
}

std::string bindingTypeToDescriptorTypeString(assetlib::ShaderBindingType bindingType)
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
        std::string line = std::format("void Set{}(RG::Resource resource", utils::canonicalizeName(binding.Name));
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
        std::string line = std::format("void Set{}(const {}& {}",
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
        Pop();
        WriteLine("}");
    }
    void WriteResourceAccess(const assetlib::ShaderBinding& binding)
    {
        const std::string shaderAccess = shaderStagesToGraphUsage(binding.ShaderStages);
        const std::string graphUsage = bindingTypeToGraphUsage(binding.Type);
        if (shaderAccess.empty() || graphUsage.empty())
        {
            AddError(std::format("Failed to infer usage for binding {}", binding.Name));
            return;
        }
        WriteLine(std::format("Graph->{}{}(resource, {} | {} | additionalAccess);",
            accessToString(binding.Access), bindingTypeToString(binding.Type),
            shaderStagesToGraphUsage(binding.ShaderStages), bindingTypeToGraphUsage(binding.Type)));
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
                  ".SizeBytes = sizeof({})}};", uniformType, uniformType));
        WriteResourceAccess(uniformBinding);
        if (enumHasAny(uniformBinding.Attributes, assetlib::ShaderBindingAttributes::Bindless))
            WriteBindlessBuffer(set, uniformBinding);
        else
            WriteOrdinaryBuffer(set, uniformBinding);
        EndSetResourceFunction();
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
    void WriteBindDescriptors()
    {
        WriteLine("void BindDescriptors(RenderCommandList& cmdList, const DescriptorArenaAllocators& allocators)");
        WriteLine("{");
        Push();
        if (Counts.Samplers > 0)
        {
            BeginForLoop("m_SamplerCount");
            WriteLine("auto& binding = m_SamplerBindings[i];");
            WriteLine("Device::UpdateDescriptors(Shader->Descriptors(binding.Set), "
                      "DescriptorSlotInfo{.Slot = binding.Slot, .Type = DescriptorType::Sampler}, sampler);");
            EndForLoop();
        }
        if (Counts.Buffers > 0)
        {
            BeginForLoop("m_BufferCount");
            WriteLine("auto& binding = m_BufferBindings[i];");
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
            WriteLine("auto& binding = m_ImageBindings[i];");
            WriteLine("auto& image = Graph->GetImageBinding(binding.Resource);");
            WriteLine("Device::UpdateDescriptors(Shader->Descriptors(binding.Set), "
                      "DescriptorSlotInfo{.Slot = binding.Slot, .Type = binding.DescriptorType}, "
                      "image.Subresource, image.Layout, binding.ArrayOffset);");
            EndForLoop();
        }
        if (HasBindlessImages)
        {
            BeginForEachLoop("m_ImageBindlessBindings");
            WriteLine("auto& image = Graph->GetImageBinding(binding.Resource);");
            WriteLine("Device::UpdateDescriptors(Shader->Descriptors(binding.Set), "
                      "DescriptorSlotInfo{.Slot = binding.Slot, .Type = binding.DescriptorType}, "
                      "image.Subresource, image.Layout, binding.ArrayOffset);");
            EndForLoop();
        }
        Pop();
        WriteLine("}");
    }
    void WriteBindEntryPoint(const assetlib::ShaderEntryPoint& entryPoint)
    {
        WriteLine(std::format("void Bind{}(RenderCommandList& cmdList, const DescriptorArenaAllocators& allocators)",
            utils::canonicalizeName(entryPoint.Name)));
        WriteLine("{");
        Push();
        if (entryPoint.ShaderStage == assetlib::ShaderStage::Compute)
            WriteLine("cmdList.BindPipelineCompute({.Pipeline = Shader->Pipeline()});");
        else
            WriteLine("cmdList.BindPipelineGraphics({.Pipeline = Shader->Pipeline()});");
        WriteLine("BindDescriptors(cmdList, allocators);");
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

SlangGenerator::SlangGenerator(SlangUniformTypeGenerator& uniformTypeGenerator)
    : m_UniformTypeGenerator(&uniformTypeGenerator)
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

assetlib::io::IoResult<SlangGeneratorResult> SlangGenerator::Generate(const assetlib::ShaderHeader& shader) const
{
    std::unordered_set<std::filesystem::path> standaloneUniforms;

    std::unordered_map<assetlib::AssetId, std::string> embeddedStructs;
    std::vector<std::pair<std::string, std::string>> uniformsTypeAndParameterNamesForSet(shader.BindingSets.size());
    for (auto& set : shader.BindingSets)
    {
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
    for (auto& entryPoint : shader.EntryPoints)
        writer.WriteBindEntryPoint(entryPoint);
    writer.WriteBeginPrivate();
    writer.WriteBindDescriptors();
    writer.WriteResourceContainers();
    writer.Pop();
    writer.WriteLine("};");
    
    return SlangGeneratorResult{
        .Generated = writer.Stream.str(),
        .FileName = generatedFileName
    };
}
