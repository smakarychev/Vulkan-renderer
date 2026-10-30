#include "catch2/catch_test_macros.hpp"

#include <fstream>
#include <AssetLib/v3/Resolve/AssetMetadataDefaultDatabase.h>

// NOLINTBEGIN

using namespace lux::assetlib;

TEST_CASE("AssetMetadataDefaultDatabase", "[Resolve][AssetMetadataDatabase]")
{
    namespace fs = std::filesystem;

    class DummyResolver final : public IAssetPathResolver
    {
    public:
        const AssetSystemIdentity& GetIdentity() const override { return {}; }
        std::optional<AssetResolvedPath> Resolve(const std::string& path) const override
        {
            return AssetResolvedPath(path, "", "");
        }
        void InvalidateCache() override {}
        void InvalidateCache(const AssetResolvedPath& path) override {}
    };
    auto pathResolver = std::make_shared<DummyResolver>();
    
    fs::path testDir = fs::temp_directory_path();
    testDir /= "AssetLibTests/DatabaseTests";
    fs::create_directories(testDir);
    fs::path firstMetadataPath = testDir / "first.meta";
    fs::path secondMetadataPath = testDir / "second.meta";
    {
        AssetPackageMetadata2 metadata = {
            .Version = 1,
            .PackageId = 123,
            .Locator = {},
        };
        std::ofstream out(firstMetadataPath);
        out << metadata.ToFormattedString();
    }
    {
        AssetPackageMetadata2 metadata = {
            .Version = 1,
            .PackageId = 456,
            .Locator = {},
        };
        std::ofstream out(secondMetadataPath);
        out << metadata.ToFormattedString();
    }
    
    SECTION("Can create")
    {
        AssetMetadataDefaultDatabase testDatabase(pathResolver);
        SUCCEED();
    }
    SECTION("Can get scanned metadata")
    {
        AssetMetadataDefaultDatabase testDatabase(pathResolver);
        auto resolved = pathResolver->Resolve(firstMetadataPath.generic_string()).value_or({});
        REQUIRE(!testDatabase.Find(resolved));
        REQUIRE(!testDatabase.Find(123));
        
        testDatabase.ScanDirectory(testDir);
        REQUIRE(testDatabase.Find(resolved));
        REQUIRE(testDatabase.Find(123));
    }
    SECTION("Metadata is cached")
    {
        AssetMetadataDefaultDatabase testDatabase(pathResolver);
        testDatabase.ScanDirectory(testDir);
        auto resolved = pathResolver->Resolve(firstMetadataPath.generic_string()).value_or({});
        REQUIRE(testDatabase.Find(resolved));
        REQUIRE(testDatabase.Find(123));
        
        {
            AssetPackageMetadata2 metadata = {
                .Version = 2,
                .PackageId = 456,
                .Locator = {},
            };
            std::ofstream out(firstMetadataPath);
            out << metadata.ToFormattedString();
        }
        auto found = testDatabase.Find(resolved);
        REQUIRE(found);
        REQUIRE(found->Version == 1);
        REQUIRE(found->PackageId == 123);
        
        found = testDatabase.Find(123);
        REQUIRE(found);
        REQUIRE(found->Version == 1);
        REQUIRE(found->PackageId == 123);
    }
    SECTION("Metadata cache can be updated")
    {
        AssetMetadataDefaultDatabase testDatabase(pathResolver);
        testDatabase.ScanDirectory(testDir);
        auto resolved = pathResolver->Resolve(firstMetadataPath.generic_string()).value_or({});
        REQUIRE(testDatabase.Find(resolved));
        REQUIRE(testDatabase.Find(123));
        
        {
            AssetPackageMetadata2 metadata = {
                .Version = 2,
                .PackageId = 4567,
                .Locator = {},
            };
            std::ofstream out(firstMetadataPath);
            out << metadata.ToFormattedString();
        }
        {
            AssetPackageMetadata2 metadata = {
                .Version = 3,
                .PackageId = 789,
                .Locator = {},
            };
            std::ofstream out(secondMetadataPath);
            out << metadata.ToFormattedString();
        }
        
        testDatabase.UpdateMetadata(firstMetadataPath.generic_string());
        auto found = testDatabase.Find(resolved);
        REQUIRE(found);
        REQUIRE(found->Version == 2);
        REQUIRE(found->PackageId == 4567);
        
        found = testDatabase.Find(123);
        REQUIRE(!found);
        
        resolved = pathResolver->Resolve(secondMetadataPath.generic_string()).value_or({});
        found = testDatabase.Find(resolved);
        REQUIRE(found);
        REQUIRE(found->Version == 1);
        REQUIRE(found->PackageId == 456);
        
        testDatabase.UpdateMetadata(secondMetadataPath.generic_string());
        found = testDatabase.Find(resolved);
        REQUIRE(found);
        REQUIRE(found->Version == 3);
        REQUIRE(found->PackageId == 789);
    }
    fs::remove_all(testDir);
}

// NOLINTEND