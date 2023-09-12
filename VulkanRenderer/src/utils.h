#pragma once

#include "types.h"
#include "core.h"

#include <algorithm>
#include <fstream>
#include <vector>
#include <filesystem>

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
    
    template <typename T>
    void hashCombine(u64& seed, const T& val)
    {
        std::hash<T> hasher;
        seed ^= hasher(val) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }
    
    void runSubProcess(const std::filesystem::path& executablePath, const std::vector<std::string>& args);
}
