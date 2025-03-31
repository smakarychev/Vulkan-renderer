#include "catch2/catch_test_macros.hpp"

#include "String/HashedStringView.h"
#include "String/StringUnorderedMap.h"

// NOLINTBEGIN

TEST_CASE("HashedStringView", "[String]")
{
    std::string_view testString = "Hello world";
    HashedStringView view{testString};
    u64 hash = Hash::string(testString);
    
    SECTION("Hash is correct")
    {
        REQUIRE(view.Hash() == hash);
    }
    SECTION("String view is correct")
    {
        REQUIRE(view.String() == testString);
    }
    SECTION("Is constructable from literal expression")
    {
        HashedStringView another = "Test"_hsv;
        REQUIRE(another.String() == "Test");
        REQUIRE(another.Hash() == Hash::string("Test"));
    }
}
TEST_CASE("StringUnorderedMap", "[String]")
{
    StringUnorderedMap<i32> map;
    
    SECTION("Accepts const char* as a key lookup")
    {
        const char* key = "Key";
        map[key] = 1;
        REQUIRE(map.contains(key));
        REQUIRE(map.find(key)->second == 1);
    }
    SECTION("Accepts const std::string& as a key lookup")
    {
        std::string key = "Key";
        map[key] = 1;
        REQUIRE(map.contains(key));
        REQUIRE(map.find(key)->second == 1);
    }
    SECTION("Accepts std::string_view as a key lookup")
    {
        std::string_view key = "Key";
        map[std::string{key}] = 1;
        REQUIRE(map.contains(key));
        REQUIRE(map.find(key)->second == 1);
    }
}

// NOLINTEND
