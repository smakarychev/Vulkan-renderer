#include "catch2/catch_test_macros.hpp"

#include "Containers/FreeList.h"

// NOLINTBEGIN

TEST_CASE("FreeList", "[Containers][FreeList]")
{
    SECTION("Can create for trivially destructable types")
    {
        lux::FreeList<u32> listA;

        struct Test
        {
            u32 Element;
        };
        lux::FreeList<Test> listB;
    }
    SECTION("Can add elements")
    {
        lux::FreeList<u32> list;
        REQUIRE(list.Size() == 0);
        list.Insert(1);
        REQUIRE(list.Size() == 1);
        list.Insert(2);
        REQUIRE(list.Size() == 2);
    }
    SECTION("Can remove elements")
    {
        lux::FreeList<u32> list;
        REQUIRE(list.Size() == 0);
        const u32 index = list.Insert(1);
        REQUIRE(list.Size() == 1);
        list.Erase(index);
        REQUIRE(list.Size() == 0);
    }
    SECTION("Remove does not change capacity")
    {
        lux::FreeList<u32> list;
        REQUIRE(list.Size() == 0);
        const u32 index = list.Insert(1);
        REQUIRE(list.Capacity() == 1);
        list.Erase(index);
        REQUIRE(list.Capacity() == 1);
    }
    SECTION("Can reuse index after remove")
    {
        lux::FreeList<u32> list;
        REQUIRE(list.Size() == 0);
        u32 index = list.Insert(1);
        u32 index2 = list.Insert(1);
        REQUIRE(index != index2);
        list.Erase(index);
        REQUIRE(list.Insert(1) == index);

        list.Erase(index);
        list.Erase(index2);
        REQUIRE(list.Insert(1) == index2);
        REQUIRE(list.Insert(1) == index);
    }
    SECTION("Can get by index")
    {
        lux::FreeList<u32> list;
        REQUIRE(list.Size() == 0);
        u32 index = list.Insert(13);
        REQUIRE(list[index] == 13);
    }
}

// NOLINTEND