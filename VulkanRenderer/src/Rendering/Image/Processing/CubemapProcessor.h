#pragma once
#include <string>
#include <unordered_map>

class Image;
class CommandBuffer;

class CubemapProcessor
{
public:
    static bool HasPending() { return !s_PendingTextures.empty(); }
    static void Add(const std::string& path, const Image& image);
    static void Process(const CommandBuffer& cmd);
private:
    static std::unordered_map<std::string, Image> s_PendingTextures;
};
