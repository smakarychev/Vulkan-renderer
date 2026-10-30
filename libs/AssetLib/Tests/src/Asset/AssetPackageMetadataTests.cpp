#include "catch2/catch_test_macros.hpp"

#include <fstream>
#include <AssetLib/v3/Asset/AssetPackageMetadata.h>

// NOLINTBEGIN

using namespace lux::assetlib;

TEST_CASE("AssetPackageMetadata", "[Asset][AssetPackageMetadata]")
{
    SECTION("Can create")
    {
        AssetPackageMetadata2 metadata;
        SUCCEED();
    }
    SECTION("Can turn to string")
    {
        AssetPackageMetadata2 metadataSimple {
            .Version = 2,
            .PackageId = {123},
            .Locator = {.Name = "TestLocator", .Guid = lux::Guid::FromString("10e61810-b159-40a1-ad63-d45a114f66f0")},
            .Assets = {},
            .OriginalFile = "file" 
        };
        
        std::string simpleToString = metadataSimple.ToString();
        REQUIRE(
            simpleToString ==
            R"({"version":2,"packageId":123,"locator":{"Name":"TestLocator","Guid":"10e61810-b159-40a1-ad63-d45a114f66f0"},"assets":[],"originalFile":"file"})");
        
        std::string simpleToStringFormatted = metadataSimple.ToFormattedString();
        REQUIRE(simpleToStringFormatted == R"({
   "version": 2,
   "packageId": 123,
   "locator": {
      "Name": "TestLocator",
      "Guid": "10e61810-b159-40a1-ad63-d45a114f66f0"
   },
   "assets": [],
   "originalFile": "file"
})");
        
        std::chrono::system_clock::time_point derivationTime = 
            std::chrono::sys_days{2026y / std::chrono::October / 30};
        
        AssetPackageMetadata2 metadataWithAssets {
            .Version = 2,
            .PackageId = {123},
            .Locator = {.Name = "TestLocator", .Guid = lux::Guid::FromString("10e61810-b159-40a1-ad63-d45a114f66f0")},
            .Assets = {
                AssetPackageMetadata2::AssetMeta{
                    .AssetId = {456},
                    .AssetTypeInfo = {
                        .Name = "Material", .Guid = lux::Guid::FromString("2cc1893a-6470-4cb5-a57f-a4cbb93f4678")
                    },
                    .LastDerivationTime = derivationTime,
                    .ImporterInfo = {
                        .ImporterType = {
                            .Name = "MaterialImporter",
                            .Guid =lux::Guid::FromString("27c8d356-794e-453d-b237-5fd2a0fe559d")
                        }, 
                        .Info = "{\"metallic\": 1.0}"
                    }
                },
                AssetPackageMetadata2::AssetMeta{
                    .VariantName = "CoolVariant",
                    .AssetId = {789},
                    .AssetTypeInfo = {
                        .Name = "Scene", .Guid = lux::Guid::FromString("7e50ffb7-08c3-4ca5-87f5-a63f6ea6ed4c")
                    },
                    .LastDerivationTime = derivationTime - std::chrono::days(1),
                    .ImporterInfo = {
                        .ImporterType = {
                            .Name = "SceneImporter",
                            .Guid =lux::Guid::FromString("19f75ad8-e8f5-4825-a349-aed2e8289a46")
                        }, 
                    }
                },
            },
            .OriginalFile = "file" 
        };
        
        std::string withAssetsToFormattedString = metadataWithAssets.ToFormattedString();
        REQUIRE(
            withAssetsToFormattedString ==
            R"({
   "version": 2,
   "packageId": 123,
   "locator": {
      "Name": "TestLocator",
      "Guid": "10e61810-b159-40a1-ad63-d45a114f66f0"
   },
   "assets": [
      {
         "assetId": 456,
         "assetTypeInfo": {
            "Name": "Material",
            "Guid": "2cc1893a-6470-4cb5-a57f-a4cbb93f4678"
         },
         "lastDerivationTime": "2026-10-30T00:00:00.000000000Z",
         "importerInfo": {
            "importerType": {
               "Name": "MaterialImporter",
               "Guid": "27c8d356-794e-453d-b237-5fd2a0fe559d"
            },
            "info": {
               "metallic": 1.0
            }
         }
      },
      {
         "variantName": "CoolVariant",
         "assetId": 789,
         "assetTypeInfo": {
            "Name": "Scene",
            "Guid": "7e50ffb7-08c3-4ca5-87f5-a63f6ea6ed4c"
         },
         "lastDerivationTime": "2026-10-29T00:00:00.000000000Z",
         "importerInfo": {
            "importerType": {
               "Name": "SceneImporter",
               "Guid": "19f75ad8-e8f5-4825-a349-aed2e8289a46"
            },
            "info": {}
         }
      }
   ],
   "originalFile": "file"
})");
    }
    SECTION("Can not create from invalid string")
    {
        auto meta = AssetPackageMetadata2::FromString("");
        REQUIRE(!meta);
        meta = AssetPackageMetadata2::FromString(R"({
   "version": 2,
   "packageId": "this_should_be_number_not_a_string",
   "locator": {
      "Name": "TestLocator",
      "Guid": "10e61810-b159-40a1-ad63-d45a114f66f0"
   },
   "assets": [],
   "originalFile": "file"
})");
        REQUIRE(!meta);
    }
    SECTION("Can create from string")
    {
        auto meta = AssetPackageMetadata2::FromString(R"({
   "version": 2,
   "packageId": 123,
   "locator": {
      "Name": "TestLocator",
      "Guid": "10e61810-b159-40a1-ad63-d45a114f66f0"
   },
   "assets": [],
   "originalFile": "file"
})");
        REQUIRE(meta);
        REQUIRE(meta->Version == 2);
        REQUIRE(meta->PackageId == 123);
        REQUIRE(meta->Locator.Guid == lux::Guid::FromString("10e61810-b159-40a1-ad63-d45a114f66f0"));
        REQUIRE(meta->Locator.Name == "TestLocator");
        REQUIRE(meta->Assets.empty());
        REQUIRE(meta->OriginalFile == "file");
        
        meta = AssetPackageMetadata2::FromString(            R"({
   "version": 2,
   "packageId": 123,
   "locator": {
      "Name": "TestLocator",
      "Guid": "10e61810-b159-40a1-ad63-d45a114f66f0"
   },
   "assets": [
      {
         "assetId": 456,
         "assetTypeInfo": {
            "Name": "Material",
            "Guid": "2cc1893a-6470-4cb5-a57f-a4cbb93f4678"
         },
         "lastDerivationTime": "2026-10-30T00:00:00.000000000Z",
         "importerInfo": {
            "importerType": {
               "Name": "MaterialImporter",
               "Guid": "27c8d356-794e-453d-b237-5fd2a0fe559d"
            },
            "info": {
               "metallic": 1.0
            }
         }
      },
      {
         "variantName": "CoolVariant",
         "assetId": 789,
         "assetTypeInfo": {
            "Name": "Scene",
            "Guid": "7e50ffb7-08c3-4ca5-87f5-a63f6ea6ed4c"
         },
         "lastDerivationTime": "2026-10-29T00:00:00.000000000Z",
         "importerInfo": {
            "importerType": {
               "Name": "SceneImporter",
               "Guid": "19f75ad8-e8f5-4825-a349-aed2e8289a46"
            },
            "info": {}
         }
      }
   ],
   "originalFile": "file"
})");
        REQUIRE(meta);
        REQUIRE(meta->Version == 2);
        REQUIRE(meta->PackageId == 123);
        REQUIRE(meta->Locator.Guid == lux::Guid::FromString("10e61810-b159-40a1-ad63-d45a114f66f0"));
        REQUIRE(meta->Locator.Name == "TestLocator");
        REQUIRE(meta->Assets.size() == 2);
        REQUIRE(meta->OriginalFile == "file");
        REQUIRE(!meta->Assets[0].VariantName);
        REQUIRE(meta->Assets[0].AssetId == 456);
        REQUIRE(meta->Assets[0].AssetTypeInfo.Guid == lux::Guid::FromString("2cc1893a-6470-4cb5-a57f-a4cbb93f4678"));
        REQUIRE(meta->Assets[0].AssetTypeInfo.Name == "Material");
        REQUIRE(meta->Assets[0].LastDerivationTime == std::chrono::sys_days{2026y / std::chrono::October / 30});
        REQUIRE(
            meta->Assets[0].ImporterInfo.ImporterType.Guid == lux::Guid::FromString(
                "27c8d356-794e-453d-b237-5fd2a0fe559d"));
        REQUIRE(meta->Assets[0].ImporterInfo.ImporterType.Name == "MaterialImporter");
        REQUIRE(meta->Assets[0].ImporterInfo.Info == R"({
               "metallic": 1.0
            })");
        
        REQUIRE(meta->Assets[1].VariantName == "CoolVariant");
        REQUIRE(meta->Assets[1].AssetId == 789);
        REQUIRE(meta->Assets[1].AssetTypeInfo.Guid == lux::Guid::FromString("7e50ffb7-08c3-4ca5-87f5-a63f6ea6ed4c"));
        REQUIRE(meta->Assets[1].AssetTypeInfo.Name == "Scene");
        REQUIRE(meta->Assets[1].LastDerivationTime == std::chrono::sys_days{2026y / std::chrono::October / 29});
        REQUIRE(
            meta->Assets[1].ImporterInfo.ImporterType.Guid == lux::Guid::FromString(
                "19f75ad8-e8f5-4825-a349-aed2e8289a46"));
        REQUIRE(meta->Assets[1].ImporterInfo.ImporterType.Name == "SceneImporter");
        REQUIRE(meta->Assets[1].ImporterInfo.Info == "{}");
    }
}

// NOLINTEND