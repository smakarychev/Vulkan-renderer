#pragma once

#include "types.h"

#include <filesystem>

namespace Platform
{
    using DllHandle = u64;
    static constexpr DllHandle INVALID_DLL_HANDLE = ~(DllHandle)0;

    DllHandle dllOpen(const std::filesystem::path& path);
    void dllFree(DllHandle dll);
    void* dllLoadFunction(DllHandle dll, std::string_view name);

    template <typename FnType>
    FnType dllLoadFunction(DllHandle dll, std::string_view name)
    {
        return (FnType)dllLoadFunction(dll, name);
    }
}

