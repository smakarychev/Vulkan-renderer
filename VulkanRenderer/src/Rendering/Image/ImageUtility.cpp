#include "rendererpch.h"

#include "ImageUtility.h"

#include "Vulkan/Device.h"

namespace Images
{
    u32 toRGBA8(const glm::vec4& color)
    {
        u8 r = (u8)(color.r * 255.0f);
        u8 g = (u8)(color.g * 255.0f);
        u8 b = (u8)(color.b * 255.0f);
        u8 a = (u8)(color.a * 255.0f);

        return r | g << 8 | b << 16 | a << 24;
    }

    u32 toRGBA8SNorm(const glm::vec4& color)
    {
        i8 r = (i8)(color.r * 127.0f);
        i8 g = (i8)(color.g * 127.0f);
        i8 b = (i8)(color.b * 127.0f);
        i8 a = (i8)(color.a * 127.0f);

        return *(u8*)&r | *(u8*)&g << 8 | *(u8*)&b << 16 | *(u8*)&a << 24;
    }

    i8 mipmapCount(const glm::uvec2& resolution)
    {
        return mipmapCount({resolution.x, resolution.y, 1});
    }

    i8 mipmapCount(const glm::uvec3& resolution)
    {
        u32 maxDimension = std::max(resolution.x, std::max(resolution.y, resolution.z));

        return (i8)std::max(1, (i8)std::log2(maxDimension) + (i8)!Math::isPowerOf2(maxDimension));    
    }

    glm::uvec3 getPixelCoordinates(Image image, const glm::vec3& coordinate, ImageSizeType sizeType)
    {
        if (sizeType == ImageSizeType::Absolute)
            return glm::uvec3{coordinate};

        const TextureDescription& description = Device::GetImageDescription(image);
        glm::uvec3 size = {
            description.Width,
            description.Height,
            description.GetDepth()};

        return glm::uvec3 {
            (u32)((f32)size.x * coordinate.x), (u32)((f32)size.y * coordinate.y), (u32)((f32)size.z * coordinate.z)};
    }

    glm::uvec2 floorResolutionToPowerOfTwo(const glm::uvec2& resolution)
    {
        const u32 width = Math::floorToPowerOf2(resolution.x);
        const u32 height = Math::floorToPowerOf2(resolution.y);

        return glm::uvec2(width, height);
    }

    std::array<Default::ImageData, (u32)DefaultKind::MaxVal> Default::s_DefaultImages = {};
    
    void Default::Init()
    {
        ImageDescription description = {
            .Width = 1,
            .Height = 1,
            .Format = Format::RGBA8_SNORM,
            .Kind = ImageKind::Image2d,
            .Usage = ImageUsage::Sampled | ImageUsage::Source | ImageUsage::Destination};
        
        u32 miniFloatOne = 0b0'1111'111;
        u32 miniFloatOneHalf = 0b0'0111'111;
        u32 white   =   miniFloatOne        | miniFloatOne << 8     | miniFloatOne << 16    | miniFloatOne << 24;
        u32 black   =   0;
        u32 red     =   miniFloatOne                                                        | miniFloatOne << 24;
        u32 green   =                         miniFloatOne << 8                             | miniFloatOne << 24;
        u32 blue    =                                                 miniFloatOne << 16    | miniFloatOne << 24;
        u32 cyan    =                         miniFloatOne << 8     | miniFloatOne << 16    | miniFloatOne << 24;
        u32 magenta =   miniFloatOne                                | miniFloatOne << 16    | miniFloatOne << 24;
        u32 yellow  =   miniFloatOne        | miniFloatOne << 8                             | miniFloatOne << 24;

        u32 normal  =   miniFloatOneHalf    | miniFloatOneHalf << 8 | miniFloatOne << 16    | miniFloatOne << 24;
        
        s_DefaultImages[(u32)DefaultKind::White] = ImageData{
            .Image = Device::CreateImage({
                .DataSource = Span<const std::byte>({white}),
                .Description = description
            }),
            .Color = white
        };

        s_DefaultImages[(u32)DefaultKind::Black] = ImageData{
            .Image = Device::CreateImage({
                .DataSource = Span<const std::byte>({black}),
                .Description = description
            }),
            .Color = black
        };
        
        s_DefaultImages[(u32)DefaultKind::Red] = ImageData{
            .Image = Device::CreateImage({
                .DataSource = Span<const std::byte>({red}),
                .Description = description
            }),
            .Color = red
        };
        s_DefaultImages[(u32)DefaultKind::Green] = ImageData{
            .Image = Device::CreateImage({
                .DataSource = Span<const std::byte>({green}),
                .Description = description
            }),
            .Color = green
        };
        s_DefaultImages[(u32)DefaultKind::Blue] = ImageData{
            .Image = Device::CreateImage({
                .DataSource = Span<const std::byte>({blue}),
                .Description = description
            }),
            .Color = blue
        };

        s_DefaultImages[(u32)DefaultKind::Cyan] = ImageData{
            .Image = Device::CreateImage({
                .DataSource = Span<const std::byte>({cyan}),
                .Description = description
            }),
            .Color = cyan
        };
        s_DefaultImages[(u32)DefaultKind::Yellow] = ImageData{
            .Image = Device::CreateImage({
                .DataSource = Span<const std::byte>({yellow}),
                .Description = description
            }),
            .Color = yellow
        };
        s_DefaultImages[(u32)DefaultKind::Magenta] = ImageData{
            .Image = Device::CreateImage({
                .DataSource = Span<const std::byte>({magenta}),
                .Description = description
            }),
            .Color = magenta
        };
        s_DefaultImages[(u32)DefaultKind::NormalMap] = ImageData{
            .Image = Device::CreateImage({
                .DataSource = Span<const std::byte>({normal}),
                .Description = description
            }),
            .Color = normal
        };
    }

    Texture Default::Get(DefaultKind kind)
    {
        ASSERT((u32)kind < (u32)DefaultKind::MaxVal, "Incorrect texture type")

        return s_DefaultImages[(u32)kind].Image;
    }

    Texture Default::GetCopy(DefaultKind kind, DeletionQueue& deletionQueue)
    {
        ASSERT((u32)kind < (u32)DefaultKind::MaxVal, "Incorrect texture type")

        const auto& [textureOriginal, color] = s_DefaultImages[(u32)kind];
        
        Texture copy = Device::CreateImage({
            .DataSource = Span<const std::byte>({color}),
            .Description = Device::GetImageDescription(textureOriginal)},
            deletionQueue);

        return copy;
    }
}
