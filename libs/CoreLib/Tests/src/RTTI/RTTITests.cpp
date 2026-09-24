#include <unordered_set>
#include <catch2/catch_test_macros.hpp>

#include <CoreLib/RTTI/RTTI.h>

// NOLINTBEGIN

template <typename T>
struct BaseTemplated
{
    LUX_RTTI(BaseTemplated)
};

TEST_CASE("RTTI", "[RTTI]")
{
    SECTION("Can get basic type info")
    {
        struct Base
        {
            LUX_RTTI(Base)
        };
        
        auto typeInfo = Base::GetStaticTypeInfo();
        REQUIRE(typeInfo.Parents.empty());
        
        Base base;
        REQUIRE(typeInfo == lux::rttiStaticType<Base>());
        REQUIRE(typeInfo == lux::rttiType(&base));
    }
    SECTION("Can get hierarchical type info")
    {
        struct Base
        {
            LUX_RTTI(Base)
        };
        struct Derived : Base
        {
            LUX_RTTI(Derived, Base)
        };
        
        auto typeInfo = Derived::GetStaticTypeInfo();
        REQUIRE(typeInfo.Parents.size() == 1);
        REQUIRE(typeInfo.Parents.front() == Base::GetStaticTypeInfo());
    }
    SECTION("Can get type info from base pointer")
    {
        struct Base
        {
            LUX_RTTI(Base)
        };
        struct Derived : Base
        {
            LUX_RTTI(Derived, Base)
        };
        
        std::unique_ptr<Base> derived = std::make_unique<Derived>(); 
        
        auto typeInfo = derived->GetTypeInfo();
        REQUIRE(typeInfo != Base::GetStaticTypeInfo());
        REQUIRE(typeInfo == Derived::GetStaticTypeInfo());
    }
    SECTION("Can check if type is in hierarchy")
    {
        struct Unrelated
        {
            LUX_RTTI(Unrelated)
        };
        struct Base
        {
            LUX_RTTI(Base)
        };
        struct Derived : Base
        {
            LUX_RTTI(Derived, Base)
        };
        struct DeepDerived : Derived
        {
            LUX_RTTI(DeepDerived, Derived)
        };
        
        std::unique_ptr<Base> deepDerived = std::make_unique<DeepDerived>(); 
        
        auto typeInfo = deepDerived->GetTypeInfo();
        REQUIRE(!typeInfo.IsA<Unrelated>());
        REQUIRE(!typeInfo.IsA(Unrelated::GetStaticTypeInfo()));
        REQUIRE(typeInfo == DeepDerived::GetStaticTypeInfo());
        REQUIRE(typeInfo.IsExactlyA(DeepDerived::GetStaticTypeInfo()));
        REQUIRE(typeInfo.IsA(Derived::GetStaticTypeInfo()));
        REQUIRE(typeInfo.IsA(Base::GetStaticTypeInfo()));
        REQUIRE(!typeInfo.IsExactlyA(Derived::GetStaticTypeInfo()));
        REQUIRE(!typeInfo.IsExactlyA(Base::GetStaticTypeInfo()));
    }
    SECTION("Can store type info in hash set")
    {
        std::unordered_set<lux::TypeInfo> typeInfos;
        SUCCEED();
    }
    SECTION("Templated classes have different type infos")
    {
        BaseTemplated<i32> a = {};
        BaseTemplated<f32> b = {};
        REQUIRE(a.GetTypeInfo() != b.GetTypeInfo());
    }
}

// NOLINTEND
