#include "catch2/catch_test_macros.hpp"

#include <CoreLib/Containers/FreeList.h>

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
    SECTION("Can copy construct")
    {
        lux::FreeList<u32> list;
        list.insert(2);

        lux::FreeList<u32> copy(list);
        REQUIRE(copy.size() == list.size());
        REQUIRE(copy.capacity() == list.capacity());
        REQUIRE(copy.insert(1) == list.insert(1));
    }
    SECTION("Can copy assign")
    {
        lux::FreeList<u32> list;
        list.insert(2);

        lux::FreeList<u32> copy = {};
        copy = list;
        REQUIRE(copy.size() == list.size());
        REQUIRE(copy.capacity() == list.capacity());
        REQUIRE(copy.insert(1) == list.insert(1));
    }
    SECTION("Can move construct")
    {
        lux::FreeList<u32> list;
        const u32 index = list.insert(2);

        const u32 size = list.size();
        const u32 capacity = list.capacity();

        lux::FreeList<u32> move(std::move(list));
        REQUIRE(move.size() == size);
        REQUIRE(move.capacity() == capacity);
        REQUIRE(move.insert(1) != index);

        REQUIRE(list.size() == 0);
        REQUIRE(list.capacity() == 0);
    }
    SECTION("Can copy assign")
    {
        lux::FreeList<u32> list;
        const u32 index = list.insert(2);

        const u32 size = list.size();
        const u32 capacity = list.capacity();

        lux::FreeList<u32> move = {};
        move = std::move(list);
        REQUIRE(move.size() == size);
        REQUIRE(move.capacity() == capacity);
        REQUIRE(move.insert(1) != index);
        
        REQUIRE(list.size() == 0);
        REQUIRE(list.capacity() == 0);
    }
}

// NOLINTEND