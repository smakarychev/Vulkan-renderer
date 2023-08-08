#pragma once

#include "types.h"

#include <algorithm>
#include <fstream>
#include <vector>

#include <shaderc/shaderc.hpp>

namespace utils
{
    template <typename T, typename V, typename Fn, typename Pr>
    bool checkArrayContainsSubArray(const std::vector<T>& required, const std::vector<V>& available, Fn comparator, Pr printer)
    {
        bool success = true;    
        for (auto& req : required)
        {
            if (std::ranges::none_of(available, [req, comparator](auto& avail) { return comparator(req, avail); }))
            {
                printer(req);
                success = false;
            }
        }
        return success;
    }

    inline std::vector<u32> compileShaderToSPIRV(std::string_view shaderPath)
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
        shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(shaderSource, shaderc_glsl_infer_from_source, shaderPath.data(), options);
        if (module.GetCompilationStatus() != shaderc_compilation_status_success)
        {
            LOG("Shader compilation error:\n {}", module.GetErrorMessage());
            return {};
        }

        return {module.cbegin(), module.cend()};
    }
}
