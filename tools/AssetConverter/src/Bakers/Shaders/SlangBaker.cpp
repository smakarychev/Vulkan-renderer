#include "SlangBaker.h"

#include "core.h"
#include "Bakers/BakersUtils.h"
#include "Utils/HashFileUtils.h"
#include "Utils/HashUtils.h"
#include "v2/Shaders/ShaderLoadInfo.h"
#include "v2/Shaders/ShaderUniform.h"
#include "v2/Shaders/SlangShaderAsset.h"

#include <fstream>
#include <queue>
#include <stack>
#include <unordered_map>
#include <ranges>
#include <unordered_set>
#include <glaze/json/generic.hpp>
#include <glaze/json/prettify.hpp>
#include <glm/vec3.hpp>
#include <slang/slang.h>
#include <slang/slang-com-ptr.h>

#define CHECK_RETURN_IO_ERROR(x, error, ...) \
    if (!(x)) { return std::unexpected(IoError{.Code = error, .Message = std::format(__VA_ARGS__)}); }

namespace bakers
{
namespace
{
slang::IGlobalSession& getGlobalSession()
{
    static std::once_flag once = {};
    static ::Slang::ComPtr<slang::IGlobalSession> globalSession;

    std::call_once(once, []()
    {
        const auto res = slang::createGlobalSession(globalSession.writeRef());
        ASSERT(!SLANG_FAILED(res), "Failed to create slang-api global session")
    });

    return *globalSession;
}

slang::ISession& getSession(const SlangBakeSettings& settings)
{
    /* slang sessions must be unique for each #define list */
    static std::unordered_map<u64, ::Slang::ComPtr<slang::ISession>> sessions;
    static const SlangProfileID PROFILE_ID = getGlobalSession().findProfile("spirv_1_5");

    if (sessions.contains(settings.DefinesHash))
        return *sessions.at(settings.DefinesHash);

    slang::TargetDesc targetDesc = {
        .format = SLANG_SPIRV,
        .profile = PROFILE_ID,
    };

    std::array options =
    {
        slang::CompilerOptionEntry{
            .name = slang::CompilerOptionName::Optimization,
            .value = {.kind = slang::CompilerOptionValueKind::Int, .intValue0 = SLANG_OPTIMIZATION_LEVEL_HIGH}
        },
        slang::CompilerOptionEntry{
            .name = slang::CompilerOptionName::VulkanUseEntryPointName,
            .value = {.kind = slang::CompilerOptionValueKind::Int, .intValue0 = true}
        },
    };

    std::vector<slang::PreprocessorMacroDesc> macros(settings.Defines.size());
    for (auto&& [i, define] : std::views::enumerate(settings.Defines))
        macros[i] = {
            .name = define.first.c_str(),
            .value = define.second.c_str()
        };

    std::vector<const char*> includePaths(settings.IncludePaths.size());
    for (auto&& [i, path] : std::views::enumerate(settings.IncludePaths))
        includePaths[i] = path.c_str();

    const slang::SessionDesc sessionDesc = {
        .targets = &targetDesc,
        .targetCount = 1,
        .searchPaths = includePaths.data(),
        .searchPathCount = (SlangInt)includePaths.size(),
        .preprocessorMacros = macros.data(),
        .preprocessorMacroCount = (SlangInt)macros.size(),
        .compilerOptionEntries = options.data(),
        .compilerOptionEntryCount = (u32)options.size(),
    };

    ::Slang::ComPtr<slang::ISession> session;
    const auto res = getGlobalSession().createSession(sessionDesc, session.writeRef());
    ASSERT(!SLANG_FAILED(res), "Failed to create slang-api session")
    const auto [newSession, _] = sessions.emplace(settings.DefinesHash, std::move(session));

    return *newSession->second;
}

assetlib::ShaderBindingType slangBindingTypeToBindingType(slang::BindingType bindingType)
{
    using enum assetlib::ShaderBindingType;
    switch (bindingType)
    {
    case slang::BindingType::Sampler:
        return Sampler;
    case slang::BindingType::Texture:
    case slang::BindingType::CombinedTextureSampler:
        return Image;
    case slang::BindingType::MutableTexture:
        return ImageStorage;
    case slang::BindingType::ConstantBuffer:
        return UniformBuffer;
    case slang::BindingType::TypedBuffer:
        return UniformTexelBuffer;
    case slang::BindingType::MutableTypedBuffer:
        return StorageTexelBuffer;
    case slang::BindingType::RawBuffer:
    case slang::BindingType::MutableRawBuffer:
        return StorageBuffer;
    case slang::BindingType::InputRenderTarget:
        return Input;
    default:
        ASSERT(false, "Unsupported bindingType {}", (u32)bindingType)
        return None;
    }
}

assetlib::ShaderUniformTypeKind slangTypeKindToTypeKind(slang::TypeReflection::Kind kind)
{
    using enum slang::TypeReflection::Kind;
    switch (kind)
    {
    case None: return assetlib::ShaderUniformTypeKind::None;
    case Struct: return assetlib::ShaderUniformTypeKind::Struct;
    case Array: return assetlib::ShaderUniformTypeKind::Array;
    case Matrix: return assetlib::ShaderUniformTypeKind::Matrix;
    case Vector: return assetlib::ShaderUniformTypeKind::Vector;
    case Scalar: return assetlib::ShaderUniformTypeKind::Scalar;
    case ConstantBuffer: return assetlib::ShaderUniformTypeKind::ConstantBuffer;
    case Resource: return assetlib::ShaderUniformTypeKind::Resource;
    case SamplerState: return assetlib::ShaderUniformTypeKind::SamplerState;
    case TextureBuffer: return assetlib::ShaderUniformTypeKind::TextureBuffer;
    case ShaderStorageBuffer: return assetlib::ShaderUniformTypeKind::ShaderStorageBuffer;
    case ParameterBlock: return assetlib::ShaderUniformTypeKind::ParameterBlock;
    case GenericTypeParameter: return assetlib::ShaderUniformTypeKind::GenericTypeParameter;
    case Interface: return assetlib::ShaderUniformTypeKind::Interface;
    case OutputStream: return assetlib::ShaderUniformTypeKind::OutputStream;
    case Specialized: return assetlib::ShaderUniformTypeKind::Specialized;
    case Feedback: return assetlib::ShaderUniformTypeKind::Feedback;
    case Pointer: return assetlib::ShaderUniformTypeKind::Pointer;
    case DynamicResource: return assetlib::ShaderUniformTypeKind::DynamicResource;
    default:
        ASSERT(false)
        return assetlib::ShaderUniformTypeKind::None;;
    }
}

assetlib::ShaderScalarType slangScalarTypeToScalarType(slang::TypeReflection::ScalarType type)
{
    switch (type)
    {
    case slang::TypeReflection::None: return assetlib::ShaderScalarType::None;
    case slang::TypeReflection::Void: return assetlib::ShaderScalarType::Void;
    case slang::TypeReflection::Bool: return assetlib::ShaderScalarType::Bool;
    case slang::TypeReflection::Int32: return assetlib::ShaderScalarType::I32;
    case slang::TypeReflection::UInt32: return assetlib::ShaderScalarType::U32;
    case slang::TypeReflection::Int64: return assetlib::ShaderScalarType::I64;
    case slang::TypeReflection::UInt64: return assetlib::ShaderScalarType::U64;
    case slang::TypeReflection::Float16: return assetlib::ShaderScalarType::F16;
    case slang::TypeReflection::Float32: return assetlib::ShaderScalarType::F32;
    case slang::TypeReflection::Float64: return assetlib::ShaderScalarType::F64;
    case slang::TypeReflection::Int8: return assetlib::ShaderScalarType::I8;
    case slang::TypeReflection::UInt8: return assetlib::ShaderScalarType::U8;
    case slang::TypeReflection::Int16: return assetlib::ShaderScalarType::I16;
    case slang::TypeReflection::UInt16: return assetlib::ShaderScalarType::U16;
    default:
        ASSERT(false, "Unsupported scalar type {}", (u32)type)
        return assetlib::ShaderScalarType::None;
    }
}

std::string createVariableName(const std::string& currentName, slang::VariableLayoutReflection* variableLayout)
{
    return variableLayout->getName() ? std::format("{}_{}", currentName, variableLayout->getName()) : currentName;
}

constexpr std::string_view SHADER_ATTRIBUTE_BINDLESS = "Bindless";
constexpr std::string_view SHADER_ATTRIBUTE_IMMUTABLE_SAMPLER = "ImmutableSampler";
constexpr std::string_view SHADER_ATTRIBUTE_STANDALONE_TYPE = "StandaloneType";

class UniformTypeReflector
{
    using Uniform = assetlib::ShaderUniform;
    using UniformVariable = assetlib::ShaderUniformVariable;
    using UniformType = assetlib::ShaderUniformType;
public:
    UniformTypeReflector(const Context& ctx, const SlangBakeSettings& settings) : m_Ctx(&ctx), m_Settings(&settings) {}
    std::optional<Uniform> Reflect(const std::string& baseName, slang::VariableLayoutReflection* variableLayout)
    {
        m_CurrentName = baseName;
        std::optional<UniformVariable> rootResult = ReflectVariableLayout(variableLayout);
        if (!rootResult.has_value())
            return std::nullopt;

        UniformVariable& root = *rootResult;
        PromoteRootToEmbeddedStructIfNecessary(root);
        
        Uniform uniform = {
            .Root = root,
            .EmbeddedStructs = std::move(m_EmbeddedStructs)
        };
        m_EmbeddedStructs.clear();
        
        return uniform;
    }
private:
    std::optional<UniformVariable> ReflectVariableLayout(slang::VariableLayoutReflection* variableLayout)
    {
        slang::TypeLayoutReflection* typeLayout = variableLayout->getTypeLayout();
        if (!typeLayout->getSize())
            return std::nullopt;

        UniformVariable reflection = {};
        ReflectCommonVariableInfo(variableLayout, reflection);
        const std::string oldName = m_CurrentName;
        m_CurrentName = reflection.Name;
        reflection.Type = ReflectTypeLayout(typeLayout);
        m_CurrentName = oldName;

        return reflection;
    }
    void PromoteRootToEmbeddedStructIfNecessary(UniformVariable& root)
    {
        if (std::holds_alternative<assetlib::ShaderUniformTypeStructReference>(root.Type.Type))
            return;
        
        ASSERT(!std::holds_alternative<std::shared_ptr<assetlib::ShaderUniformTypeStruct>>(root.Type.Type),
            "Struct type is unexpected at this stage: {}", root.Name)
        assetlib::ShaderUniformTypeStruct promoted = {
            .TypeName = root.Name,
            .Fields = {{
                .Name = "_value",
                .OffsetBytes = 0,
                .Type = root.Type,
            }}
        };
        const assetlib::AssetId assetId = {};
        m_EmbeddedStructs.push_back({.Struct = std::move(promoted), .Id = assetId});
        root.Type.TypeKind = assetlib::ShaderUniformTypeKind::Struct;
        root.Type.Type = assetlib::ShaderUniformTypeStructReference{
            .Target = std::to_string(assetId.AsU64()),
            .TypeName = root.Name,
            .IsEmbedded = true
        };
    }

