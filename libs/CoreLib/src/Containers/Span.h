#pragma once

#include <array>
#include <span>
#include <vector>

#include "types.h"

template<typename T>
class Span {
public:
    using value_type = std::remove_cv_t<T>;

    constexpr Span() = default;
    constexpr Span(std::span<T> span) : m_Span(span) {}
    template <typename R>
    constexpr Span(std::span<R> span)
        requires std::is_same_v<std::decay_t<T>, std::byte>
        : m_Span((const T*)span.data(), span.size_bytes()) {}
    constexpr Span(const Span& span) : m_Span(span) {}
    template <typename R>
    constexpr Span(Span<R> span)
        requires std::is_same_v<std::decay_t<T>, std::byte>
        : m_Span((const T*)span.data(), span.size() * sizeof(R)) {}
    constexpr Span(std::initializer_list<std::remove_const_t<T>> initializerList)
        : m_Span(initializerList.begin(), initializerList.size()) {}
    constexpr Span(T* pointer, std::size_t size) : m_Span(pointer, size) {}
    constexpr Span(const std::vector<std::remove_const_t<T>>& vector)
        : m_Span(const_cast<T*>(vector.data()), vector.size()) {}

    template <typename R>
    constexpr Span(std::initializer_list<R> initializerList)
        requires std::is_same_v<std::decay_t<T>, std::byte>
        : m_Span((const T*)initializerList.begin(), initializerList.size() * sizeof(R)) {}
    template <typename R>
    constexpr Span(const std::vector<R>& vector)
        requires std::is_same_v<std::decay_t<T>, std::byte>
        : m_Span((const T*)vector.data(), vector.size() * sizeof(R)) {}

    template <usize Size>
    constexpr Span(const std::array<std::remove_const_t<T>, Size>& array)
        : m_Span(const_cast<T*>(array.data()), Size) {}
    template <typename R, usize Size>
    constexpr Span(const std::array<R, Size>& array)
        requires std::is_same_v<std::decay_t<T>, std::byte>
        : m_Span((const T*)array.data(), Size * sizeof(R)) {}

    constexpr operator std::span<T>() const { return m_Span; }

    constexpr Span Subspan() const { return m_Span.subspan(); }
    constexpr Span subspan() const { return Subspan(); }
    constexpr Span Subspan(const u64 offset, const u64 count) const { return m_Span.subspan(offset, count); }
    constexpr Span subspan(const u64 offset, const u64 count) const { return Subspan(offset, count); }

    constexpr T* Data() const { return m_Span.data(); }
    constexpr T* data() const { return Data(); }

    constexpr usize Size() const { return m_Span.size(); }
    constexpr usize size() const { return Size(); }
    constexpr bool Empty() const { return m_Span.empty(); }
    constexpr bool empty() const { return Empty(); }

    constexpr T& Front() const { return m_Span.front(); }
    constexpr T& front() const { return Front(); }
    constexpr T& Back() const { return m_Span.back(); }
    constexpr T& back() const { return Back(); }
    
    constexpr T& operator[](usize index) { return m_Span[index]; }
    constexpr const T& operator[](usize index) const { return m_Span[index]; }

    constexpr auto begin() const { return m_Span.begin(); }
    constexpr auto end() const { return m_Span.end(); }

    constexpr auto cbegin() const { return m_Span.cbegin(); }
    constexpr auto cend() const { return m_Span.cend(); }
private:
    std::span<T> m_Span;
};
