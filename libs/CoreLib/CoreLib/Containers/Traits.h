#pragma once

#include <span>
#include <vector>

#include "Span.h"

template <typename T>
struct is_span : std::false_type {};
template <typename ElementType, std::size_t Extent>
struct is_span<std::span<ElementType, Extent>> : std::true_type {};
template <typename ElementType>
struct is_span<Span<ElementType>> : std::true_type {};
template <typename T>
inline constexpr bool is_span_v = is_span<std::decay_t<T>>::value;

template <typename T>
struct is_array : std::false_type {};
template <typename ElementType, std::size_t Size>
struct is_array<std::array<ElementType, Size>> : std::true_type {};
template <typename T>
inline constexpr bool is_array_v = is_array<std::decay_t<T>>::value;

template <typename T>
struct is_vector : std::false_type {};
template <typename ElementType, typename Allocator>
struct is_vector<std::vector<ElementType, Allocator>> : std::true_type {};
template <typename T>
inline constexpr bool is_vector_v = is_vector<std::decay_t<T>>::value;
