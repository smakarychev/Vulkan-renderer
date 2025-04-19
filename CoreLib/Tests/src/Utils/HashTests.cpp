#include "catch2/catch_test_macros.hpp"
#include "utils/hash.h"


// NOLINTBEGIN

TEST_CASE("murmur3b32", "[Utils][Hash]")
{
    SECTION("empty string with zero seed should give zero")
    {
        REQUIRE(Hash::murmur3b32(0, 0, 0) == 0);
    }
    SECTION("empty string with not zero seed")
    {
        REQUIRE(Hash::murmur3b32(0, 0, 1) == 0x514e28b7);
    }
    SECTION("seed value is handled as unsigned")
    {
        REQUIRE(Hash::murmur3b32(0, 0, 0xffffffff) == 0x81f16f39);
    }
    SECTION("embedded nulls are handled")
    {
        REQUIRE(Hash::murmur3b32((const u8*)"\0\0\0\0", 4, 0) == 0x2362f9de);
    }
    SECTION("chunks")
    {
        REQUIRE(Hash::murmur3b32((const u8*)"a", 1, 0x9747b28c) == 0x7fa09ea6);
        REQUIRE(Hash::murmur3b32((const u8*)"aa", 2, 0x9747b28c) == 0x5d211726);
        REQUIRE(Hash::murmur3b32((const u8*)"aaa", 3, 0x9747b28c) == 0x283e0130);
        REQUIRE(Hash::murmur3b32((const u8*)"aaaa", 4, 0x9747b28c) == 0x5a97808a);
    }
    SECTION("endian order within the chunks")
    {
        REQUIRE(Hash::murmur3b32((const u8*)"a", 1, 0x9747b28c) == 0x7fa09ea6);
        REQUIRE(Hash::murmur3b32((const u8*)"ab", 2, 0x9747b28c) == 0x74875592);
        REQUIRE(Hash::murmur3b32((const u8*)"abc", 3, 0x9747b28c) == 0xc84a62dd);
        REQUIRE(Hash::murmur3b32((const u8*)"abcd", 4, 0x9747b28c) == 0xf0478627);
    }
    SECTION("utf8")
    {
        REQUIRE(Hash::murmur3b32((const u8*)"ππππππππ", 16, 0x9747b28c) == 0xd58063c1);
    }
}
TEST_CASE("fnv1a32 (Hash::bytes32)", "[Utils][Hash]")
{
    SECTION("empty string seed should give offsetBasis")
    {
        REQUIRE(Hash::bytes32(0, 0, 0x811c9dc5) == 0x811c9dc5);
    }
    SECTION("seed value is handled as unsigned")
    {
        REQUIRE(Hash::bytes32(0, 0, 0xffffffff) == 0xffffffff);
    }
    SECTION("utf8")
    {
        REQUIRE(Hash::bytes32((const u8*)"ππππππππ", 16, 0x811C9DC5) == 0x324acca5);
    }
}
TEST_CASE("fnv1a64 (Hash::bytes)", "[Utils][Hash]")
{
    SECTION("empty string should give offsetBasis")
    {
        REQUIRE(Hash::bytes(0, 0, 0xcbf29ce484222325) == 0xcbf29ce484222325);
    }
    SECTION("seed value is handled as unsigned")
    {
        REQUIRE(Hash::bytes(0, 0, 0xffffffffffffffff) == 0xffffffffffffffff);
    }
    SECTION("utf8")
    {
        REQUIRE(Hash::bytes((const u8*)"ππππππππ", 16, 0xcbf29ce484222325) == 0x9e614eb1084beb05);
    }
}
TEST_CASE("hash combine", "[Utils][Hash]")
{
    SECTION("combination of value with zero hash results in different value")
    {
        u64 hash = 123456;
        Hash::combine(hash, 0);
        REQUIRE(hash != 123456);
    }
    SECTION("combination of two zero hashes does not result in zero")
    {
        u64 hash = 0;
        Hash::combine(hash, 0);
        REQUIRE(hash != 0);
    }
}
TEST_CASE("charBytes hash", "[Utils][Hash]")
{
    SECTION("consteval hash is same with runtime hash (32-bit)")
    {
        constexpr u32 constevalHash = Hash::charBytes32("Hello", 5);
        const std::string string = "Hello";
        REQUIRE(constevalHash == Hash::charBytes32(string.c_str(), (u32)string.size()));
    }
    SECTION("consteval hash is same with runtime hash (64-bit)")
    {
        constexpr u64 constevalHash = Hash::charBytes("Hello", 5);
        const std::string string = "Hello";
        REQUIRE(constevalHash == Hash::charBytes(string.c_str(), (u32)string.size()));
    }
}
TEST_CASE("string hash", "[Utils][Hash]")
{
    SECTION("consteval hash is same with runtime hash (32-bit)")
    {
        REQUIRE(Hash::string32("Hello") == Hash::string32(std::string{"Hello"}));
    }
    SECTION("consteval hash is same with runtime hash (64-bit)")
    {
        REQUIRE(Hash::string("Hello") == Hash::string(std::string{"Hello"}));

    }
}
// NOLINTEND