    void ReflectCommonVariableInfo(slang::VariableLayoutReflection* variableLayout, UniformVariable& variable) const
    {
        variable.Name = createVariableName(m_CurrentName, variableLayout);
        variable.OffsetBytes = (u32)variableLayout->getOffset();
    }
    
    UniformType ReflectTypeLayout(slang::TypeLayoutReflection* typeLayout)
    {
        UniformType reflection = {};

        ReflectCommonTypeInfo(typeLayout, reflection);
        
        switch (typeLayout->getKind())
        {
        case slang::TypeReflection::Kind::Struct:
            ReflectStruct(typeLayout, reflection);
            break;
        case slang::TypeReflection::Kind::Array:
            ReflectArray(typeLayout, reflection);
            break;
        case slang::TypeReflection::Kind::Matrix:
            ReflectMatrix(typeLayout, reflection);
            break;
        case slang::TypeReflection::Kind::Vector:
            ReflectVector(typeLayout, reflection);
            break;
        case slang::TypeReflection::Kind::Scalar:
            ReflectScalar(typeLayout, reflection);
            break;
        default:
            ASSERT(false)
            break;
        }

        return reflection;
    }

    static void ReflectCommonTypeInfo(slang::TypeLayoutReflection* typeLayout, UniformType& reflection)
    {
        reflection.SizeBytes = (u32)typeLayout->getSize();
        reflection.TypeKind = slangTypeKindToTypeKind(typeLayout->getKind());
    }

