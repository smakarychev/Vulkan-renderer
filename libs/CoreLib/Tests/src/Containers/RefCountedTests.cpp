#include "catch2/catch_test_macros.hpp"

#include <unordered_map>
#include <unordered_set>
#include <CoreLib/Containers/RefCounted.h>

// NOLINTBEGIN

using namespace lux;

TEST_CASE("RefCounted", "[Containers][RefCounted]")
{
    SECTION("Can create refcounted object by inheriting from base RefCounted")
    {
        struct MyRefCounted : RefCounted<MyRefCounted>
        {
        };
        
        MyRefCounted refCounted;
        SUCCEED();
    }
    SECTION("Default ref counted object has initial count of 1")
    {
        struct MyRefCounted : RefCounted<MyRefCounted>
        {
            u32 GetCount() const { return (u32)m_ReferenceCount; }
        };
        
        MyRefCounted refCounted;
        REQUIRE(refCounted.GetCount() == 1);
    }
    SECTION("Default ref counted can add and release references")
    {
        struct MyRefCounted : RefCounted<MyRefCounted>
        {
            u32 GetCount() const { return (u32)m_ReferenceCount; }
        };
        
        MyRefCounted refCounted;
        REQUIRE(refCounted.GetCount() == 1);
        refCounted.AddReference();
        REQUIRE(refCounted.GetCount() == 2);
        refCounted.ReleaseReference();
        REQUIRE(refCounted.GetCount() == 1);
    }
}

