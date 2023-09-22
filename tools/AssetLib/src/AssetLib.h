#pragma once
#include <string>
#include <vector>

#include "types.h"

namespace assetLib
{
    enum class FileType : u32
    {
        Texture, Model, Mesh, Shader
    };
    
    struct File
    {
        FileType Type;
        u32 Version;
        std::string JSON;
        std::vector<u8> Blob;
    };

    enum class CompressionMode : u32
    {
        None,
        LZ4,
    };
    
    struct AssetInfoBase
    {
        CompressionMode CompressionMode;
        std::string OriginalFile;
    };
    

    bool saveBinaryFile(std::string_view path, const File& file);
    bool loadBinaryFile(std::string_view path, File& file);

    CompressionMode parseCompressionModeString(std::string_view modeString);
}