    void ReflectStruct(slang::TypeLayoutReflection* typeLayout, UniformType& reflection)
    {
        auto reflectionTarget = FindStructTypeStandaloneAttribute(typeLayout);
        const bool isEmbedded = !reflectionTarget.has_value();
        
        if (!m_ProcessedEmbeddedStructTypes.contains(typeLayout->getType()))
        {
            const assetlib::AssetId assetId = {};
            assetlib::ShaderUniformTypeStruct structType = GetStructTypeReflection(typeLayout);
            if (isEmbedded)
                m_EmbeddedStructs.push_back({.Struct = std::move(structType), .Id = assetId});
            else
                WriteStructReflection(structType, *reflectionTarget);
            m_ProcessedEmbeddedStructTypes.emplace(typeLayout->getType(), assetId);
        }
        
        if (isEmbedded)
            reflectionTarget = std::to_string(m_ProcessedEmbeddedStructTypes.at(typeLayout->getType()).AsU64());

        reflection.Type = assetlib::ShaderUniformTypeStructReference{
            .Target = *reflectionTarget,
            .TypeName = typeLayout->getName(),
            .IsEmbedded = isEmbedded
        };
    }

    assetlib::ShaderUniformTypeStruct GetStructTypeReflection(slang::TypeLayoutReflection* typeLayout)
    {
        std::string oldName = std::exchange(m_CurrentName, "");
        assetlib::ShaderUniformTypeStruct structReflection = {
            .TypeName = typeLayout->getName()
        };
        structReflection.Fields.reserve(typeLayout->getFieldCount());
        for (u32 i = 0; i < typeLayout->getFieldCount(); i++)
        {
            const std::optional<UniformVariable> reflectionField =
                ReflectVariableLayout(typeLayout->getFieldByIndex(i));
            if (reflectionField.has_value())
                structReflection.Fields.push_back(*reflectionField);
        }
        m_CurrentName = std::move(oldName);

        return structReflection;
    }

    void WriteStructReflection(const assetlib::ShaderUniformTypeStruct& reflection, const std::string& target)
    {
        namespace fs = std::filesystem;
        const fs::path targetPath = 
            fs::weakly_canonical(m_Ctx->InitialDirectory / m_Settings->UniformReflectionDirectoryName / target);

        std::string reflectionString = assetlib::shader::packUniformStruct(reflection).value_or("{}");
        reflectionString = glz::prettify_json(reflectionString);
        
        if (fs::exists(targetPath))
        {
            const u32 reflectionHash = Hash::murmur3b32((const u8*)reflectionString.data(), reflectionString.length());        
            const u32 targetHash = Hash::murmur3b32File(targetPath).value_or(0);

            if (targetHash == reflectionHash)
                return;
        }
        
        if (!fs::exists(targetPath))
            fs::create_directories(targetPath.parent_path());

        std::ofstream out(targetPath.string(), std::ios::binary);
        out.write(reflectionString.data(), (isize)reflectionString.length());
    }

    void ReflectArray(slang::TypeLayoutReflection* typeLayout, UniformType& reflection)
    {
        assetlib::ShaderUniformTypeArray arrayReflection = {
            .Size = (u32)typeLayout->getElementCount(),
            .Element = ReflectTypeLayout(typeLayout->getElementTypeLayout())
        };
        reflection.Type = std::make_shared<assetlib::ShaderUniformTypeArray>(std::move(arrayReflection));
    }
    
    void ReflectMatrix(slang::TypeLayoutReflection* typeLayout, UniformType& reflection)
    {
        reflection.Type = assetlib::ShaderUniformTypeMatrix{
            .Rows = typeLayout->getRowCount(),
            .Columns = typeLayout->getColumnCount(),
            /* the element type of matrix is vector, which contradicts math definition and logic >:( */
            .Scalar = slangScalarTypeToScalarType(typeLayout->
                getElementTypeLayout()->getElementTypeLayout()->getScalarType())
        };
    }
    
    void ReflectVector(slang::TypeLayoutReflection* typeLayout, UniformType& reflection)
    {
        reflection.Type = assetlib::ShaderUniformTypeVector{
            .Elements = (u32)typeLayout->getElementCount(),
            .Scalar = slangScalarTypeToScalarType(typeLayout->getElementTypeLayout()->getScalarType())
        };
    }
    
    void ReflectScalar(slang::TypeLayoutReflection* typeLayout, UniformType& reflection)
    {
        reflection.Type = assetlib::ShaderUniformTypeScalar{
            .Scalar = slangScalarTypeToScalarType(typeLayout->getScalarType())
        };
    }

    static std::optional<std::string> FindStructTypeStandaloneAttribute(slang::TypeLayoutReflection* typeLayout)
    {
        const u32 attributeCount = typeLayout->getType()->getUserAttributeCount();
        for (u32 i = 0; i < attributeCount; i++)
        {
            slang::UserAttribute* attribute = typeLayout->getType()->getUserAttributeByIndex(i);
            const std::string name = attribute->getName();
            if (name == SHADER_ATTRIBUTE_STANDALONE_TYPE)
            {
                ASSERT(typeLayout->getKind() == slang::TypeReflection::Kind::Struct)
                
                usize reflectionNameLength = 0;
                const char* reflectionName = attribute->getArgumentValueString(0, &reflectionNameLength);
                std::string reflectionTarget(reflectionName, reflectionNameLength);

                if (!reflectionTarget.ends_with(assetlib::SHADER_UNIFORM_TYPE_EXTENSION))
                    reflectionTarget.append(assetlib::SHADER_UNIFORM_TYPE_EXTENSION);
                
                return reflectionTarget;
            }                
        }

        return std::nullopt;
    }