TEST_CASE("Ref", "[Containers][Ref]")
{
    struct MyRefCounted : RefCounted<MyRefCounted>
    {
        u32 GetCount() const { return (u32)m_ReferenceCount; }
        bool ReturnTrue() const { return true; }
    };
    
    SECTION("Can create empty Ref")
    {
        Ref<MyRefCounted> ptr{};
        REQUIRE(ptr.get() == nullptr);
    }
    SECTION("Can create Ref from nullptr")
    {
        Ref<MyRefCounted> ptr(nullptr);
        REQUIRE(ptr.get() == nullptr);
    }
    SECTION("Can create Ref from pointer and do not increment initial count")
    {
        auto ptr = Ref<MyRefCounted>::Adopt(new MyRefCounted());
        REQUIRE(ptr.get());
        REQUIRE(ptr.get()->GetCount() == 1);
    }
    SECTION("Can create Ref from pointer and increment initial count")
    {
        auto ptr = Ref<MyRefCounted>::Take(new MyRefCounted());
        REQUIRE(ptr.get());
        REQUIRE(ptr.get()->GetCount() == 2);
    }
    SECTION("Can access underlying type methods by ->")
    {
        auto ptr = Ref<MyRefCounted>::Adopt(new MyRefCounted());
        REQUIRE(ptr->ReturnTrue());
    }
    SECTION("Can get underlying pointer by get")
    {
        auto ptr = Ref<MyRefCounted>::Adopt(new MyRefCounted());
        auto* raw = ptr.Get();
        REQUIRE(raw->ReturnTrue());
    }
    SECTION("Can get underlying reference by *")
    {
        auto ptr = Ref<MyRefCounted>::Adopt(new MyRefCounted());
        auto& raw = *ptr;
        REQUIRE(raw.ReturnTrue());
    }
    SECTION("Will call destructor when reference count reaches zero")
    {
        bool destructorCalled = false;
        struct MyRefCountedWithDestructor : RefCounted<MyRefCountedWithDestructor>
        {
            MyRefCountedWithDestructor(bool& flag) : Flag(&flag) {}
            ~MyRefCountedWithDestructor() { *Flag = true; }
            bool* Flag{};
        };
        
        {
            
            auto ptr = 
                Ref<MyRefCountedWithDestructor>::Adopt(new MyRefCountedWithDestructor(destructorCalled));
            REQUIRE_FALSE(destructorCalled);
        }
        REQUIRE(destructorCalled);
    }
    SECTION("Copy will increase reference count")
    {
        auto ptr = Ref<MyRefCounted>::Adopt(new MyRefCounted());
        const u32 refcount = ptr->GetCount();
        Ref<MyRefCounted> ptr2 = ptr;
        REQUIRE(ptr2->GetCount() == refcount + 1);
        {
            Ref<MyRefCounted> ptr3 = nullptr;
            ptr3 = ptr;
            REQUIRE(ptr3->GetCount() == refcount + 2);
        }
        REQUIRE(ptr->GetCount() == refcount + 1);
    }
    SECTION("Self copy assignment is allowed")
    {
        auto ptr = Ref<MyRefCounted>::Adopt(new MyRefCounted());
        const u32 refcount = ptr->GetCount();
        ptr = ptr;
        REQUIRE(ptr->GetCount() == refcount);
    }
    SECTION("Move will not increase reference count")
    {
        auto ptr = Ref<MyRefCounted>::Adopt(new MyRefCounted());
        const u32 refcount = ptr->GetCount();
        Ref<MyRefCounted> ptr2 = std::move(ptr);
        REQUIRE(ptr2->GetCount() == refcount);
        {
            Ref<MyRefCounted> ptr3 = nullptr;
            ptr3 = std::move(ptr2);
            REQUIRE(ptr3->GetCount() == refcount);
        }
    }
    SECTION("Self move assignment is allowed")
    {
        auto ptr = Ref<MyRefCounted>::Adopt(new MyRefCounted());
        const u32 refcount = ptr->GetCount();
        ptr = std::move(ptr);
        REQUIRE(ptr->GetCount() == refcount);
    }
    SECTION("Can release")
    {
        MyRefCounted* raw;
        {
            auto ptr = Ref<MyRefCounted>::Adopt(new MyRefCounted());
            raw = ptr.release();
        }
        REQUIRE(raw->GetCount() == 1);
    }
    SECTION("Can reset")
    {
        auto ptr = Ref<MyRefCounted>::Adopt(new MyRefCounted());
        REQUIRE(ptr.get() != nullptr);
        ptr.reset();
        REQUIRE(ptr.get() == nullptr);
    }
    SECTION("Can swap")
    {
        auto ptrA = Ref<MyRefCounted>::Adopt(new MyRefCounted());
        auto ptrB = Ref<MyRefCounted>::Adopt(new MyRefCounted());
        MyRefCounted* rawA = ptrA.get();
        MyRefCounted* rawB = ptrB.get();
        
        ptrA.Swap(ptrB);
        REQUIRE(ptrB.get() == rawA);
        REQUIRE(ptrA.get() == rawB);
        
        swap(ptrA, ptrB);
        REQUIRE(ptrA.get() == rawA);
        REQUIRE(ptrB.get() == rawB);
    }
    SECTION("Can create with utility function")
    {
        struct MyRefCountedWithConstructor : RefCounted<MyRefCountedWithConstructor>
        {
            MyRefCountedWithConstructor(int val, std::string name) {}
        };
        
        auto ptr = adoptRef<MyRefCountedWithConstructor>(1, "test");
        REQUIRE(ptr.get());
    }
    SECTION("Can compare")
    {
        auto ptr = adoptRef<MyRefCounted>();
        auto ptr2 = adoptRef<MyRefCounted>();
        REQUIRE(ptr != ptr2);
        REQUIRE(ptr == ptr);
        REQUIRE(ptr != nullptr);
    }
    SECTION("Can reference derived class")
    {
        struct Derived : MyRefCounted {};
        Ref<MyRefCounted> ref = adoptRef<Derived>();
        SUCCEED();
    }
    SECTION("Can compare base and derived class pointers")
    {
        struct Derived : MyRefCounted {};
        Ref<Derived> ref = adoptRef<Derived>();
        Ref<MyRefCounted> refBase = ref;
        REQUIRE(ref == refBase);
    }
    SECTION("Can use as key in hashmap")
    {
        std::unordered_map<Ref<MyRefCounted>, bool> map;
        std::unordered_set<Ref<MyRefCounted>> set;
        SUCCEED();
    }
}

// NOLINTEND