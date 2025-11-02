#pragma once

#include "ShaderImageFormat.h"
#include "v2/AssetLibV2.h"

namespace assetlib
{
enum class ShaderRasterizationDynamicState : u8
{
    None,
    Viewport,
    Scissor,
    DepthBias,
    
    MaxVal
};
using ShaderRasterizationDynamicStates =
    std::array<ShaderRasterizationDynamicState, (u32)ShaderRasterizationDynamicState::MaxVal>;

enum class ShaderRasterizationDepthMode : u8 { Read, ReadWrite, None };
enum class ShaderRasterizationDepthTest : u8 { GreaterOrEqual, Equal };
enum class ShaderRasterizationFaceCullMode : u8 { Front, Back, None };
enum class ShaderRasterizationPrimitiveKind : u8 { Triangle, Point };
enum class ShaderRasterizationAlphaBlending : u8 { None, Over };

struct ShaderLoadRasterizationColor
{
    ShaderImageFormat Format{};
    std::string Name{};
};

struct ShaderLoadRasterizationInfo
{
    ShaderRasterizationDynamicStates DynamicStates{
        ShaderRasterizationDynamicState::Viewport,
        ShaderRasterizationDynamicState::Scissor
    };
    ShaderRasterizationDepthMode DepthMode{ShaderRasterizationDepthMode::ReadWrite};
    ShaderRasterizationDepthTest DepthTest{ShaderRasterizationDepthTest::GreaterOrEqual};
    ShaderRasterizationFaceCullMode FaceCullMode{ShaderRasterizationFaceCullMode::Back};
    ShaderRasterizationPrimitiveKind PrimitiveKind{ShaderRasterizationPrimitiveKind::Triangle};
    ShaderRasterizationAlphaBlending AlphaBlending{ShaderRasterizationAlphaBlending::Over};
    std::vector<ShaderLoadRasterizationColor> Colors{};
    std::optional<ShaderImageFormat> Depth{};
    bool ClampDepth{false};
};

struct ShaderLoadInfo
{
    std::string Name{};

    struct EntryPoint
    {
        std::string Name{};
        std::string Path{};
    };

    std::vector<EntryPoint> EntryPoints{};
    std::optional<std::string> BindlessSetReference{std::nullopt};
    std::optional<u32> BindlessCount{std::nullopt};
    std::optional<ShaderLoadRasterizationInfo> RasterizationInfo{std::nullopt};
};

namespace shader
{
io::IoResult<ShaderLoadInfo> readLoadInfo(const std::filesystem::path& path);
}
}
