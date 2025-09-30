#pragma once
#include <expected>
#include <filesystem>

namespace bakers
{

enum class BakeError
{
    Unknown = 0,
};

using BakeResult = std::expected<void, BakeError>;

struct Context
{
    std::filesystem::path InitialDirectory{};
};

}

