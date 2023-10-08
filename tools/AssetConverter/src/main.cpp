#include <filesystem>
#include <iostream>
#include <format>

#include "ConverterDispatcher.h"
#include "Converters.h"
#include "types.h"

i32 main(i32 argc, char** argv)
{
    namespace fs = std::filesystem;
    
    if (argc < 2)
    {
        std::cout << "Usage: AssetConverter <directory>\n";
        exit(1);
    }

    fs::path directory{argv[1]};

    for (const auto& file : fs::recursive_directory_iterator(directory))
    {
        if (!file.is_directory())
        {
            ConverterDispatcher dispatcher(file);
            dispatcher.Dispatch<TextureConverter>({".png", ".jpg", ".jpeg"});
            dispatcher.Dispatch<ModelConverter>({".obj", ".fbx", ".blend", ".gltf"});
            dispatcher.Dispatch<ShaderConverter>({".vert", ".frag", ".compute"});
        }
    }
}

// SPIV-reflect implementation
#include "spirv_reflect.c"