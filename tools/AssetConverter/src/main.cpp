﻿#include <filesystem>
#include <iostream>

#include "AssetConverter.h"

i32 main(i32 argc, char** argv)
{
    namespace fs = std::filesystem;
    if (argc < 2)
    {
        std::cout << "Usage: AssetConverter <directory>\n";
        return 1;
    }
    //SceneConverter::Convert("E:/Repos/C++/Gamedev/vulkan-renderer/assets", "E:/Repos/C++/Gamedev/vulkan-renderer/assets/models/medieval fantasy book/scene.gltf");
    AssetConverter::BakeDirectory(std::filesystem::weakly_canonical(argv[1]));
}