    std::string m_CurrentName{};
    const Context* m_Ctx{nullptr};
    const SlangBakeSettings* m_Settings{nullptr};
    std::unordered_map<slang::TypeReflection*, assetlib::AssetId> m_ProcessedEmbeddedStructTypes;
    std::vector<assetlib::ShaderUniformTypeEmbeddedStruct> m_EmbeddedStructs;
};

class LeafReflector
{
public:
    virtual ~LeafReflector() = default;
    virtual void ReflectTypeLayout(slang::TypeLayoutReflection* typeLayout) {}
protected:
    void ReflectVariableLayout(slang::VariableLayoutReflection* variableLayout)
    {
        const std::string oldName = m_CurrentName;
        m_CurrentVariable = variableLayout;
        m_CurrentName = createVariableName(m_CurrentName, variableLayout);
        ReflectTypeLayout(variableLayout->getTypeLayout());
        m_CurrentName = oldName;
    }
protected:
    std::string m_CurrentName{};
    slang::VariableLayoutReflection* m_CurrentVariable{};
};

class InputAttributeReflector final : public LeafReflector
{
public:
    std::vector<assetlib::ShaderInputAttribute> Reflect(slang::VariableLayoutReflection* variableLayout)
    {
        ReflectVariableLayout(variableLayout);

        std::vector<assetlib::ShaderInputAttribute> reflection = std::move(m_InputAttributes);
        m_InputAttributes.clear();

        return reflection;
    }

private:
    void ReflectTypeLayout(slang::TypeLayoutReflection* typeLayout) override
    {
        switch (typeLayout->getKind())
        {
        case slang::TypeReflection::Kind::Struct:
            ReflectStruct(typeLayout);
            break;
        case slang::TypeReflection::Kind::Array:
            ReflectArray(typeLayout);
            break;
        case slang::TypeReflection::Kind::Matrix:
            ReflectMatrix(typeLayout);
            break;
        case slang::TypeReflection::Kind::Vector:
            ReflectVector(typeLayout);
            break;
        case slang::TypeReflection::Kind::Scalar:
            ReflectScalar(typeLayout);
            break;
        default:
            break;
        }
    }

    void ReflectStruct(slang::TypeLayoutReflection* typeLayout)
    {
        for (u32 i = 0; i < typeLayout->getFieldCount(); i++)
            ReflectVariableLayout(typeLayout->getFieldByIndex(i));
    }

    void ReflectArray(slang::TypeLayoutReflection* typeLayout)
    {
        const u32 elementCount = (u32)typeLayout->getElementCount();

        const u32 currentInputIndex = (u32)m_InputAttributes.size();
        ReflectTypeLayout(typeLayout->getElementTypeLayout());
        const u32 newInputs = (u32)m_InputAttributes.size() - currentInputIndex;
        if (newInputs == 0)
            return;

        for (u32 elementIndex = 1; elementIndex < elementCount; elementIndex++)
        {
            for (u32 inputIndex = 0; inputIndex < newInputs; inputIndex++)
            {
                auto input = m_InputAttributes[currentInputIndex + inputIndex];
                input.Location = (u32)m_InputAttributes.size();
                m_InputAttributes.push_back(input);
            }
        }
    }

    void ReflectMatrix(slang::TypeLayoutReflection* typeLayout)
    {
        if (typeLayout->getSize(slang::ParameterCategory::VaryingInput) == 0)
            return;

        const u32 rowCount = typeLayout->getRowCount();
        const u32 colCount = typeLayout->getColumnCount();

        for (u32 i = 0; i < rowCount; i++)
        {
            m_InputAttributes.push_back({
                .Name = m_CurrentName,
                .Location = (u32)m_InputAttributes.size(),
                .ElementCount = colCount,
                .ElementScalar = slangScalarTypeToScalarType(typeLayout->getElementTypeLayout()->getScalarType())
            });
        }
    }

    void ReflectVector(slang::TypeLayoutReflection* typeLayout)
    {
        if (typeLayout->getSize(slang::ParameterCategory::VaryingInput) == 0)
            return;

        m_InputAttributes.push_back({
            .Name = m_CurrentName,
            .Location = (u32)m_InputAttributes.size(),
            .ElementCount = (u32)typeLayout->getElementCount(),
            .ElementScalar = slangScalarTypeToScalarType(typeLayout->getElementTypeLayout()->getScalarType())
        });
    }

    void ReflectScalar(slang::TypeLayoutReflection* typeLayout)
    {
        if (typeLayout->getSize(slang::ParameterCategory::VaryingInput) == 0)
            return;

        m_InputAttributes.push_back({
            .Name = m_CurrentName,
            .Location = (u32)m_InputAttributes.size(),
            .ElementCount = 1,
            .ElementScalar = slangScalarTypeToScalarType(typeLayout->getScalarType())
        });
    }
private:
    std::vector<assetlib::ShaderInputAttribute> m_InputAttributes;
};

class SpecializationConstantsReflector final : public LeafReflector
{
public:
    std::vector<assetlib::ShaderSpecializationConstants> Reflect(assetlib::ShaderStage shaderStages,
        slang::VariableLayoutReflection* variableLayout)
    {
        m_ShaderStages = shaderStages;
        ReflectVariableLayout(variableLayout);

        std::vector<assetlib::ShaderSpecializationConstants> reflection = std::move(m_SpecializationConstants);
        m_SpecializationConstants.clear();

        return reflection;
    }

private:
    void ReflectTypeLayout(slang::TypeLayoutReflection* typeLayout) override
    {
        switch (typeLayout->getKind())
        {
        case slang::TypeReflection::Kind::Struct:
            ReflectStruct(typeLayout);
            break;
        case slang::TypeReflection::Kind::Scalar:
            ReflectScalar(typeLayout);
            break;
        default:
            break;
        }
    }

    void ReflectStruct(slang::TypeLayoutReflection* typeLayout)
    {
        for (u32 i = 0; i < typeLayout->getFieldCount(); i++)
            ReflectVariableLayout(typeLayout->getFieldByIndex(i));
    }

