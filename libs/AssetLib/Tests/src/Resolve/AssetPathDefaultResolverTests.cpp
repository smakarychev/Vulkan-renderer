#include "catch2/catch_test_macros.hpp"

#include <fstream>
#include <AssetLib/v3/Resolve/AssetPathDefaultResolver.h>

// NOLINTBEGIN

using namespace lux::assetlib;

TEST_CASE("AssetPathDefaultResolver", "[Resolve][AssetPathResolver]")
{
    namespace fs = std::filesystem;
    
    AssetPathDefaultResolver defaultResolver;
    fs::path testDir = fs::temp_directory_path();
    testDir /= "AssetLibTests";
    fs::create_directories(testDir);
    
    SECTION("Can create")
    {
        AssetPathDefaultResolver testResolver;
        SUCCEED();
    }
    SECTION("Has RTTI")
    {
        REQUIRE(lux::rttiStaticType<AssetPathDefaultResolver>().IsA<IAssetPathResolver>());
        REQUIRE(lux::rttiStaticType<AssetPathDefaultResolver>().IsExactlyA<AssetPathDefaultResolver>());
    }
    SECTION("Empty path is not resolved")
    {
        REQUIRE(!defaultResolver.Resolve(""));
        REQUIRE(!defaultResolver.Resolve("./"));
        REQUIRE(!defaultResolver.Resolve("/"));
    }
    SECTION("Can set search path")
    {
        REQUIRE(!defaultResolver.Resolve("Relative.Asset"));
        defaultResolver.AddSearchPath(testDir);
        {
            std::ofstream(testDir / "Relative.meta");
        }
        REQUIRE(defaultResolver.Resolve("Relative.Asset"));
    }
    SECTION("Can resolve absolute path if search path is relative to it")
    {
        REQUIRE(!defaultResolver.Resolve((testDir / "Relative.Asset").string()));
        defaultResolver.AddSearchPath(testDir);
        {
            std::ofstream(testDir / "Relative.meta");
        }
        REQUIRE(defaultResolver.Resolve((testDir / "Relative.Asset").string()));
    }
    SECTION("Can set multiple search paths")
    {
        defaultResolver.AddSearchPath(testDir);
        defaultResolver.AddSearchPath(testDir / "Materials");
        {
            std::ofstream(testDir / "Relative.meta");
            fs::create_directories(testDir / "Materials");
            std::ofstream(testDir / "Materials/Material.meta");
        }
        REQUIRE(defaultResolver.Resolve("Relative.Asset"));
        REQUIRE(defaultResolver.Resolve("Material.Asset"));
        REQUIRE(defaultResolver.Resolve("Materials/Material.Asset"));
        REQUIRE(
            defaultResolver.Resolve("Materials/Material.Asset")->GetPackagePath() == defaultResolver.Resolve(
                "Material.Asset")->GetPackagePath());
    }
    SECTION("Can set protocols")
    {
        defaultResolver.AddProtocolResolution("mat", testDir / "Materials");
        {
            fs::create_directories(testDir / "Materials");
            std::ofstream(testDir / "Materials/Material.meta");
        }
        REQUIRE(!defaultResolver.Resolve("mat://MaterialDoesNotExist.Asset"));
        REQUIRE(defaultResolver.Resolve("mat://Material.Asset"));
    }
    SECTION("Resolves path correctly")
    {
        defaultResolver.AddProtocolResolution("mat", testDir / "Materials");
        defaultResolver.AddSearchPath(testDir);
        {
            fs::create_directories(testDir / "Materials");
            fs::create_directories(testDir / "Scenes/Forest");
            std::ofstream(testDir / "Materials/Material.meta");
            std::ofstream(testDir / "Scenes/Forest/Forest.meta");
        }

        auto materialPath = defaultResolver.Resolve("mat://Material.Asset");
        auto scenePath = defaultResolver.Resolve("Scenes/Forest/Forest.MyForest{Evergreen}");
        REQUIRE(materialPath);
        REQUIRE(scenePath);
        REQUIRE(
            fs::path(fs::weakly_canonical(materialPath->GetPackagePath())).generic_string() == (testDir /
                "Materials/Material.meta").generic_string());
        REQUIRE(materialPath->GetAssetName() == "Asset");
        REQUIRE(!materialPath->GetVariant().has_value());

        REQUIRE(
            fs::path(fs::weakly_canonical(scenePath->GetPackagePath())).generic_string() == (testDir /
                "Scenes/Forest/Forest.meta").generic_string());
        REQUIRE(scenePath->GetAssetName() == "MyForest");
        REQUIRE(scenePath->GetVariant().has_value());
        REQUIRE(*scenePath->GetVariant() == "Evergreen");
    }
    SECTION("Can invalidate cache")
    {
        defaultResolver.AddProtocolResolution("mat", testDir / "MaterialsDir");
        {
            fs::create_directories(testDir / "MaterialsDir");
            std::ofstream(testDir / "MaterialsDir/Material.meta");
        }
        REQUIRE(defaultResolver.Resolve("mat://Material.Asset"));
        REQUIRE(defaultResolver.Resolve("mat://Material.Asset")->GetPackagePath().contains("materialsdir/"));
        
        
        defaultResolver.AddProtocolResolution("mat", testDir / "MaterialsDir2");
        {
            fs::create_directories(testDir / "MaterialsDir2");
            std::ofstream(testDir / "MaterialsDir2/Material.meta");
            std::ofstream(testDir / "MaterialsDir2/Material2.meta");
        }
        REQUIRE(defaultResolver.Resolve("mat://Material.Asset")->GetPackagePath().contains("materialsdir/"));
        REQUIRE(!defaultResolver.Resolve("mat://Material.Asset")->GetPackagePath().contains("materialsdir2/"));
        REQUIRE(defaultResolver.Resolve("mat://Material2.Asset")->GetPackagePath().contains("materialsdir2/"));
        
        defaultResolver.InvalidateCache();
        REQUIRE(defaultResolver.Resolve("mat://Material.Asset")->GetPackagePath().contains("materialsdir2/"));
    }
    SECTION("Can invalidate cache for resolved path")
    {
        defaultResolver.AddProtocolResolution("mat", testDir / "MaterialsDir");
        {
            fs::create_directories(testDir / "MaterialsDir");
            std::ofstream(testDir / "MaterialsDir/Material.meta");
            std::ofstream(testDir / "MaterialsDir/Material2.meta");
        }
        REQUIRE(defaultResolver.Resolve("mat://Material.Asset"));
        REQUIRE(defaultResolver.Resolve("mat://Material.Asset")->GetPackagePath().contains("materialsdir/"));
        REQUIRE(defaultResolver.Resolve("mat://Material2.Asset")->GetPackagePath().contains("materialsdir/"));
        
        defaultResolver.AddProtocolResolution("mat", testDir / "MaterialsDir2");
        {
            fs::create_directories(testDir / "MaterialsDir2");
            std::ofstream(testDir / "MaterialsDir2/Material.meta");
            std::ofstream(testDir / "MaterialsDir2/Material3.meta");
        }
        REQUIRE(defaultResolver.Resolve("mat://Material.Asset")->GetPackagePath().contains("materialsdir/"));
        REQUIRE(!defaultResolver.Resolve("mat://Material.Asset")->GetPackagePath().contains("materialsdir2/"));
        REQUIRE(!defaultResolver.Resolve("mat://Material2.Asset")->GetPackagePath().contains("materialsdir2/"));
        REQUIRE(defaultResolver.Resolve("mat://Material3.Asset")->GetPackagePath().contains("materialsdir2/"));
        
        defaultResolver.InvalidateCache(*defaultResolver.Resolve("mat://Material.Asset"));
        REQUIRE(defaultResolver.Resolve("mat://Material.Asset")->GetPackagePath().contains("materialsdir2/"));
        REQUIRE(!defaultResolver.Resolve("mat://Material2.Asset")->GetPackagePath().contains("materialsdir2/"));
    }

    fs::remove_all(testDir);
}

// NOLINTEND