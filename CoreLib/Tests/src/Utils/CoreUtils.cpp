#include "catch2/catch_test_macros.hpp"
#include "catch2/generators/catch_generators.hpp"
#include "catch2/generators/catch_generators_adapters.hpp"
#include "catch2/generators/catch_generators_random.hpp"

#include "utils/CoreUtils.h"



// NOLINTBEGIN

TEST_CASE("align", "[Utils][CoreUtils]")
{
    SECTION("supports 0 alignment")
    {
        REQUIRE(CoreUtils::align(1, 0) == 1);
    }
    SECTION("aligns to the power of 2 alignment")
    {
        for (u32 i = 0; i < 64; i++)
            REQUIRE(CoreUtils::align(
                GENERATE_COPY(take(100, random(0llu, ~0llu))),
                1llu << i) % (1llu << i) == 0);
    }
}

// NOLINTEND