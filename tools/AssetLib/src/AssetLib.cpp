#include "AssetLib.h"

#include <fstream>

namespace assetLib
{
    bool saveBinaryFile(std::string_view path, const File& file)
    {
        std::ofstream out(path.data(), std::ios::binary | std::ios::out);

        out.write((const char*)&file.Type, sizeof file.Type);

        out.write((const char*)&file.Version, sizeof file.Version);

        isize jsonSize = (isize)file.JSON.size();
        isize blobSize = (isize)file.Blob.size();
        out.write((const char*)&jsonSize, sizeof jsonSize);
        out.write((const char*)&blobSize, sizeof blobSize);

        out.write(file.JSON.data(), jsonSize);
        out.write((const char*)file.Blob.data(), blobSize);

        return true;
    }

    bool loadBinaryFile(std::string_view path, File& file)
    {
        std::ifstream in(path.data(), std::ios::binary);
        if (!in.good())
            return false;
        in.seekg(0);

        in.read((char*)&file.Type, sizeof file.Type);

        in.read((char*)&file.Version, sizeof file.Version);

        isize jsonSize, blobSize;
        in.read((char*)&jsonSize, sizeof jsonSize);
        in.read((char*)&blobSize, sizeof blobSize);

        file.JSON.resize(jsonSize);
        file.Blob.resize(blobSize);
        in.read((char*)file.JSON.data(), jsonSize);
        in.read((char*)file.Blob.data(), blobSize);

        return true;
    }

    CompressionMode parseCompressionModeString(std::string_view modeString)
    {
        if (modeString == "LZ4")
            return CompressionMode::LZ4;
        if (modeString == "None")
            return CompressionMode::None;
        
        std::unreachable();
    }
}
