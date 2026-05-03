#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>

#include <CoreLib/utils/MemoryUtils.h>


// NOLINTBEGIN

TEST_CASE("align", "[Utils][MemoryUtils]")
{
    SECTION("supports 0 alignment")
    {
        REQUIRE(lux::mem::alignAddressPow2(1, 0) == 1);
    }
    SECTION("aligns to the power of 2 alignment")
    {
        for (u32 i = 0; i < 16; i++)
            REQUIRE(lux::mem::alignAddressPow2(
            GENERATE_COPY(take(100, random((u16)0llu, (u16)~0llu))),
            1llu << i) % (1llu << i) == 0);
    }
    SECTION("aligns to the not power of 2 alignment")
    {
        REQUIRE(lux::mem::alignAddress(12, 14) == 14);
        REQUIRE(lux::mem::alignAddress(1, 14) == 14);
    }
}

// NOLINTEND