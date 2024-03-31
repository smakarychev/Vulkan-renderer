#pragma once
#include <vector>

#include "Rendering/Image/Image.h"

class CommandBuffer;
class Image;

class EnvironmentPrefilterProcessor
{
public:
    static bool HasPending() { return !s_PendingTextures.empty(); }
    static void Add(const Image& source, const Image& prefiltered);
    static void Process(const CommandBuffer& cmd);
    static Texture CreateEmptyTexture();
private:
    static std::vector<std::pair<Image, Image>> s_PendingTextures;
};