    void ReflectScalar(slang::TypeLayoutReflection* typeLayout)
    {
        if (typeLayout->getSize(slang::ParameterCategory::SpecializationConstant) == 0)
            return;

        m_SpecializationConstants.push_back({
            .Name = m_CurrentName,
            .Id = (u32)m_CurrentVariable->getOffset(slang::ParameterCategory::SpecializationConstant),
            .Type = slangScalarTypeToScalarType(typeLayout->getScalarType()),
            .ShaderStages = m_ShaderStages
        });
    }
private:
    std::vector<assetlib::ShaderSpecializationConstants> m_SpecializationConstants;
    assetlib::ShaderStage m_ShaderStages{assetlib::ShaderStage::None};
};

class ShaderReflector
{
public:
    ShaderReflector(const Context& ctx, const SlangBakeSettings& settings) : m_Ctx(&ctx), m_Settings(&settings) {}
    assetlib::ShaderHeader Reflect(std::vector<slang::IModule*>& modules, slang::IComponentType* program,
        const std::vector<::Slang::ComPtr<slang::IMetadata>>& entryPointMetadata)
    {
        slang::ProgramLayout* layout = program->getLayout();
        ReflectGlobalLayout(layout);
        
        const u32 entryPointCount = (u32)layout->getEntryPointCount();
        for (u32 i = 0; i < entryPointCount; i++)
        {
            m_CurrentShaderStages = GetEntryPointShaderStage(layout->getEntryPointByIndex(i));
            ReflectEntryPointLayout(layout->getEntryPointByIndex(i));
        }

        ReflectDependencies(modules);

        for (auto& set : m_ShaderInfo.BindingSets)
            for (auto& binding : set.Bindings)
                binding.ShaderStages = GetBindingStageUsage(set.Set, binding.Binding, layout, entryPointMetadata);
        
        assetlib::ShaderHeader shaderInfo = std::move(m_ShaderInfo);
        m_ShaderInfo = {};

        return shaderInfo;
    }

private:
    struct ParameterBlockInfo
    {
        std::string Name{};
        slang::TypeLayoutReflection* TypeLayout{nullptr};
        slang::VariableLayoutReflection* VariableLayout{nullptr};
    };

    void ReflectGlobalLayout(slang::ProgramLayout* layout)
    {
        const u32 entryPointCount = (u32)layout->getEntryPointCount();
        for (u32 i = 0; i < entryPointCount; i++)
            m_CurrentShaderStages |= GetEntryPointShaderStage(layout->getEntryPointByIndex(i));

        m_ParameterBlocksToReflect.push({
            .Name = "",
            .TypeLayout = layout->getGlobalParamsTypeLayout(),
            .VariableLayout = layout->getGlobalParamsVarLayout()
        });
        ReflectParameterBlocks();

        SpecializationConstantsReflector specializationConstantsReflector = {};
        m_ShaderInfo.SpecializationConstants = specializationConstantsReflector.Reflect(m_CurrentShaderStages,
            layout->getGlobalParamsVarLayout());
    }

    void ReflectEntryPointLayout(slang::EntryPointLayout* layout)
    {
        m_ShaderInfo.EntryPoints.push_back({
            .Name = layout->getName(),
            .ShaderStage = m_CurrentShaderStages,
            .ThreadGroupSize = GetEntryPointThreadGroupSize(layout)
        });
        m_ParameterBlocksToReflect.push({
            .Name = "",
            .TypeLayout = layout->getTypeLayout(),
            .VariableLayout = layout->getVarLayout()
        });
        ReflectParameterBlocks();

        if (enumHasAny(m_CurrentShaderStages, assetlib::ShaderStage::Vertex))
        {
            InputAttributeReflector inputReflector = {};
            m_ShaderInfo.InputAttributes = inputReflector.Reflect(layout->getVarLayout());
        }
    }

    void ReflectDependencies(std::vector<slang::IModule*>& modules)
    {
        i32 dependencyCount = 0;
        for (auto* module : modules)
            dependencyCount += module->getDependencyFileCount();
        if (dependencyCount == 0)
            return;

        m_ShaderInfo.Includes.reserve(dependencyCount);
        for (auto* module : modules)
        {
            for (i32 i = 0; i < module->getDependencyFileCount(); i++)
            {
                std::string dependency = module->getDependencyFilePath(i);
                const auto it = std::ranges::find(m_ShaderInfo.Includes, dependency);
                if (it != m_ShaderInfo.Includes.end())
                    continue;
                
                m_ShaderInfo.Includes.push_back(std::move(dependency));
            }    
        }
    }

    static assetlib::ShaderStage GetBindingStageUsage(u32 setIndex, u32 bindingIndex, slang::ProgramLayout* layout,
        const std::vector<::Slang::ComPtr<slang::IMetadata>>& entryPointMetadata)
    {
        assetlib::ShaderStage stages = {};
        
        for (u32 i = 0; i < entryPointMetadata.size(); i++)
        {
            const assetlib::ShaderStage entryPointStage = GetEntryPointShaderStage(layout->getEntryPointByIndex(i));
            auto& metadata = entryPointMetadata[i];
            
            bool isUsed = false;
            metadata->isParameterLocationUsed(SLANG_PARAMETER_CATEGORY_DESCRIPTOR_TABLE_SLOT,
                setIndex, bindingIndex, isUsed);
            if (!isUsed)
                continue;

            stages |= entryPointStage;
        }

        return stages;
    }

    void ReflectParameterBlocks()
    {
        while (!m_ParameterBlocksToReflect.empty())
        {
            ParameterBlockInfo parameterBlockInfo = m_ParameterBlocksToReflect.top();
            m_ParameterBlocksToReflect.pop();

            std::optional<assetlib::ShaderUniform> uniformBufferReflection = std::nullopt;
            if (parameterBlockInfo.TypeLayout->getSize())
                uniformBufferReflection = ReflectUniformBuffer(parameterBlockInfo);
            ReflectParameterBlock(parameterBlockInfo);
            
            if (!m_CurrentBindings.empty())
            {
                m_ShaderInfo.BindingSets.push_back({
                    .Set = (u32)m_ShaderInfo.BindingSets.size(),
                    .Bindings = std::move(m_CurrentBindings),
                });
                if (uniformBufferReflection.has_value())
                    m_ShaderInfo.BindingSets.back().UniformType =
                        assetlib::shader::packUniform(*uniformBufferReflection).value_or("{}");
                m_CurrentBindings.clear();
            }
        }
    }

