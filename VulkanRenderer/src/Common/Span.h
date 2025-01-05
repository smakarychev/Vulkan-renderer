#pragma once

#include <span>
#include <vector>

#include "types.h"

template<typename T>
class Span {
public:
    using value_type = std::remove_cv_t<T>;

    constexpr Span() = default;
    constexpr Span(std::span<T> span) : m_Span(span) {}
    constexpr Span(std::initializer_list<T> initializerList)
        : m_Span(initializerList.begin(), initializerList.size()) {}
    constexpr Span(T* pointer, std::size_t size) : m_Span(pointer, size) {}
    constexpr Span(const std::vector<std::remove_const_t<T>>& vector)
        : m_Span(const_cast<T*>(vector.data()), vector.size()) {}
    template <typename R>
    constexpr Span(const std::vector<R>& vector)
        requires std::is_same_v<std::decay_t<T>, std::byte>
        : m_Span(const_cast<T*>((const T*)vector.data()), vector.size() * sizeof(R)) {}

    constexpr operator std::span<T>() const { return m_Span; }

    constexpr T* Data() const { return m_Span.data(); }
    constexpr T* data() const { return Data(); }

    constexpr usize Size() const { return m_Span.size(); }
    constexpr usize size() const { return Size(); }
    constexpr bool Empty() const { return m_Span.empty(); }
    constexpr bool empty() const { return Empty(); }

    constexpr T& operator[](usize index) { return m_Span[index]; }
    constexpr const T& operator[](usize index) const { return m_Span[index]; }

    constexpr auto begin() const { return m_Span.begin(); }
    constexpr auto end() const { return m_Span.end(); }

    constexpr auto cbegin() const { return m_Span.cbegin(); }
    constexpr auto cend() const { return m_Span.cend(); }
private:
    std::span<T> m_Span;
};
