#include "catch2/catch_test_macros.hpp"
#include "catch2/generators/catch_generators.hpp"
#include "catch2/generators/catch_generators_adapters.hpp"
#include "catch2/generators/catch_generators_random.hpp"
#include "catch2/matchers/catch_matchers.hpp"
#include "catch2/matchers/catch_matchers_floating_point.hpp"

#include "Math/CoreMath.h"

// NOLINTBEGIN

TEST_CASE("floorToPowerOf2", "[CoreMath]")
{
    SECTION("Floor of 0 is 0")
    {
        REQUIRE(Math::floorToPowerOf2(0) == 0);
    }
    SECTION("Floor of power of 2 is unchanged")
    {
        for (u32 i = 0; i < 32; i++)
        {
            const u32 val = 1lu << i;
            REQUIRE(Math::floorToPowerOf2(val) == val);
        }
    }
    SECTION("Floor of not power of 2 gets rounded")
    {
        REQUIRE(Math::floorToPowerOf2(3) == 2);
        REQUIRE(Math::floorToPowerOf2(17) == 16);
        REQUIRE(Math::floorToPowerOf2(1500) == 1024);
        REQUIRE(Math::floorToPowerOf2(2'000'000'000) == 1'073'741'824);
    }
}
TEST_CASE("isPowerOf2", "[CoreMath]")
{
    SECTION("Is true for 0")
    {
        REQUIRE(Math::isPowerOf2(0));
    }
    SECTION("Is true for powers of 2")
    {
        for (u32 i = 0; i < 32; i++)
            REQUIRE(Math::isPowerOf2(1lu << i));
        for (u32 i = 0; i < 64; i++)
            REQUIRE(Math::isPowerOf2(1llu << i));
    }
    SECTION("Is false for not powers of 2")
    {
        REQUIRE_FALSE(Math::isPowerOf2(3));
        REQUIRE_FALSE(Math::isPowerOf2(17));
        REQUIRE_FALSE(Math::isPowerOf2(1500));
        REQUIRE_FALSE(Math::isPowerOf2(2'000'000'000));
    }
}
TEST_CASE("log2", "[CoreMath]")
{
    SECTION("Log2 of 0 is u32::max")
    {
        REQUIRE(Math::log2(0) == ~0lu);
    }
    SECTION("Log2 of powers of 2 is correct")
    {
        for (u32 i = 0; i < 32; i++)
            REQUIRE(Math::log2(1lu << i) == i);
    }
    SECTION("Log2 of not powers of 2 is correct")
    {
        REQUIRE(Math::log2(3) == (u32)std::log2(3.0));
        REQUIRE(Math::log2(17) == (u32)std::log2(17.0));
        REQUIRE(Math::log2(1500) == (u32)std::log2(1500.0));
        REQUIRE(Math::log2(2'000'000'000) == (u32)std::log2(2'000'000'000.0));
    }
}
TEST_CASE("lerp", "[CoreMath]")
{
    SECTION("lerp with t = 0 is 'a'")
    {
        REQUIRE(Math::lerp(-1.0, 1.0, 0.0) == -1.0);
    }
    SECTION("lerp with t = 1 is 'b'")
    {
        REQUIRE(Math::lerp(-1.0, 1.0, 1.0) == 1.0);
    }
    SECTION("lerp with t = 0.5 is midpoint")
    {
        REQUIRE(Math::lerp(-1.0, 1.0, 0.5) == 0.0);
    }
    SECTION("lerp is inverse of ilerp")
    {
        const f32 a = -100.0f;
        const f32 b = 100.0f;
        const f32 t = GENERATE_COPY(take(100, random(a, b)));
        REQUIRE_THAT(Math::lerp(a, b, Math::ilerp(a, b, t)), Catch::Matchers::WithinRel(t, 1e-3f));
    }
}
TEST_CASE("ilerp", "[CoreMath]")
{
    SECTION("ilerp with t = a is 0")
    {
        REQUIRE(Math::ilerp(-1.0, 1.0, -1.0) == 0.0);
    }
    SECTION("ilerp with t = b is 1")
    {
        REQUIRE(Math::ilerp(-1.0, 1.0, 1.0) == 1.0);
    }
    SECTION("ilerp with t = midpoint is 0.5")
    {
        REQUIRE(Math::ilerp(-1.0, 1.0, 0.0) == 0.5);
    }
    SECTION("ilerp is inverse of lerp")
    {
        const f32 a = -100.0f;
        const f32 b = 100.0f;
        const f32 t = GENERATE_COPY(take(100, random(a, b)));
        REQUIRE_THAT(Math::ilerp(a, b, Math::lerp(a, b, t)), Catch::Matchers::WithinRel(t, 1e-3f));
    }
}
TEST_CASE("fastMod", "[CoreMath]")
{
    SECTION("fastMod is equivalent to mod for bases of powers of 2")
    {
        for (u32 basePow = 0; basePow < 32; basePow++)
        {
            const u32 base = 1lu << basePow;
            const u32 value = GENERATE_COPY(take(10, random(0lu, ~0lu)));
            REQUIRE(Math::fastMod(value, base) == value % base);
        }
        for (u32 basePow = 0; basePow < 64; basePow++)
        {
            const u64 base = 1llu << basePow;
            const u64 value = GENERATE_COPY(take(10, random(0llu, ~0llu)));
            REQUIRE(Math::fastMod(value, base) == value % base);
        }
    }
}

// NOLINTEND