    void ReflectParameterBlock(const ParameterBlockInfo& blockInfo)
    {
        ReflectRanges(blockInfo);
    }

    std::optional<assetlib::ShaderUniform> ReflectUniformBuffer(const ParameterBlockInfo& blockInfo)
    {
        UniformTypeReflector uniformTypeReflector(*m_Ctx, *m_Settings);
        
        std::optional<assetlib::ShaderUniform> uniformTypeReflection =
            uniformTypeReflector.Reflect(blockInfo.Name, blockInfo.VariableLayout);

        assetlib::ShaderBinding uniformBuffer = {
            .Name = blockInfo.Name,
            .Count = 1,
            .Binding = (u32)m_CurrentBindings.size(),
            .Type = assetlib::ShaderBindingType::UniformBuffer,
            .Access = assetlib::ShaderBindingAccess::Read,
        };

        m_CurrentBindings.push_back(std::move(uniformBuffer));

        return uniformTypeReflection;
    }

    void ReflectRanges(const ParameterBlockInfo& blockInfo)
    {
        ReflectBindingRanges(blockInfo);
        ReflectSubObjectRanges(blockInfo);
    }

    void ReflectBindingRanges(const ParameterBlockInfo& blockInfo)
    {
        const i32 rangeCount = (i32)blockInfo.TypeLayout->getDescriptorSetDescriptorRangeCount(RELATIVE_SET_INDEX);

        for (i32 rangeIndex = 0; rangeIndex < rangeCount; rangeIndex++)
            ReflectBindingRange(blockInfo, rangeIndex);
    }

    void ReflectBindingRange(const ParameterBlockInfo& blockInfo, i32 rangeIndex)
    {
        const slang::BindingType bindingType =
            blockInfo.TypeLayout->getDescriptorSetDescriptorRangeType(RELATIVE_SET_INDEX, rangeIndex);

        if (bindingType == slang::BindingType::PushConstant)
            return;

        const u32 descriptorCount =
            (u32)blockInfo.TypeLayout->getDescriptorSetDescriptorRangeDescriptorCount(RELATIVE_SET_INDEX, rangeIndex);
        assetlib::ShaderBinding binding = {
            .Name = GetBindingName(blockInfo, rangeIndex),
            .Count = descriptorCount,
            .Binding = (u32)m_CurrentBindings.size(),
            .Type = slangBindingTypeToBindingType(bindingType),
            .Access = GetResourceAccess(blockInfo, rangeIndex),
            .Attributes = GetBindingAttributes(blockInfo, rangeIndex)
        };

        m_CurrentBindings.push_back(std::move(binding));
    }

    void ReflectSubObjectRanges(const ParameterBlockInfo& blockInfo)
    {
        const i32 rangeCount = (i32)blockInfo.TypeLayout->getSubObjectRangeCount();

        for (i32 rangeIndex = rangeCount - 1; rangeIndex >= 0; rangeIndex--)
            ReflectSubObjectRange(blockInfo, rangeIndex);
    }

    void ReflectSubObjectRange(const ParameterBlockInfo& blockInfo, i32 rangeIndex)
    {
        const i32 bindingRangeIndex = (i32)blockInfo.TypeLayout->getSubObjectRangeBindingRangeIndex(rangeIndex);
        switch (blockInfo.TypeLayout->getBindingRangeType(bindingRangeIndex))
        {
        case slang::BindingType::ParameterBlock:
            m_ParameterBlocksToReflect.push({
                .Name = std::format("{}_{}",
                    blockInfo.Name, blockInfo.TypeLayout->getBindingRangeLeafVariable(bindingRangeIndex)->getName()),
                .TypeLayout =
                blockInfo.TypeLayout->getBindingRangeLeafTypeLayout(bindingRangeIndex)->getElementTypeLayout(),
                .VariableLayout =
                blockInfo.TypeLayout->getBindingRangeLeafTypeLayout(bindingRangeIndex)->getElementVarLayout()
            });
            break;
        case slang::BindingType::PushConstant:
            ReflectPushConstant(blockInfo.TypeLayout->getBindingRangeLeafTypeLayout(bindingRangeIndex));
            break;
        default:
            break;
        }
    }

    void ReflectPushConstant(slang::TypeLayoutReflection* constantBufferTypeLayout)
    {
        slang::TypeLayoutReflection* elementTypeLayout = constantBufferTypeLayout->getElementTypeLayout();
        const u32 elementSize = (u32)elementTypeLayout->getSize();
        if (elementSize == 0)
            return;

        m_ShaderInfo.PushConstant = {
            .SizeBytes = elementSize,
            .Offset = 0,
            .ShaderStages = m_CurrentShaderStages
        };
    }

    static std::string GetBindingName(const ParameterBlockInfo& blockInfo, i32 rangeIndex)
    {
        slang::VariableReflection* variableReflection = blockInfo.TypeLayout->getBindingRangeLeafVariable(rangeIndex);
        if (!variableReflection)
            return blockInfo.Name;
        const char* name = variableReflection->getName();
        if (!name)
            return blockInfo.Name;

        return std::format("{}_{}", blockInfo.Name, name);
    }

    static assetlib::ShaderBindingAccess GetResourceAccess(const ParameterBlockInfo& blockInfo, i32 rangeIndex)
    {
        slang::TypeReflection* typeReflection =
            blockInfo.TypeLayout->getBindingRangeLeafTypeLayout(rangeIndex)->getType();

        ASSERT(typeReflection)
        if (!typeReflection)
            return assetlib::ShaderBindingAccess::Read;

        switch (typeReflection->getResourceAccess())
        {
        case SLANG_RESOURCE_ACCESS_READ:
            return assetlib::ShaderBindingAccess::Read;
        case SLANG_RESOURCE_ACCESS_READ_WRITE:
            return assetlib::ShaderBindingAccess::ReadWrite;
        case SLANG_RESOURCE_ACCESS_WRITE:
            return assetlib::ShaderBindingAccess::Write;
        case SLANG_RESOURCE_ACCESS_NONE:
            return assetlib::ShaderBindingAccess::Read;
        default:
            ASSERT(false)
            return assetlib::ShaderBindingAccess::Read;
        }
    }

