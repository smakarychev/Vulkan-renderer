#include "catch2/catch_test_macros.hpp"

#include "types.h"
#include "Utils/ContainterUtils.h"

#include <unordered_set>

// NOLINTBEGIN

TEST_CASE("checkArrayContainsSubArray", "[Utils][ContainerUtils]")
{
    std::vector<i32> required = {1, 2};

    SECTION("returns true if subarray has 0 size")
    {
        REQUIRE(Utils::checkArrayContainsSubArray(
            std::vector<i32>{}, std::vector<i32>{1}, std::equal_to<>{}, [](i32){}));
    }
    SECTION("returns true if subarray is present in array")
    {
        REQUIRE(Utils::checkArrayContainsSubArray(
            std::vector<i32>{1, 2}, std::vector<i32>{1, 2, 3, 4}, std::equal_to<>{}, [](i32){}));
        REQUIRE(Utils::checkArrayContainsSubArray(
            std::vector{"Hello"}, std::vector{"Hello", "World"},
            std::equal_to<std::string>{}, [](const std::string&){}));
    }
    SECTION("returns false if subarray is not present in array")
    {
        REQUIRE_FALSE(Utils::checkArrayContainsSubArray(
            std::vector<i32>{5}, std::vector<i32>{1, 2, 3, 4}, std::equal_to<>{}, [](i32){}));
        REQUIRE_FALSE(Utils::checkArrayContainsSubArray(
            std::vector{"Hello", "World"}, std::vector{"Hello"},
            std::equal_to<std::string>{}, [](const std::string&){}));
    }
    SECTION("logs missing required elements")
    {
        std::unordered_set<std::string> missing;
        Utils::checkArrayContainsSubArray(
            std::vector{"Hello", "World", "!"}, std::vector{"Hello"},
            std::equal_to<std::string>{}, [&missing](const std::string& val)
            {
                missing.emplace(val);
            });
        REQUIRE(missing.size() == 2);
        REQUIRE(missing.contains("World"));
        REQUIRE(missing.contains("!"));
    }
    SECTION("supports different types via custom comparator")
    {
        REQUIRE(Utils::checkArrayContainsSubArray(
           std::vector<std::string>{"1"}, std::vector<i32>{1, 2, 3, 4},
           [](const std::string& req, i32 avail) { return std::to_string(avail) == req; },
           [](const std::string&){}));
    }
}

// NOLINTEND