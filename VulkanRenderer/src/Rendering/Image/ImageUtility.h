#pragma once

#include "Image.h"

namespace Images
{
    u32 toRGBA8(const glm::vec4& color);
    u32 toRGBA8SNorm(const glm::vec4& color);

    i8 mipmapCount(const glm::uvec2& resolution);
    i8 mipmapCount(const glm::uvec3& resolution);
    glm::uvec3 getPixelCoordinates(Image image, const glm::vec3& coordinate, ImageSizeType sizeType);
    
    enum class DefaultKind
    {
        White = 0, Black, Red, Green, Blue, Cyan, Yellow, Magenta,
        NormalMap,
        MaxVal
    };
    class Default
    {
    public:
        static void Init();
        static Image Get(DefaultKind kind);
        static Image GetCopy(DefaultKind texture, DeletionQueue& deletionQueue);
    private:
        struct ImageData
        {
            Image Image;
            u32 Color;
        };
        static std::array<ImageData, (u32)DefaultKind::MaxVal> s_DefaultImages;
    };
}