    static assetlib::ShaderBindingAttributes GetBindingAttributes(const ParameterBlockInfo& blockInfo, i32 rangeIndex)
    {
        slang::VariableReflection* variableReflection =
            blockInfo.TypeLayout->getBindingRangeLeafVariable(rangeIndex);

        assetlib::ShaderBindingAttributes attributes = assetlib::ShaderBindingAttributes::None;
        
        if (!variableReflection)
            return attributes;
        
        const u32 attributeCount = variableReflection->getUserAttributeCount();
        for (u32 i = 0; i < attributeCount; i++)
        {
            slang::UserAttribute* attribute = variableReflection->getUserAttributeByIndex(i);
            const std::string name = attribute->getName();
            if (name == SHADER_ATTRIBUTE_BINDLESS)
            {
                attributes |= assetlib::ShaderBindingAttributes::Bindless;
            }
            else if (name == SHADER_ATTRIBUTE_IMMUTABLE_SAMPLER)
            {
                attributes |= assetlib::ShaderBindingAttributes::ImmutableSampler;

                i32 samplerFlags = 0;
                attribute->getArgumentValueInt(0, &samplerFlags);

                if (samplerFlags & (i32)assetlib::ShaderBindingAttributes::ImmutableSamplerNearest)
                    attributes |= assetlib::ShaderBindingAttributes::ImmutableSamplerNearest;
                if (samplerFlags & (i32)assetlib::ShaderBindingAttributes::ImmutableSamplerClampEdge)
                    attributes |= assetlib::ShaderBindingAttributes::ImmutableSamplerClampEdge;
                if (samplerFlags & (i32)assetlib::ShaderBindingAttributes::ImmutableSamplerClampBlack)
                    attributes |= assetlib::ShaderBindingAttributes::ImmutableSamplerClampBlack;
                if (samplerFlags & (i32)assetlib::ShaderBindingAttributes::ImmutableSamplerClampWhite)
                    attributes |= assetlib::ShaderBindingAttributes::ImmutableSamplerClampWhite;
                if (samplerFlags & (i32)assetlib::ShaderBindingAttributes::ImmutableSamplerShadow)
                    attributes |= assetlib::ShaderBindingAttributes::ImmutableSamplerShadow;
                if (samplerFlags & (i32)assetlib::ShaderBindingAttributes::ImmutableSamplerReductionMin)
                    attributes |= assetlib::ShaderBindingAttributes::ImmutableSamplerReductionMin;
                if (samplerFlags & (i32)assetlib::ShaderBindingAttributes::ImmutableSamplerReductionMax)
                    attributes |= assetlib::ShaderBindingAttributes::ImmutableSamplerReductionMax;
            }
        }

        return attributes;
    }

    static assetlib::ShaderStage GetEntryPointShaderStage(slang::EntryPointLayout* entryPointLayout)
    {
        switch (entryPointLayout->getStage())
        {
        case SLANG_STAGE_VERTEX:
            return assetlib::ShaderStage::Vertex;
        case SLANG_STAGE_COMPUTE:
            return assetlib::ShaderStage::Compute;
        case SLANG_STAGE_PIXEL:
            return assetlib::ShaderStage::Pixel;
        default:
            ASSERT(false, "Unsupported entry point stage")
            return assetlib::ShaderStage::None;
        }
    }

    glm::uvec3 GetEntryPointThreadGroupSize(slang::EntryPointLayout* entryPointLayout) const
    {
        if (m_CurrentShaderStages != assetlib::ShaderStage::Compute)
            return glm::uvec3(0);
        u64 group[3]{};
        entryPointLayout->getComputeThreadGroupSize(3, group);

        return glm::uvec3(group[0], group[1], group[2]);
    }

    assetlib::ShaderHeader m_ShaderInfo{};

    std::vector<assetlib::ShaderBinding> m_CurrentBindings;
    assetlib::ShaderStage m_CurrentShaderStages{assetlib::ShaderStage::None};
    std::stack<ParameterBlockInfo> m_ParameterBlocksToReflect;
    const Context* m_Ctx{nullptr};
    const SlangBakeSettings* m_Settings{nullptr};
    
