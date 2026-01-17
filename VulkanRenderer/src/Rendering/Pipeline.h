#pragma once

#include "ResourceHandle.h"

#include "Descriptors.h"
#include "DescriptorsTraits.h"
#include "Containers/Span.h"
#include "Shader/ShaderModule.h"

#include <vector>

class DeletionQueue;

static constexpr u32 MAX_PIPELINE_SHADER_COUNT = 5;

struct PushConstantDescription
{
    u32 SizeBytes{};
    u32 Offset{};
    ShaderStage StageFlags{};
};

struct PipelineLayoutCreateInfo
{
    Span<const PushConstantDescription> PushConstants{};
    Span<const DescriptorsLayout> DescriptorsLayouts{};
};

struct PipelineLayoutTag{};
using PipelineLayout = ResourceHandleType<PipelineLayoutTag>;

struct PipelineSpecializationDescription
{
    u32 Id{};
    u32 SizeBytes{};
    u32 Offset{};
    ShaderStage ShaderStages{};
};

struct PipelineSpecializationsView
{
    Span<const std::byte> Data{};
    Span<const PipelineSpecializationDescription> Descriptions{};

    constexpr PipelineSpecializationsView() = default;
    constexpr PipelineSpecializationsView(
        Span<const std::byte> data, Span<PipelineSpecializationDescription> descriptions)
        : Data(data), Descriptions(descriptions) {}
};

enum class DynamicStates : u8
{
    None        = 0,
    Viewport    = BIT(1),
    Scissor     = BIT(2),
    DepthBias   = BIT(3),

    Default     = Viewport | Scissor,
};
CREATE_ENUM_FLAGS_OPERATORS(DynamicStates)

enum class DepthMode : u8 {Read, ReadWrite, None};

enum class DepthTest : u8 {GreaterOrEqual, Equal};

enum class FaceCullMode : u8 {Front, Back, None};

enum class PrimitiveKind : u8 {Triangle, Point};

enum class AlphaBlending : u8 {None, Over};

struct VertexInputDescription
{
    struct Binding
    {
        u32 Index;
        u32 StrideBytes;
    };
    struct Attribute
    {
        u32 Index;
        u32 BindingIndex;
        Format Format;
        u32 OffsetBytes;
    };
    std::vector<Binding> Bindings;
    std::vector<Attribute> Attributes;
};

// todo: maybe it is possible to remove VertexInputDescription and leave only this struct
// it is hard to tell, because no pipeline uses it atm
struct VertexInputDescriptionView
{
    Span<const VertexInputDescription::Binding> Bindings;
    Span<const VertexInputDescription::Attribute> Attributes;

    VertexInputDescriptionView() = default;
    VertexInputDescriptionView(const VertexInputDescription& description)
        : Bindings(description.Bindings), Attributes(description.Attributes) {}
};

struct PipelineCreateInfo
{
    PipelineLayout PipelineLayout{};
    Span<const ShaderModule> Shaders{};
    Span<const ShaderStage> ShaderStages{};
    Span<const std::string> ShaderEntryPoints{};
    Span<const Format> ColorFormats{};
    Format DepthFormat{Format::Undefined};
    VertexInputDescriptionView VertexDescription{};
    DynamicStates DynamicStates{DynamicStates::Default};
    DepthMode DepthMode{DepthMode::ReadWrite};
    DepthTest DepthTest{DepthTest::GreaterOrEqual};
    FaceCullMode CullMode{FaceCullMode::None};
    AlphaBlending AlphaBlending{AlphaBlending::Over};
    PrimitiveKind PrimitiveKind{PrimitiveKind::Triangle};
    PipelineSpecializationsView Specialization{};
    bool IsComputePipeline{false};
    bool ClampDepth{false};
};

struct PipelineTag{};
using Pipeline = ResourceHandleType<PipelineTag>;