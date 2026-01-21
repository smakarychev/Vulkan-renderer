#include "catch2/catch_test_macros.hpp"

#include "Containers/Guid.h"

#include <unordered_map>
#include <unordered_set>

// NOLINTBEGIN

TEST_CASE("Guid", "[Containers][Guid]")
{
    SECTION("Can create from string literal at compiletime")
    {
        constexpr Guid guid("76b96176-0b26-476f-a049-fcc20e9bbf11");
        constexpr Guid guid2 = "76b96176-0b26-476f-a049-fcc20e9bbf11"_guid;
        SUCCEED();
    }
    SECTION("Can create from non constexpr string")
    {
        std::string guidString = "76b96176-0b26-476f-a049-fcc20e9bbf11";
        Guid guid = Guid::FromString(guidString);
        SUCCEED();
    }
    SECTION("Can cast to string")
    {
        std::string guidString = "76b96176-0b26-476f-a049-fcc20e9bbf11";
        Guid guid = Guid::FromString(guidString);
        REQUIRE(guid.AsString() == guidString);
    }
    SECTION("Invalid guids are transfomed to bad")
    {
        std::string guidString = "76w96176-0w26-476j-u049-fcc20e9bbf11";
        Guid guid = Guid::FromString(guidString);
        REQUIRE(guid.AsString() == "badbadba-dbad-badb-adba-dbadbadbadba");

        guid = Guid::FromString("short");
        REQUIRE(guid.AsString() == "badbadba-dbad-badb-adba-dbadbadbadba");
    }
    SECTION("Can std::format")
    {
        std::string guidString = "76b96176-0b26-476f-a049-fcc20e9bbf11";
        Guid guid = Guid::FromString(guidString);
        REQUIRE(std::format("{}", guid) == guidString);
    }
    SECTION("Can create from uppercase")
    {
        std::string guidString = "76B96176-0B26-476F-A049-FCC20E9BBF11";
        Guid guid = Guid::FromString(guidString);
        REQUIRE(guid.AsString() == "76b96176-0b26-476f-a049-fcc20e9bbf11");
    }
    SECTION("Can create without dashes")
    {
        std::string guidString = "76b961760b26476fa049fcc20e9bbf11";
        Guid guid = Guid::FromString(guidString);
        REQUIRE(guid.AsString() == "76b96176-0b26-476f-a049-fcc20e9bbf11");
    }
    SECTION("Can use in an std::unordered_map")
    {
        std::unordered_map<Guid, int> map;
        std::unordered_set<Guid> set;

        map["76b96176-0b26-476f-a049-fcc20e9bbf11"_guid] = 1;
        set.emplace("76b96176-0b26-476f-a049-fcc20e9bbf11"_guid);
        REQUIRE(map.size() == 1);
        REQUIRE(set.size() == 1);
    }
}

// NOLINTEND