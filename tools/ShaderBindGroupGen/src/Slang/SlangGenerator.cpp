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
struct BindingsInfo
{
public:
    struct Signature
    {
        std::string Name;
        u32 Count{1};

        bool operator==(const Signature& other) const
        {
            return Name == other.Name && Count == other.Count;
        }
    };
    struct SignatureHasher
    {
        u64 operator()(const Signature& signature) const
        {
            u64 hash = Hash::string(signature.Name);
            Hash::combine(hash, std::hash<u32>{}(signature.Count));
            
            return hash;
        }
    };
    struct AccessVariant
    {
        std::vector<u32> Variants;
        assetlib::ShaderStage ShaderStages{};
        u32 Binding{0};
        assetlib::ShaderBindingType Type{assetlib::ShaderBindingType::None};
        assetlib::ShaderBindingAccess Access{assetlib::ShaderBindingAccess::Read};
        assetlib::ShaderBindingAttributes Attributes{assetlib::ShaderBindingAttributes::None};
    };

    std::unordered_map<Signature, std::vector<AccessVariant>, SignatureHasher> Bindings;
public:
    void AddAccessVariant(const Signature& signature, const AccessVariant& accessVariant)
    {
        ASSERT(accessVariant.Variants.size() == 1)

        if (!Bindings.contains(signature))
        {
            Bindings[signature] = {accessVariant};
            return;
        }
        
        const auto it = std::ranges::find_if(Bindings[signature], [accessVariant](auto& access) {
            return
                access.ShaderStages == accessVariant.ShaderStages &&
                access.Binding == accessVariant.Binding &&
                access.Type == accessVariant.Type &&
                access.Access == accessVariant.Access &&
                access.Attributes == accessVariant.Attributes;
        });
        if (it == Bindings[signature].end())
            Bindings[signature].push_back(accessVariant);
        else
            it->Variants.push_back(accessVariant.Variants.front());
    }
    
    bool HasSamplers(const Signature& signature) const
    {
        return std::ranges::any_of(Bindings.at(signature), [](auto& access) {
            return access.Type == assetlib::ShaderBindingType::Sampler;
        });
    }

    bool OnlySamplers(const Signature& signature) const
    {
        return std::ranges::all_of(Bindings.at(signature), [](auto& access) {
            return access.Type == assetlib::ShaderBindingType::Sampler;
        });
    }
};

bool bindingAccessIsDivergent(const std::vector<BindingsInfo::AccessVariant>& accesses,
    const std::vector<std::string>& variants)
{
    // access is divergent if binding is used differently across variants
    // (e.g. read-only in one variant and read-write in other),
    // or if it is not present in some variants (e.g. binding declaration is hidden behind #ifdef)
    return accesses.size() != 1 || accesses.front().Variants.size() < variants.size();
}

struct UniformBindingsInfo
{
    struct Signature
    {
        std::string Type;
        std::string Parameter;
        BindingsInfo::Signature BindingSignature;

        bool operator==(const Signature& other) const
        {
            return Type == other.Type && Parameter == other.Parameter && BindingSignature == other.BindingSignature;
        }
    };
    struct SignatureHasher
    {
        u64 operator()(const Signature& signature) const
        {
            u64 hash = Hash::string(signature.Type);
            Hash::combine(hash, Hash::string(signature.Parameter));
            Hash::combine(hash, BindingsInfo::SignatureHasher{}(signature.BindingSignature));
            
            return hash;
        }
    };

    std::unordered_map<Signature, std::vector<u32>, SignatureHasher> Bindings;
public:
    void Add(const Signature& signature, u32 variant)
    {
        Bindings[signature].push_back(variant);
    }
};

struct BindingSetsInfo
{
    struct Signature
    {
        bool HasImmutableSamplers{false};
        u32 Set{};

        bool operator==(const Signature& other) const
        {
            return HasImmutableSamplers == other.HasImmutableSamplers && Set == other.Set;
        }
    };
    struct SignatureHasher
    {
        u64 operator()(const Signature& signature) const
        {
            u64 hash = std::hash<bool>{}(signature.HasImmutableSamplers);
            Hash::combine(hash, std::hash<u32>{}(signature.Set));
            
            return hash;
        }
    };

