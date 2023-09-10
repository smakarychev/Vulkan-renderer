#pragma once

#include "types.h"
#include "core.h"

#include <algorithm>
#include <fstream>
#include <vector>

#include <shaderc/shaderc.hpp>

namespace utils
{
    // Fn : (const char* req, T& avail) -> bool; Lg : (const char* req) -> void
    template <typename T, typename V, typename Fn, typename Lg>
    bool checkArrayContainsSubArray(const std::vector<T>& required, const std::vector<V>& available, Fn comparator, Lg logger)
    {
        bool success = true;    
        for (auto& req : required)
        {
            if (std::ranges::none_of(available, [req, comparator](auto& avail) { return comparator(req, avail); }))
            {
                logger(req);
                success = false;
            }
        }
        return success;
    }

    // Fn : (const T& a, const T& b) -> bool
    template <typename T, typename Fn>
    T getIntersectionOrDefault(const std::vector<T>& desired, const std::vector<T>& available, Fn comparator)
    {
        for (auto& des : desired)
            for (auto& avail : available)
                if (comparator(des, avail))
                    return des;
        return available.front();
    }
    
    inline std::vector<u32> compileShaderToSPIRV(std::string_view shaderPath, shaderc_shader_kind shaderKind)
    {
        // read shader glsl code as a string
        std::ifstream file(shaderPath.data(), std::ios::in | std::ios::binary);
        if (!file.is_open())
        {
            LOG("Error reading file {}", shaderPath);
            return {};
        }
        std::string shaderSource((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        
        shaderc::Compiler compiler;
        shaderc::CompileOptions options;
        options.SetOptimizationLevel(shaderc_optimization_level_size);
        shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(shaderSource, shaderKind, shaderPath.data(), options);
        if (module.GetCompilationStatus() != shaderc_compilation_status_success)
        {
            LOG("Shader compilation error:\n {}", module.GetErrorMessage());
            return {};
        }

        return {module.cbegin(), module.cend()};
    }

    template <typename T>
    void hashCombine(u64& seed, const T& val)
    {
        std::hash<T> hasher;
        seed ^= hasher(val) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }
}
