#pragma once
#include <filesystem>

#include "ConverterDispatcher.h"
#include "Converters.h"

class AssetConverter
{
public:
    static void BakeDirectory(const std::filesystem::path& path)
    {
        namespace fs = std::filesystem;

        for (const auto& file : fs::recursive_directory_iterator(path))
        {
            if (!file.is_directory())
            {
                ConverterDispatcher dispatcher(path, file);
                dispatcher.Dispatch<SceneConverter>(SceneConverter::GetWatchedExtensions());
                dispatcher.Dispatch<TextureConverter>(TextureConverter::GetWatchedExtensions());
            }
        }
    }
};

