#pragma once
#include "CoreLib/core.h"

#include <atomic>

namespace lux
{
class RefCountedBase
{
public:
    void AddReference() const noexcept
    {
        [[maybe_unused]] const auto old = m_ReferenceCount.fetch_add(1, std::memory_order_relaxed);
        ASSERT(old > 0)
    }
    
    constexpr RefCountedBase(const RefCountedBase&) = delete;
    constexpr RefCountedBase& operator=(const RefCountedBase&) = delete;
    constexpr RefCountedBase(RefCountedBase&&) noexcept = delete;
    constexpr RefCountedBase& operator=(RefCountedBase&&) noexcept = delete;
protected:
    bool ReleaseReferenceBase() const noexcept
    {
        return m_ReferenceCount.fetch_sub(1, std::memory_order_release) == 1;
    }
    constexpr RefCountedBase() noexcept = default;
    ~RefCountedBase() noexcept = default;
protected:
    mutable std::atomic_size_t m_ReferenceCount{1};
};

template <typename Impl>
class RefCounted : public RefCountedBase
{
public:
    void ReleaseReference() const noexcept
    {
        if (ReleaseReferenceBase())
        {
            std::atomic_thread_fence(std::memory_order_acquire);
            delete (Impl*)this;
        }
    }

protected:
    constexpr RefCounted() noexcept = default;
    ~RefCounted() noexcept = default;
};

struct DefaultRefCountedTraits
{
    template <typename T>
    static void AddReference(T* refCounted) noexcept { refCounted->AddReference(); }    
    template <typename T>
    static void ReleaseReference(T* refCounted) noexcept { refCounted->ReleaseReference(); }    
};

template <typename T, typename Traits = DefaultRefCountedTraits>
class Ref
{
public:
    constexpr Ref() noexcept = default; 
   
    constexpr Ref(std::nullptr_t) noexcept {}
    
    static constexpr Ref Adopt(T* ptr) noexcept
    {
        Ref result;
        result.m_Ptr = ptr;
        return result;
    }

    static constexpr Ref Take(T* ptr) noexcept
    {
        Ref result;
        result.m_Ptr = ptr;
        DoAddReference(result.m_Ptr);
        
        return result;
    }
   
    constexpr Ref(const Ref& other) noexcept : m_Ptr(other.m_Ptr)
    {
        DoAddReference(other.m_Ptr);
    }
    
    constexpr Ref(Ref&& other) noexcept : m_Ptr(other.release())
    {
    }

    template <typename U, typename UTraits>
    requires std::same_as<Traits, UTraits> && std::convertible_to<U*, T*>
    constexpr Ref(const Ref<U, UTraits>& other) noexcept
        : m_Ptr(other.Get())
    {
        DoAddReference(m_Ptr);
    }

    template <typename U, typename UTraits>
    requires std::same_as<Traits, UTraits> && std::convertible_to<U*, T*>
    constexpr Ref(Ref&& other) noexcept
        : m_Ptr(other.release())
    {
    }
    
    constexpr Ref& operator=(const Ref& other) noexcept
    {
        T* next = other.m_Ptr;
        DoAddReference(next);
        DoReleaseReference(std::exchange(m_Ptr, next));
        
        return *this;
    }
    
    constexpr Ref& operator=(Ref&& other) noexcept
    {
        if (this != &other)
            DoReleaseReference(std::exchange(m_Ptr, other.Release()));
        return *this;
    }
    
    template <typename U, typename UTraits>
    requires std::same_as<Traits, UTraits> && std::convertible_to<U*, T*>
    constexpr Ref& operator=(const Ref& other) noexcept
    {
        T* next = other.m_Ptr;
        DoAddReference(next);
        DoReleaseReference(std::exchange(m_Ptr, next));
        
        return *this;
    }
    
    template <typename U, typename UTraits>
    requires std::same_as<Traits, UTraits> && std::convertible_to<U*, T*>
    constexpr Ref& operator=(Ref&& other) noexcept
    {
        DoReleaseReference(std::exchange(m_Ptr, other.Release()));
        
        return *this;
    }
   
    constexpr ~Ref() noexcept
    {
        Reset();
    }
    
    constexpr explicit operator bool() const noexcept
    {
        return m_Ptr != nullptr;
    }
    
    constexpr const T* operator->() const noexcept
    {
        return m_Ptr;
    }

    template <class X=T>
    constexpr X& operator*() const noexcept requires std::is_base_of_v<X, T>
    {
        return *m_Ptr;
    }

    template <class M, class X=T>
    constexpr M& operator->*(M X::* memberPointer) const noexcept requires std::is_base_of_v<X, T>
    {
        return m_Ptr->*memberPointer;
    }
    
    constexpr T* Get() const noexcept { return m_Ptr; }
    constexpr T* get() const noexcept { return Get(); }
    
    constexpr T* Release() noexcept
    {
        return std::exchange(m_Ptr, nullptr);
    }
    constexpr T* release() noexcept { return Release(); }
    
    constexpr void Reset() noexcept
    {
        T* toRelease = std::exchange(m_Ptr, nullptr);
        DoReleaseReference(toRelease);
    }
    constexpr void reset() noexcept { return Reset(); }
    
    constexpr void Swap(Ref& other) noexcept
    {
        T* ptr = std::exchange(m_Ptr, other.m_Ptr);
        other.m_Ptr = ptr;
    }
    friend constexpr void swap(Ref& a, Ref& b) noexcept { a.Swap(b); }
    
    template <typename U, typename UTraits>
    requires std::same_as<Traits, UTraits> && std::convertible_to<U*, T*>
    friend constexpr bool operator==(const Ref& a, const Ref<U, UTraits>& b) noexcept
    {
        return a.m_Ptr == b.Get();
    }

    friend constexpr bool operator==(const Ref& a, std::nullptr_t) noexcept
    {
        return a.m_Ptr == nullptr;
    }

    template <typename U, typename UTraits>
    requires std::same_as<Traits, UTraits> && std::convertible_to<U*, T*>
    friend constexpr auto operator<=>(const Ref& a, const Ref<U, UTraits>& b) noexcept
    {
        return a.m_Ptr <=> (T*)b.Get();
    }

    friend constexpr auto operator<=>(const Ref& a, std::nullptr_t) noexcept
    {
        return a.m_Ptr <=> (T*)nullptr;
    }
private:
    static constexpr void DoAddReference(T* ptr) noexcept
    {
        if (ptr)
            Traits::AddReference(ptr);
    }
    static constexpr void DoReleaseReference(T* ptr) noexcept
    {
        if (ptr)
            Traits::ReleaseReference(ptr);
    }
private:
    T* m_Ptr{nullptr};
};

template <typename T, typename Traits = DefaultRefCountedTraits, typename ... Args>
Ref<T, Traits> adoptRef(Args&&... args)
{
    return Ref<T, Traits>::Adopt(new T(std::forward<Args>(args)...));
}
template <typename T, typename Traits = DefaultRefCountedTraits, typename ... Args>
Ref<T, Traits> takeRef(Args&&... args)
{
    return Ref<T, Traits>::Take(new T(std::forward<Args>(args)...));
}
}

namespace std
{
template <typename T, typename Traits>
struct hash<lux::Ref<T, Traits>>
{
    size_t operator()(const lux::Ref<T, Traits>& ptr) const noexcept
    {
        return hash<T*>{}(ptr.Get());
    }
};
}