    std::unordered_map<Signature, std::vector<u32>, SignatureHasher> Sets;
public:
    void Add(const Signature& signature, u32 variant)
    {
        Sets[signature].push_back(variant);
    }
};

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
        return "Storage";
    default:
        ASSERT(false)
        return "";
    }
}

bool isBuffer(assetlib::ShaderBindingType bindingType)
{
    switch (bindingType)
    {
    case assetlib::ShaderBindingType::TexelUniform:
    case assetlib::ShaderBindingType::TexelStorage:
    case assetlib::ShaderBindingType::UniformBuffer:
    case assetlib::ShaderBindingType::UniformTexelBuffer:
    case assetlib::ShaderBindingType::StorageBuffer:
    case assetlib::ShaderBindingType::StorageTexelBuffer:
        return true;
    default:
        return false;
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

    void BeginSetResourceFunction(const BindingsInfo::Signature& signature)
    {
        std::string line = std::format("RG::Resource Set{}(RG::Resource resource",
            utils::canonicalizeName(signature.Name));
        if (signature.Count > 1)
            line.append(", u32 index");
        line.append(", RG::ResourceAccessFlags additionalAccess = RG::ResourceAccessFlags::None)");
        WriteLine(line);
        WriteLine("{");
        Push();
        WriteLine("using enum RG::ResourceAccessFlags;");
    }

    void BeginSetUniformFunction(const UniformBindingsInfo::Signature& signature)
    {
        std::string line = std::format("RG::Resource Set{}(const {}& {}",
            utils::canonicalizeName(signature.BindingSignature.Name), signature.Type, signature.Parameter);
        if (signature.BindingSignature.Count > 1)
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

    bool WriteResourceAccess(const BindingsInfo::Signature& signature, const BindingsInfo::AccessVariant& access)
    {
        const std::string shaderAccess = shaderStagesToGraphUsage(access.ShaderStages);
        const std::string graphUsage = bindingTypeToGraphUsage(access.Type);
        if (shaderAccess.empty() || graphUsage.empty())
        {
            AddError(std::format("Failed to infer usage for binding {}", signature.Name));
            return false;
        }
        WriteLine(std::format("resource = Graph->{}{}(resource, {} | {} | additionalAccess);",
            accessToString(access.Access), bindingTypeToString(access.Type), shaderAccess, graphUsage));

        return true;
    }

    static std::string GetBindingInfoContentString(u32 set,
        const BindingsInfo::Signature& signature, const BindingsInfo::AccessVariant& access)
    {
        return std::format(
            ".Set = {}, "
            ".Slot = {}, "
            ".DescriptorType = {}, "
            ".Resource = resource, "
            ".ArrayOffset = {}",
            set,
            access.Binding,
            bindingTypeToDescriptorTypeString(access.Type),
            signature.Count > 1 ? "index" : "0");
    }

    void WriteOrdinaryImage(u32 set,
        const BindingsInfo::Signature& signature, const BindingsInfo::AccessVariant& access)
    {
        const std::string line = std::format("m_ImageBindings[m_ImageCount] = ImageBindingInfoRG{{{}}};",
            GetBindingInfoContentString(set, signature, access));
        WriteLine(line);
        WriteLine("m_ImageCount += 1;");
        Counts.Images += signature.Count;
    }

    void WriteBindlessImage(u32 set,
        const BindingsInfo::Signature& signature, const BindingsInfo::AccessVariant& access)
    {
        HasBindlessImages = true;
        const std::string line = std::format("m_ImageBindlessBindings.push_back(ImageBindingInfoRG{{{}}});",
            GetBindingInfoContentString(set, signature, access));
        WriteLine(line);
    }

    void WriteOrdinaryBuffer(u32 set,
        const BindingsInfo::Signature& signature, const BindingsInfo::AccessVariant& access)
    {
        const std::string line = std::format("m_BufferBindings[m_BufferCount] = BufferBindingInfoRG{{{}}};",
            GetBindingInfoContentString(set, signature, access));
        WriteLine(line);
        WriteLine("m_BufferCount += 1;");
        Counts.Buffers += signature.Count;
    }

    void WriteBindlessBuffer(u32 set,
        const BindingsInfo::Signature& signature, const BindingsInfo::AccessVariant& access)
    {
        HasBindlessBuffers = true;
        const std::string line = std::format("m_BufferBindlessBindings.push_back(BufferBindingInfoRG{{{}}});",
            GetBindingInfoContentString(set, signature, access));
        WriteLine(line);
    }

    bool OnlyMainVariant(const std::vector<std::string>& variants)
    {
        return
            variants.empty() ||
            variants.size() == 1 && variants.front() == assetlib::ShaderLoadInfo::SHADER_VARIANT_MAIN_NAME;
    }

    void WriteVariantsEnum(const std::vector<std::string>& variants)
    {
        if (OnlyMainVariant(variants))
            return;
        WriteLine("enum class Variants : u8");
        WriteLine("{");
        Push();
        for (auto& variant : variants)
            WriteLine(std::format("{},", utils::canonicalizeName(variant)));
        Pop();
        WriteLine("};");
    }
    
    void WriteConstructor(std::string_view structName, std::string_view shaderName,
        const std::vector<std::string>& variants)
    {
        WriteLine(std::format("{}() = default;", structName));
        
        if (OnlyMainVariant(variants))
        {
            WriteLine(std::format("{}(RG::Graph& graph)", structName));
            WriteLine(std::format("\t: BindGroupBaseRG(graph, graph.SetShader(\"{}\"_hsv)) {{}}", shaderName));
            WriteLine(std::format("{}(RG::Graph& graph, ShaderOverridesView&& overrides)", structName));
            WriteLine(std::format("\t: BindGroupBaseRG(graph, graph.SetShader(\"{}\"_hsv, std::move(overrides))) {{}}",
                shaderName));
            return;
        }

        WriteLine(std::format("{}(RG::Graph& graph, Variants variant)", structName));
        WriteLine("\t: BindGroupBaseRG(graph), m_Variant(variant)");
        WriteLine("{");
        Push();
        WriteLine("switch (m_Variant)");
        WriteLine("{");
        for (auto& variant : variants)
        {
            WriteLine(std::format("case Variants::{}:", utils::canonicalizeName(variant)));
            Push();
            WriteLine(std::format(R"(Shader = &graph.SetShader("{}"_hsv, "{}"_hsv);)", shaderName, variant));
            WriteLine("break;");
            Pop();
        }
        WriteLine("}");
        Pop();
        WriteLine("}");

        WriteLine(std::format("{}(RG::Graph& graph, Variants variant, ShaderOverridesView&& overrides)", structName));
        WriteLine("\t: BindGroupBaseRG(graph), m_Variant(variant)");
        WriteLine("{");
        Push();
        WriteLine("switch (m_Variant)");
        WriteLine("{");
        for (auto& variant : variants)
        {
            WriteLine(std::format("case Variants::{}:", utils::canonicalizeName(variant)));
            Push();
            WriteLine(std::format(R"(Shader = &graph.SetShader("{}"_hsv, "{}"_hsv, std::move(overrides));)",
                shaderName, variant));
            WriteLine("break;");
            Pop();
        }
        WriteLine("}");
        Pop();
        WriteLine("}");
    }
    
    void WriteVariantEnumField(const std::vector<std::string>& variants)
    {
        if (OnlyMainVariant(variants))
            return;
        WriteLine(std::format("Variants m_Variant{{Variants::{}}};", utils::canonicalizeName(variants.front())));
    }

    void WriteEmbeddedUniformTypes(const std::string& uniform)
    {
        std::stringstream ss(uniform);
        std::string line;
        while (std::getline(ss, line))
            WriteLine(line);
    }

    bool ValidateAccessVariants(const BindingsInfo& bindings, const BindingsInfo::Signature& signature)
    {
        const bool hasSamplers = bindings.HasSamplers(signature);
        const bool onlySamplers = bindings.OnlySamplers(signature);

        if (hasSamplers && !onlySamplers)
        {
            AddError("Sampler-Resource divergence is not supported");
            return false;
        }

        return true;
    }

    void BeginDivergenceSwitch(bool isDivergent)
    {
        if (isDivergent)
        {
            WriteLine("switch (m_Variant)");
            WriteLine("{");
            Push();
        }
    }
    
    void EndDivergenceSwitch(bool isDivergent)
    {
        if (isDivergent)
        {
            WriteLine("default:");
            Push();
            WriteLine("break;");
            Pop();
            Pop();
            WriteLine("}");
        }
    }

    void BeginDivergenceCase(bool isDivergent, const std::vector<u32> accessVariants,
        const std::vector<std::string>& variants)
    {
        if (isDivergent)
        {
            for (auto& variant : accessVariants)
                WriteLine(std::format("case Variants::{}:", utils::canonicalizeName(variants[variant])));
            Push();
        }
    }
    void EndDivergenceCase(bool isDivergent)
    {
        if (isDivergent)
        {
            WriteLine("break;");
            Pop();
        }
    }

    void WriteSamplerBindings(u32 set, const BindingsInfo::Signature& signature,
        const std::vector<BindingsInfo::AccessVariant>& accesses, const std::vector<std::string>& variants,
        bool isDivergent)
    {
        if (signature.Count > 1)
        {
            AddError(std::format("Array of samplers are not supported: {}", signature.Name));
            return;
        }

        const bool isImmutableAcrossDivergence =
            !isDivergent && enumHasAny(accesses.front().Attributes,
                assetlib::ShaderBindingAttributes::ImmutableSampler) ||
            std::ranges::all_of(accesses, [](auto& access) {
                return enumHasAny(access.Attributes,assetlib::ShaderBindingAttributes::ImmutableSampler);
        });

        if (isImmutableAcrossDivergence)
            return;

        WriteLine(std::format("void Set{}(Sampler sampler)", utils::canonicalizeName(signature.Name)));
        WriteLine("{");
        Push();

        BeginDivergenceSwitch(isDivergent);
        for (auto& access : accesses)
        {
            BeginDivergenceCase(isDivergent, access.Variants, variants);

            WriteLine(std::format("m_SamplerBindings[m_SamplerCount] = SamplerBindingInfoRG{{"
                ".Set = {}, "
                ".Slot = {}, "
                ".Sampler = sampler}};",
                set,
                access.Binding
            ));
            WriteLine("m_SamplerCount += 1;");

            EndDivergenceCase(isDivergent);
        }
        EndDivergenceSwitch(isDivergent);
        
        Pop();
        WriteLine("}");
        Counts.Samplers += signature.Count;
    }

    void WriteResourceBindingsFunctionBody(u32 set,
        const BindingsInfo::Signature& signature, const std::vector<BindingsInfo::AccessVariant>& accesses,
        const std::vector<std::string>& variants, bool isDivergent)
    {
        BeginDivergenceSwitch(isDivergent);
        for (auto& access : accesses)
        {
            BeginDivergenceCase(isDivergent, access.Variants, variants);

            bool success = WriteResourceAccess(signature, access);
            if (success)
            {
                if (enumHasAny(access.Attributes, assetlib::ShaderBindingAttributes::Bindless))
                    isBuffer(access.Type) ?
                        WriteBindlessBuffer(set, signature, access) : WriteBindlessImage(set, signature, access);
                else
                    isBuffer(access.Type) ?
                        WriteOrdinaryBuffer(set, signature, access) : WriteOrdinaryImage(set, signature, access);
            }

            EndDivergenceCase(isDivergent);
        }
        EndDivergenceSwitch(isDivergent);
    }
        
    void WriteResourceBindings(u32 set, const BindingsInfo::Signature& signature,
        const std::vector<BindingsInfo::AccessVariant>& accesses, const std::vector<std::string>& variants,
        bool isDivergent)
    {
        BeginSetResourceFunction(signature);
        WriteResourceBindingsFunctionBody(set, signature, accesses, variants, isDivergent);
        EndSetResourceFunction();
    }

    void WriteBindings(u32 set, const BindingsInfo& bindings,
        const std::vector<std::string>& variants)
    {
        for (auto&& [signature, accessVariants] : bindings.Bindings)
        {
            const bool isValid = ValidateAccessVariants(bindings, signature);
            if (!isValid)
            {
                AddError(std::format("Validate binding failed for set {}", set));
                continue;
            }

            const bool isDivergent = bindingAccessIsDivergent(accessVariants, variants);

            if (bindings.OnlySamplers(signature))
            {
                WriteSamplerBindings(set, signature, accessVariants, variants, isDivergent);
                continue;
            }

            WriteResourceBindings(set, signature, accessVariants, variants, isDivergent);
        }
    }

    void WriteUniformBindings(u32 set, const UniformBindingsInfo& uniforms,
        const BindingsInfo& bindings, const std::vector<std::string>& variants)
    {
        if (uniforms.Bindings.empty())
            return;
        
        if (uniforms.Bindings.size() > 1)
        {
            AddError("Divergent uniforms are not supported");
            return;
        }

        for (auto&& [uniformSignature, _] : uniforms.Bindings)
        {
            BeginSetUniformFunction(uniformSignature);

            WriteLine(std::format("RG::Resource resource = Graph->Create(\"{}\"_hsv, RG::RGBufferDescription{{"
                ".SizeBytes = sizeof({})}});", uniformSignature.Type, uniformSignature.Type));
            WriteLine(std::format("resource = Graph->Upload(resource, {});", uniformSignature.Parameter));

            auto& bindingSignature = uniformSignature.BindingSignature;
            auto& bindingAccesses = bindings.Bindings.at(bindingSignature);
            WriteResourceBindingsFunctionBody(set, bindingSignature, bindingAccesses, variants,
                bindingAccessIsDivergent(bindingAccesses, variants));

            EndSetResourceFunction();
        }
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

    void WriteBindDescriptorsType(const std::vector<BindingSetsInfo>& bindingSets,
        const std::vector<std::string>& variants, const std::string& type)
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
        for (auto& set : bindingSets)
        {
            const bool isDivergent = set.Sets.size() > 1 || set.Sets.begin()->second.size() != variants.size();
            BeginDivergenceSwitch(isDivergent);
            for (auto&& [signature, accessVariants] : set.Sets)
            {
                BeginDivergenceCase(isDivergent, accessVariants, variants);
                if (signature.HasImmutableSamplers)
                    WriteLine(std::format(
                        "cmdList.BindImmutableSamplers{}({{"
                        ".Descriptors = Shader->Descriptors({}), .PipelineLayout = Shader->GetLayout(), .Set = {}}});",
                        type, signature.Set, signature.Set));
                else
                    WriteLine(std::format(
                        "cmdList.BindDescriptors{}({{"
                        ".Descriptors = Shader->Descriptors({}), .Allocators = &allocators, "
                        ".PipelineLayout = Shader->GetLayout(), .Set = {}}});",
                        type, signature.Set, signature.Set));
                EndDivergenceCase(isDivergent);
            }
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
            WriteLine("std::vector<BufferBindingInfoRG> m_BufferBindlessBindings{};");
        if (HasBindlessImages)
            WriteLine("std::vector<ImageBindingInfoRG> m_ImageBindlessBindings{};");
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
    BindGroupBaseRG(RG::Graph& graph)
        : Graph(&graph) {}

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

    std::vector<std::string> variants;
    std::unordered_set<std::filesystem::path> standaloneUniforms;
    std::unordered_map<assetlib::AssetId, std::string> embeddedStructs;

    std::vector<BindingSetsInfo> bindingsSetsInfoPerSet;
    std::vector<BindingsInfo> bindingsInfoPerSet;
    std::vector<UniformBindingsInfo> uniformBindingsInfoPerSet;
    std::vector<assetlib::ShaderEntryPoint> entryPoints;
    
    for (auto& variant : shaderLoadInfo->Variants)
    {
        const u32 variantIndex = (u32)variants.size();
        variants.push_back(variant.Name);
        
        const std::filesystem::path bakedPath =
        bakers::Slang::GetBakedPath(path, StringId::FromString(variant.Name), {},
            {.InitialDirectory = m_ShadersDirectory});
        auto assetFileResult = assetlib::io::loadAssetFileHeader(bakedPath);
        if (!assetFileResult.has_value())
            return std::unexpected(assetFileResult.error());

        auto shaderUnpack = assetlib::shader::unpackHeader(*assetFileResult);
        if (!shaderUnpack.has_value())
            return std::unexpected(shaderUnpack.error());

        const assetlib::ShaderHeader& shader = *shaderUnpack;

        if (entryPoints.empty())
            entryPoints = shader.EntryPoints;
        
        for (auto& set : shader.BindingSets)
        {
            if (set.Set >= bindingsSetsInfoPerSet.size())
                bindingsSetsInfoPerSet.resize(set.Set + 1);
            bindingsSetsInfoPerSet[set.Set].Add(
                {
                    .HasImmutableSamplers = std::ranges::any_of(set.Bindings, [](const assetlib::ShaderBinding& binding)
                        {
                            return enumHasAny(binding.Attributes, assetlib::ShaderBindingAttributes::ImmutableSampler);
                        }),
                    .Set = set.Set
                }, variantIndex);
            
            if (set.Set >= bindingsInfoPerSet.size())
                bindingsInfoPerSet.resize(set.Set + 1);
            for (auto& binding : set.Bindings)
                bindingsInfoPerSet[set.Set].AddAccessVariant(
                    {
                        .Name = binding.Name,
                        .Count = binding.Count
                    },
                    {
                        .Variants = {variantIndex},
                        .ShaderStages = binding.ShaderStages,
                        .Binding = binding.Binding,
                        .Type = binding.Type,
                        .Access = binding.Access,
                        .Attributes = binding.Attributes 
                    });
            
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

            if (set.Set >= uniformBindingsInfoPerSet.size())
                uniformBindingsInfoPerSet.resize(set.Set + 1);

            uniformBindingsInfoPerSet[set.Set].Add(
                {
                    .Type = uniform.TypeName,
                    .Parameter = uniform.ParameterName,
                    .BindingSignature =
                        {
                            .Name = set.Bindings.front().Name,
                            .Count = set.Bindings.front().Count
                        }
                }, variantIndex);
        }
    }
    
    std::string generatedStructName = std::format("{}BindGroupRG", utils::canonicalizeName(shaderLoadInfo->Name));
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
    writer.WriteVariantsEnum(variants);
    writer.WriteConstructor(generatedStructName, shaderLoadInfo->Name, variants);
    for (auto& embedded : embeddedStructs | std::views::values)
        writer.WriteEmbeddedUniformTypes(embedded);
    for (u32 i = 0; i < uniformBindingsInfoPerSet.size(); i++)
        writer.WriteUniformBindings(i, uniformBindingsInfoPerSet[i], bindingsInfoPerSet[i], variants);
    for (u32 i = 0; i < bindingsInfoPerSet.size(); i++)
        writer.WriteBindings(i, bindingsInfoPerSet[i], variants);
    if (shaderLoadInfo->RasterizationInfo.has_value())
        writer.WriteRasterizationInfo(*shaderLoadInfo->RasterizationInfo);
    writer.WriteGroupSizeInfo(entryPoints);
    
    bool hasGraphics = false;
    bool hasCompute = false;
    for (auto& entry : entryPoints)
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
        writer.WriteBindDescriptorsType(bindingsSetsInfoPerSet, variants, "Graphics");
    if (hasCompute)
        writer.WriteBindDescriptorsType(bindingsSetsInfoPerSet, variants, "Compute");
    writer.WriteVariantEnumField(variants);
    writer.WriteResourceContainers();
    writer.Pop();
    writer.WriteLine("};");

    return SlangGeneratorResult{
        .Generated = writer.Stream.str(),
        .FileName = generatedFileName
    };
}
