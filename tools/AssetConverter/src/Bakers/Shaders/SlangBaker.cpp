#include "SlangBaker.h"

#include "core.h"

#include <queue>
#include <stack>
#include <unordered_map>
#include <ranges>
#include <glm/vec3.hpp>
#include <nlohmann/json.hpp>
#include <slang/slang.h>
#include <slang/slang-com-ptr.h>
#include <slang/slang-com-helper.h>

// todo: move to assetlib
namespace assetLib
{

enum class ShaderStage : u8
{
    None = 0,
    Vertex = BIT(0),
    Pixel = BIT(1),
    Compute = BIT(2),
};

enum class ShaderBindingType : u8
{
    None,
    Sampler,
    Image,
    ImageStorage,
    TexelUniform,
    TexelStorage,
    UniformBuffer,
    UniformTexelBuffer,
    StorageBuffer,
    StorageTexelBuffer,
    UniformBufferDynamic,
    StorageBufferDynamic,
    Input,
};

enum class ShaderBindingAccess : u8
{
    Read,
    Write,
    ReadWrite,
};

enum class ShaderBindingAttributes
{
    None = 0,
};

struct ShaderBinding
{
    std::string Name;
    ShaderStage ShaderStages{};
    u32 Count{1};
    u32 Binding{0};
    ShaderBindingType Type{ShaderBindingType::None};
    ShaderBindingAccess Access{ShaderBindingAccess::Read};
    ShaderBindingAttributes Attributes{ShaderBindingAttributes::None};
};

struct ShaderBindingSet
{
    u32 Set{0};
    std::vector<ShaderBinding> Bindings;
};

struct ShaderPushConstant
{
    u32 SizeBytes{};
    u32 Offset{};
    ShaderStage ShaderStages{};
};

struct ShaderEntryPoint
{
    std::string Name;
    ShaderStage ShaderStage{};
    glm::uvec3 ThreadGroupSize{};
};

struct ShaderAsset
{
    std::vector<ShaderBindingSet> BindingSets{};
    std::vector<ShaderEntryPoint> EntryPoints{};
};

}

CREATE_ENUM_FLAGS_OPERATORS(assetLib::ShaderStage);
CREATE_ENUM_FLAGS_OPERATORS(assetLib::ShaderBindingAttributes);

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

