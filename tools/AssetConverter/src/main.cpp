#include <filesystem>
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

    AssetConverter::BakeDirectory(argv[1]);
}