    static constexpr i32 RELATIVE_SET_INDEX = 0;
};

std::string getBakedDefineAwareFileName(const std::filesystem::path& path, u64 definesHash)
{
    if (definesHash == 0)
        return path.filename().string();
    
    return std::format("{}-{}{}", path.stem().string(), definesHash, path.extension().string());
}

AssetPaths convertPathsToDefineAwarePaths(const AssetPaths& paths, u64 definesHash)
{
    AssetPaths converted = paths;
    converted.HeaderPath.replace_filename(getBakedDefineAwareFileName(converted.HeaderPath, definesHash));
    converted.BinaryPath.replace_filename(getBakedDefineAwareFileName(converted.BinaryPath, definesHash));

    return converted;
}

}

std::filesystem::path Slang::GetBakedPath(const std::filesystem::path& originalFile, const SlangBakeSettings& settings,
    const Context& ctx)
{
    std::filesystem::path path = getPostBakePath(originalFile, ctx);
    path.replace_filename(getBakedDefineAwareFileName(path, settings.DefinesHash));
    path.replace_extension(POST_BAKE_EXTENSION);

    return path;
}

IoResult<void> Slang::BakeToFile(const std::filesystem::path& path, const SlangBakeSettings& settings,
    const Context& ctx)
{
    auto baked = Bake(path, settings, ctx);
    if (!baked.has_value())
        return std::unexpected(baked.error());

    const AssetPaths paths = convertPathsToDefineAwarePaths(
        getPostBakePaths(path, ctx, POST_BAKE_EXTENSION, ctx.IoType), settings.DefinesHash);

    auto shaderHeader = assetlib::shader::packHeader(baked->Header);
    if (!shaderHeader.has_value())
        return std::unexpected(shaderHeader.error());
    
    const std::vector<std::byte> spirv = assetlib::shader::packBinary(baked->Spirv, ctx.CompressionMode);
    
    assetlib::AssetFile assetFile = {};
    assetFile.IoInfo = {
        .HeaderFile = paths.HeaderPath.generic_string(),
        .BinaryFile = paths.BinaryPath.generic_string(),
        .BinarySizeBytes = baked->Spirv.size(),
        .BinarySizeBytesCompressed = spirv.size(),
        .CompressionMode = ctx.CompressionMode,
    };
    assetFile.Metadata = assetlib::shader::generateMetadata(path.string());
    assetFile.AssetSpecificInfo = std::move(*shaderHeader);    

    switch (ctx.IoType)
    {
    case assetlib::AssetFileIoType::Separate:
        return assetlib::io::saveAssetFile(assetFile, spirv);
    case assetlib::AssetFileIoType::Combined:
        return assetlib::io::saveAssetFileCombined(assetFile, spirv);
    default:
        return std::unexpected(IoError{IoError::ErrorCode::GeneralError, "Unknown io type"});
    }
}

IoResult<assetlib::ShaderAsset> Slang::Bake(const std::filesystem::path& path,
    const SlangBakeSettings& settings, const Context& ctx)
{
    slang::ISession& session = getSession(settings);
    ::Slang::ComPtr<slang::IBlob> diagnosticsBlob;

    const auto loadInfo = assetlib::shader::readLoadInfo(path);
    if (!loadInfo.has_value())
        return std::unexpected(loadInfo.error());

    std::vector<slang::IModule*> shaderModules;
    shaderModules.reserve(loadInfo->EntryPoints.size());
    for (const auto& entryPoint : loadInfo->EntryPoints)
    {
        slang::IModule* shaderModule = session.loadModule(entryPoint.Path.c_str(), diagnosticsBlob.writeRef());
        CHECK_RETURN_IO_ERROR(shaderModule, IoError::ErrorCode::FailedToLoad,
            "Shader::Bake: failed to load shader module: {} ({})",
            (const char*)diagnosticsBlob->getBufferPointer(), entryPoint.Path)

        shaderModules.push_back(shaderModule);
    }

    std::vector<::Slang::ComPtr<slang::IEntryPoint>> entryPoints;
    entryPoints.resize(loadInfo->EntryPoints.size());
    for (auto&& [i, entryPoint] : std::views::enumerate(loadInfo->EntryPoints))
    {
        shaderModules[i]->findEntryPointByName(entryPoint.Name.c_str(), entryPoints[i].writeRef());
        CHECK_RETURN_IO_ERROR(entryPoints[i], IoError::ErrorCode::FailedToLoad,
            "Shader::Bake: failed to load shader entry point module: {} ({})", entryPoint.Name, entryPoint.Path)
    }

    auto it = std::ranges::unique(shaderModules);
    shaderModules.erase(it.begin(), shaderModules.end());
    
    std::vector<slang::IComponentType*> componentTypes;
    componentTypes.reserve(entryPoints.size() + shaderModules.size());
    for (auto* module : shaderModules)
        componentTypes.push_back(module);
    for (auto& entryPoint : entryPoints)
        componentTypes.push_back(entryPoint);
    
    ::Slang::ComPtr<slang::IComponentType> composedProgram;
    auto res = session.createCompositeComponentType(componentTypes.data(), (SlangInt)componentTypes.size(),
        composedProgram.writeRef(), diagnosticsBlob.writeRef());
    CHECK_RETURN_IO_ERROR(!SLANG_FAILED(res), IoError::ErrorCode::GeneralError,
        "Shader::Bake: failed to create composed shader program: {} ({})",
        (const char*)diagnosticsBlob->getBufferPointer(), path.string())

    ::Slang::ComPtr<slang::IComponentType> linkedProgram;
    res = composedProgram->link(linkedProgram.writeRef(), diagnosticsBlob.writeRef());
    CHECK_RETURN_IO_ERROR(!SLANG_FAILED(res), IoError::ErrorCode::GeneralError,
        "Shader::Bake: failed to link shader program: {} ({})",
        (const char*)diagnosticsBlob->getBufferPointer(), path.string())

    ::Slang::ComPtr<slang::IBlob> spirvCode;
    res = linkedProgram->getTargetCode(
        /*targetIndex=*/ 0,
        spirvCode.writeRef(),
        diagnosticsBlob.writeRef());
    CHECK_RETURN_IO_ERROR(!SLANG_FAILED(res), IoError::ErrorCode::GeneralError,
        "Shader::Bake: failed to get shader spirv: {} ({})",
        (const char*)diagnosticsBlob->getBufferPointer(), path.string())

    std::vector<::Slang::ComPtr<slang::IMetadata>> entryPointMetadata(linkedProgram->getLayout()->getEntryPointCount());
    for (u32 i = 0; i < linkedProgram->getLayout()->getEntryPointCount(); i++)
    {
        res = linkedProgram->getEntryPointMetadata(i, 0, entryPointMetadata[i].writeRef(), diagnosticsBlob.writeRef());
        CHECK_RETURN_IO_ERROR(!SLANG_FAILED(res), IoError::ErrorCode::GeneralError,
            "Shader::Bake: failed to get shader entry point metadata: {} ({})",
            (const char*)diagnosticsBlob->getBufferPointer(), path.string())
    }
    

    ShaderReflector reflector(ctx, settings);
    assetlib::ShaderAsset shaderAsset = {};
    shaderAsset.Header = reflector.Reflect(shaderModules, linkedProgram, entryPointMetadata);
    shaderAsset.Header.Name = loadInfo->Name;
    shaderAsset.Spirv.resize(spirvCode->getBufferSize());
    memcpy(shaderAsset.Spirv.data(), spirvCode->getBufferPointer(), spirvCode->getBufferSize());
    
    return shaderAsset;
}
}

#undef CHECK_RETURN_IO_ERROR