slang::ISession& getSession(const SlangBakeSetting& settings)
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
        }
    };

    std::vector<slang::PreprocessorMacroDesc> macros(settings.Defines.size());
    for (auto&& [i, define] : std::views::enumerate(settings.Defines))
        macros[i] = {
            .name = define.first.c_str(),
            .value = define.second.c_str()
        };

    slang::SessionDesc sessionDesc = {
        .targets = &targetDesc,
        .targetCount = 1,
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

assetLib::ShaderBindingType slangBindingTypeToBindingType(slang::BindingType bindingType)
{
    using enum assetLib::ShaderBindingType;
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

class UniformTypeReflector
{
public:
    nlohmann::json Reflect(const std::string& baseName, slang::VariableLayoutReflection* variableLayout)
    {
        m_BaseName = baseName;
        return *ReflectVariableLayout(variableLayout);
    }
private:
    std::optional<nlohmann::json> ReflectVariableLayout(slang::VariableLayoutReflection* variableLayout)
    {
        slang::TypeLayoutReflection* typeLayout = variableLayout->getTypeLayout();
        if (!typeLayout->getSize())
            return std::nullopt;

        nlohmann::json reflection = ReflectCommonVariableInfo(variableLayout);
        const std::string currentBaseName = m_BaseName;
        m_BaseName = reflection["name"].get<std::string>();
        reflection["type"] = ReflectTypeLayout(typeLayout);
        m_BaseName = currentBaseName;
        
        return reflection;
    }
    
    nlohmann::json ReflectCommonVariableInfo(slang::VariableLayoutReflection* variableLayout)
    {
        nlohmann::json reflection = {};
        const char* variableName = variableLayout->getName();
        reflection["name"] = variableName ? std::format("{}_{}", m_BaseName, variableName) : m_BaseName;
        reflection["offset"] = variableLayout->getOffset();
        reflection["size"] = variableLayout->getTypeLayout()->getSize();
        reflection["kind"] = SlangTypeKindToString(variableLayout->getTypeLayout()->getKind());
        
        return reflection;
    }

    nlohmann::json ReflectTypeLayout(slang::TypeLayoutReflection* typeLayout)
    {
        nlohmann::json reflection = {};
        switch (typeLayout->getKind())
        {
        case slang::TypeReflection::Kind::Array:
            reflection["element_count"] = typeLayout->getElementCount();
            reflection["element"] = ReflectTypeLayout(typeLayout->getElementTypeLayout());
            break;
        case slang::TypeReflection::Kind::Matrix:
            reflection = SlangMatrixTypeToString(typeLayout);
            break;
        case slang::TypeReflection::Kind::Vector:
            reflection = SlangVectorTypeToString(typeLayout);
            break;
        case slang::TypeReflection::Kind::Scalar:
            reflection = SlangScalarTypeToString(typeLayout->getScalarType());
            break;
        case slang::TypeReflection::Kind::Struct:
            reflection["fields"] = {};
            for (u32 i = 0; i < typeLayout->getFieldCount(); i++)
            {
                const std::optional<nlohmann::json> reflectionField =
                    ReflectVariableLayout(typeLayout->getFieldByIndex(i));
                if (reflectionField.has_value())
                    reflection["fields"].push_back(*reflectionField);    
            }
            break;
        default:
            ASSERT(false)
            break;
        }

        return reflection;
    }

    static std::string SlangTypeKindToString(slang::TypeReflection::Kind kind)
    {
        using enum slang::TypeReflection::Kind;
        switch (kind)
        {
        case Struct:                return "struct";
        case Array:                 return "array";
        case Matrix:                return "matrix";
        case Vector:                return "vector";
        case Scalar:                return "scalar";
        case ConstantBuffer:        return "constant_buffer";
        case Resource:              return "resource";
        case SamplerState:          return "sampler";
        case TextureBuffer:         return "texture_buffer";
        case ShaderStorageBuffer:   return "shader_storage_buffer";
        case ParameterBlock:        return "parameter_block";
        case GenericTypeParameter:  return "generic_parameter";
        case Interface:             return "interface";
        case OutputStream:          return "output_stream";
        case Specialized:           return "specialized";
        case Feedback:              return "feedback";
        case Pointer:               return "pointer";
        case DynamicResource:       return "dynamic_resource";
        default:
            ASSERT(false)
            return "";
        }
    }

    static std::string SlangMatrixTypeToString(slang::TypeLayoutReflection* typeLayout)
    {
        const u32 rowCount = typeLayout->getRowCount();
        const u32 colCount = typeLayout->getColumnCount();

        /* the element type of matrix is vector, which contradicts math definition and logic >:( */
        slang::TypeLayoutReflection* elementTypeLayout = typeLayout->getElementTypeLayout()->getElementTypeLayout();
        ASSERT(rowCount > 0 && rowCount <= 4)
        ASSERT(colCount > 0 && colCount <= 4)
        ASSERT(elementTypeLayout->getKind() == slang::TypeReflection::Kind::Scalar)
        switch (elementTypeLayout->getScalarType())
        {
        case slang::TypeReflection::Int32:
            return std::format("imat{}x{}", colCount, rowCount);
        case slang::TypeReflection::UInt32:
            return std::format("umat{}x{}", colCount, rowCount);
        case slang::TypeReflection::Float32:
            return std::format("mat{}x{}", colCount, rowCount);
        case slang::TypeReflection::Float64:
            return std::format("dmat{}x{}", colCount, rowCount);
        default:
            ASSERT(false, "Unsupported vector layout type {}", (u32)elementTypeLayout->getScalarType())
            return "mat";
        }
    }

    static std::string SlangVectorTypeToString(slang::TypeLayoutReflection* typeLayout)
    {
        const u32 elementCount = (u32)typeLayout->getElementCount();
        slang::TypeLayoutReflection* elementTypeLayout = typeLayout->getElementTypeLayout();
        ASSERT(elementCount > 0 && elementCount <= 4)
        ASSERT(elementTypeLayout->getKind() == slang::TypeReflection::Kind::Scalar)
        switch (elementTypeLayout->getScalarType())
        {
        case slang::TypeReflection::Bool:
            return std::format("bvec{}", elementCount);
        case slang::TypeReflection::Int32:
            return std::format("ivec{}", elementCount);
        case slang::TypeReflection::UInt32:
            return std::format("uvec{}", elementCount);
        case slang::TypeReflection::Float32:
            return std::format("vec{}", elementCount);
        case slang::TypeReflection::Float64:
            return std::format("dvec{}", elementCount);
        default:
            ASSERT(false, "Unsupported vector layout type {}", (u32)elementTypeLayout->getScalarType())
            return "vec";
        }
    }
    
    static std::string SlangScalarTypeToString(slang::TypeReflection::ScalarType scalarType)
    {
        switch (scalarType)
        {
        case slang::TypeReflection::Void:       return "void";
        case slang::TypeReflection::Bool:       return "bool";
        case slang::TypeReflection::Int32:      return "i32";
        case slang::TypeReflection::UInt32:     return "u32";
        case slang::TypeReflection::Int64:      return "i64";
        case slang::TypeReflection::UInt64:     return "u64";
        case slang::TypeReflection::Float16:    return "f16";
        case slang::TypeReflection::Float32:    return "f32";
        case slang::TypeReflection::Float64:    return "f64";
        case slang::TypeReflection::Int8:       return "i8";
        case slang::TypeReflection::UInt8:      return "u8";
        case slang::TypeReflection::Int16:      return "i16";
        case slang::TypeReflection::UInt16:     return "u16";
        default:
            ASSERT(false)
            return "";
        }
    }

    std::string m_BaseName{};
};

class LayoutReflector
{
public:
    void Reflect(slang::IComponentType* program)
    {
        slang::ProgramLayout* layout = program->getLayout();
        ReflectGlobalLayout(layout);

        const u32 entryPointCount = (u32)layout->getEntryPointCount();
        for (u32 i = 0; i < entryPointCount; i++)
        {
            m_CurrentShaderStages = GetEntryPointShaderStage(layout->getEntryPointByIndex(i));
            ReflectEntryPointLayout(layout->getEntryPointByIndex(i));
        }
    }
private:
    struct ParameterBlockInfo
    {
        std::string Name{};
        slang::TypeLayoutReflection* TypeLayout{nullptr};
        slang::VariableLayoutReflection* VariableLayout{nullptr};
    };
    struct UniformTypeReflectionInfo
    {
        u32 Set{0};
        nlohmann::json Reflection{};
    };
    void ReflectGlobalLayout(slang::ProgramLayout* layout)
    {
        const u32 entryPointCount = (u32)layout->getEntryPointCount();
        for (u32 i = 0; i < entryPointCount; i++)
            m_CurrentShaderStages |= GetEntryPointShaderStage(layout->getEntryPointByIndex(i));
        
        m_ParameterBlocksToReflect.push({
            .Name = "",
            .TypeLayout = layout->getGlobalParamsTypeLayout(),
            .VariableLayout = layout->getGlobalParamsVarLayout()});
        ReflectParameterBlocks();
    }
    void ReflectEntryPointLayout(slang::EntryPointLayout* layout)
    {
        m_EntryPoints.push_back({
            .Name = layout->getName(),
            .ShaderStage = m_CurrentShaderStages,
            .ThreadGroupSize = GetEntryPointThreadGroupSize(layout)
        });
        m_ParameterBlocksToReflect.push({
            .Name = "",
            .TypeLayout = layout->getTypeLayout(),
            .VariableLayout = layout->getVarLayout()});
        ReflectParameterBlocks();
    }
    void ReflectParameterBlocks()
    {
        while (!m_ParameterBlocksToReflect.empty())
        {
            ParameterBlockInfo parameterBlockInfo = m_ParameterBlocksToReflect.top();
            m_ParameterBlocksToReflect.pop();

            ReflectParameterBlock(parameterBlockInfo);

            if (!m_CurrentBindings.empty())
            {
                m_Sets.push_back({
                    .Set = (u32)m_Sets.size(),
                    .Bindings = std::move(m_CurrentBindings)
                });
                m_CurrentBindings.clear();
            }
        }
    }
    void ReflectParameterBlock(const ParameterBlockInfo& blockInfo)
    {
        if (blockInfo.TypeLayout->getSize())
            ReflectUniformBuffer(blockInfo);
        ReflectRanges(blockInfo);
    }
    void ReflectUniformBuffer(const ParameterBlockInfo& blockInfo)
    {
        UniformTypeReflector uniformTypeReflector = {};
        m_UniformTypeReflections.push_back({
            .Set = (u32)m_Sets.size(),
            .Reflection = uniformTypeReflector.Reflect(blockInfo.Name, blockInfo.VariableLayout)
        });
        
        assetLib::ShaderBinding uniformBuffer = {
            .Name = blockInfo.Name + "_auto_uniform",
            .ShaderStages = m_CurrentShaderStages,
            .Count = 1,
            .Binding = (u32)m_CurrentBindings.size(),
            .Type = assetLib::ShaderBindingType::UniformBuffer,
            .Access = assetLib::ShaderBindingAccess::Read,
        };

        m_CurrentBindings.push_back(std::move(uniformBuffer));
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

        u32 descriptorCount =
            (u32)blockInfo.TypeLayout->getDescriptorSetDescriptorRangeDescriptorCount(RELATIVE_SET_INDEX, rangeIndex);
        assetLib::ShaderBinding binding = {
            .Name = GetBindingName(blockInfo, rangeIndex),
            .ShaderStages = m_CurrentShaderStages,
            .Count = descriptorCount,
            .Binding = (u32)m_CurrentBindings.size(),
            .Type = slangBindingTypeToBindingType(bindingType),
            .Access = GetResourceAccess(blockInfo, rangeIndex),
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
        const slang::BindingType bindingType = blockInfo.TypeLayout->getBindingRangeType(bindingRangeIndex);
        switch (bindingType)
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
        if(elementSize == 0)
            return;

        m_PushConstant = {
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
    static assetLib::ShaderBindingAccess GetResourceAccess(const ParameterBlockInfo& blockInfo, i32 rangeIndex)
    {
        slang::TypeReflection* typeReflection =
            blockInfo.TypeLayout->getBindingRangeLeafTypeLayout(rangeIndex)->getType();

        ASSERT(typeReflection)
        if (!typeReflection)
            return assetLib::ShaderBindingAccess::Read;

        switch (typeReflection->getResourceAccess())
        {
        case SLANG_RESOURCE_ACCESS_READ:
            return assetLib::ShaderBindingAccess::Read;
        case SLANG_RESOURCE_ACCESS_READ_WRITE:
            return assetLib::ShaderBindingAccess::ReadWrite;
        case SLANG_RESOURCE_ACCESS_WRITE:
            return assetLib::ShaderBindingAccess::Write;
        case SLANG_RESOURCE_ACCESS_NONE:
            return assetLib::ShaderBindingAccess::Read;
        default:
            ASSERT(false)
            return assetLib::ShaderBindingAccess::Read;
        }
    }
    static assetLib::ShaderStage GetEntryPointShaderStage(slang::EntryPointLayout* entryPointLayout)
    {
        switch (entryPointLayout->getStage())
        {
        case SLANG_STAGE_VERTEX:
            return assetLib::ShaderStage::Vertex;
        case SLANG_STAGE_COMPUTE:
            return assetLib::ShaderStage::Compute;
        case SLANG_STAGE_PIXEL:
            return assetLib::ShaderStage::Pixel;
        default:
            ASSERT(false, "Unsupported entry point stage")
            return assetLib::ShaderStage::None;
        }
    }
    glm::uvec3 GetEntryPointThreadGroupSize(slang::EntryPointLayout* entryPointLayout) const
    {
        if (m_CurrentShaderStages != assetLib::ShaderStage::Compute)
            return glm::uvec3(0);
        u64 group[3]{};
        entryPointLayout->getComputeThreadGroupSize(3, group);

        return glm::uvec3(group[0], group[1], group[2]);
    }
    
    std::vector<UniformTypeReflectionInfo> m_UniformTypeReflections;
    std::vector<assetLib::ShaderBinding> m_CurrentBindings;
    std::vector<assetLib::ShaderBindingSet> m_Sets;
    std::vector<assetLib::ShaderEntryPoint> m_EntryPoints;
    assetLib::ShaderPushConstant m_PushConstant{};
    assetLib::ShaderStage m_CurrentShaderStages{assetLib::ShaderStage::None};
    std::stack<ParameterBlockInfo> m_ParameterBlocksToReflect;
    
    static constexpr i32 RELATIVE_SET_INDEX = 0;
};

}

BakeResult Slang::Bake(const std::filesystem::path& path, const SlangBakeSetting& settings, const Context& ctx)
{
    // todo: correct errors
    slang::ISession& session = getSession(settings);
    ::Slang::ComPtr<slang::IBlob> diagnosticsBlob;

    slang::IModule* shaderModule = session.loadModule(path.string().c_str(), diagnosticsBlob.writeRef());
    if (!shaderModule)
        return std::unexpected{BakeError::Unknown};

    ::Slang::ComPtr<slang::IEntryPoint> entryPoint;
    shaderModule->findEntryPointByName("computeMain", entryPoint.writeRef());
    if (!entryPoint)
        return std::unexpected{BakeError::Unknown};

    const std::array componentTypes = {
        (slang::IComponentType*)shaderModule,
        (slang::IComponentType*)entryPoint
    };

    ::Slang::ComPtr<slang::IComponentType> composedProgram;
    auto res = session.createCompositeComponentType(componentTypes.data(), componentTypes.size(),
        composedProgram.writeRef(), diagnosticsBlob.writeRef());
    if (SLANG_FAILED(res))
        return std::unexpected{BakeError::Unknown};

    ::Slang::ComPtr<slang::IComponentType> linkedProgram;
    res = composedProgram->link(linkedProgram.writeRef(), diagnosticsBlob.writeRef());
    if (SLANG_FAILED(res))
        return std::unexpected{BakeError::Unknown};

    ::Slang::ComPtr<slang::IBlob> spirvCode;
    res = linkedProgram->getEntryPointCode(
        /*entryPointIndex=*/ 0,
        /*targetIndex=*/ 0,
        spirvCode.writeRef(),
        diagnosticsBlob.writeRef());
    if (SLANG_FAILED(res))
        return std::unexpected{BakeError::Unknown};

    LayoutReflector reflector = {};
    reflector.Reflect(linkedProgram);

    return {};
}
}
