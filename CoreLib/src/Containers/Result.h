#pragma once
#include <expected>

template <class T, class Err>
using Result = std::expected<T, Err>;
