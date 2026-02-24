#pragma once

#include "Log.h"

#include <format>
#include <iostream>
#include <stacktrace>

#ifdef NDEBUG
#define ASSERT(x, ...) ((void)0)
#else
#define ASSERT(x, ...) if(x) {} else { LUX_LOG_ERROR("Assertion failed"); LUX_LOG_ERROR(__VA_ARGS__); LUX_LOG_ERROR("{}", std::stacktrace::current()); __debugbreak(); }
#endif

#define BIT(x) (1 << (x))

#define ENUM_FLAGS_UNARY_NOT(enumType) \
inline constexpr enumType operator ~(enumType a) \
{ \
    static_assert(std::is_enum_v<enumType>, "Provided type is not an enum"); \
    return enumType(~std::underlying_type_t<enumType>(a)); \
} \

#define ENUM_FLAGS_BINARY_CONST_OP(enumType, op) \
inline constexpr enumType operator op(enumType a, enumType b) \
{ \
    static_assert(std::is_enum_v<enumType>, "Provided type is not an enum"); \
    return enumType(std::underlying_type_t<enumType>(a) op std::underlying_type_t<enumType>(b)); \
} \

#define ENUM_FLAGS_BINARY_OP(enumType, op, constOp) \
inline constexpr enumType& operator op(enumType& a, enumType b) \
{ \
    static_assert(std::is_enum_v<enumType>, "Provided type is not an enum"); \
    return a = enumType(a constOp b); \
} \

template <typename Enum>
constexpr bool enumHasAll(Enum a, Enum b)
{
    static_assert(std::is_enum_v<Enum>, "Provided type is not an enum");
    return (std::underlying_type_t<Enum>(a) & std::underlying_type_t<Enum>(b)) == std::underlying_type_t<Enum>(b);
}

template <typename Enum>
constexpr bool enumHasAny(Enum a, Enum b)
{
    static_assert(std::is_enum_v<Enum>, "Provided type is not an enum");
    return (std::underlying_type_t<Enum>(a) & std::underlying_type_t<Enum>(b)) != 0;
}

template <typename Enum>
constexpr bool enumHasOnly(Enum a, Enum b)
{
    static_assert(std::is_enum_v<Enum>, "Provided type is not an enum");
    return (std::underlying_type_t<Enum>(a) ^ std::underlying_type_t<Enum>(b)) == 0;
}

#define CREATE_ENUM_FLAGS_OPERATORS(enumType) \
    ENUM_FLAGS_UNARY_NOT(enumType) \
    ENUM_FLAGS_BINARY_CONST_OP(enumType, |) \
    ENUM_FLAGS_BINARY_CONST_OP(enumType, &) \
    ENUM_FLAGS_BINARY_CONST_OP(enumType, ^) \
    ENUM_FLAGS_BINARY_OP(enumType, |=, |) \
    ENUM_FLAGS_BINARY_OP(enumType, &=, &) \
    ENUM_FLAGS_BINARY_OP(enumType, ^=, ^) 

#if defined _MSC_VER
#   define GENERATOR_PRETTY_FUNCTION __FUNCSIG__
#elif defined __clang__ || (defined __GNUC__)
#   define GENERATOR_PRETTY_FUNCTION __PRETTY_FUNCTION__
#endif

#define C_CONCAT_(x,y) x##y
#define C_CONCAT(x,y) C_CONCAT_(x,y)