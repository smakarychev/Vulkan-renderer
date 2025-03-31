#include "catch2/catch_test_macros.hpp"

#include "String/StringId.h"
#include "String/HashedStringView.h"

// NOLINTBEGIN

TEST_CASE("StringId constructor", "[String][StringId]")
{
    SECTION("Is constructable from HashedStringView")
    {
        StringId id = "Hello"_hsv;
        SUCCEED();
    }
    SECTION("Empty StringId has valid string value if StringIdRegistry is initialized")
    {
        StringIdRegistry::Init();
        StringId id = {};
        REQUIRE(!id.AsString().empty());
    }
    SECTION("Is copy constructable/assignable from other StringId")
    {
        StringId id = "Hello"_hsv;
        StringId copy = id;
        REQUIRE(copy.AsString() == id.AsString());

        StringId assigned = {};
        assigned = id;
        REQUIRE(assigned.AsString() == copy.AsString());
    }
    SECTION("Is move constructable/assignable from other StringId")
    {
        StringId id = "Hello"_hsv;
        StringId copy = std::move(id);
        REQUIRE(copy.AsString() == id.AsString());

        StringId assigned = {};
        assigned = std::move(id);
        REQUIRE(assigned.AsString() == copy.AsString());
    }
    SECTION("Can be created from std::string via static method")
    {
        std::string string = "Hello";
        StringId id = StringId::FromString(string);
        REQUIRE(id.AsString() == string);
    }
}
TEST_CASE("StringId can retrieve string", "[String][StringId]")
{
    std::string string = "Hello";
    StringId id = StringId::FromString(string);
    SECTION("As std::string")
    {
        REQUIRE(id.AsString() == string);
    }
    SECTION("As std::string_view")
    {
        REQUIRE(id.AsStringView() == string);
    }
}
TEST_CASE("StringId created from same strings are equal", "[String][StringId]")
{
    StringId one = "Hello"_hsv;
    StringId other = "Hello"_hsv;
    REQUIRE(one == other);
    REQUIRE(one.AsString() == other.AsString());
}
TEST_CASE("StringId created from different strings are not equal", "[String][StringId]")
{
    StringId one = "Hello"_hsv;
    StringId other = "World"_hsv;
    REQUIRE(one != other);
    REQUIRE(one.AsString() != other.AsString());
}

// NOLINTEND