#include "catch2/catch_test_macros.hpp"

#include "Containers/Span.h"

// NOLINTBEGIN

TEST_CASE("Span", "[Containers][Span]")
{
    SECTION("Can create from initializer list")
    {
        u32 a = 1, b = 1;
        Span<const u32> span{a, b};
        REQUIRE(span.size() == 2);
    }
    SECTION("Can create from std::span")
    {
        std::array array{1, 2};
        std::span span(array);
        Span<const i32> myspan(span);
        REQUIRE(myspan.size() == 2);
    }
    SECTION("Can create from std::array")
    {
        std::array array{1, 2};
        Span<const i32> myspan(array);
        REQUIRE(myspan.size() == 2);
    }
    SECTION("Can create from std::vector")
    {
        std::vector vector{1, 2};
        Span<const i32> myspan(vector);
        REQUIRE(myspan.size() == 2);
    }
    SECTION("Can create from Span")
    {
        std::vector vector{1, 2};
        Span<const i32> myspan(vector);
        Span<const i32> myspan2(myspan);
        REQUIRE(myspan2.size() == 2);
    }
    SECTION("Can cast anything to Span of const bytes")
    {
        u32 value = 1;
        std::vector<i32> vector{1, 2, 3};
        std::array array{1.0, 3.0, 2.0};
        Span<const i32> myspan(vector);

        {
            Span<const std::byte> bytesSpan = {value};
            REQUIRE(bytesSpan.size() == 4);
        }
        {
            Span<const std::byte> bytesSpan = vector;
            REQUIRE(bytesSpan.size() == 4 * 3);
        }
        {
            Span<const std::byte> bytesSpan = array;
            REQUIRE(bytesSpan.size() == 8 * 3);
        }
        {
            Span<const std::byte> bytesSpan = myspan;
            REQUIRE(bytesSpan.size() == 4 * 3);
        }
    }
}

// NOLINTEND