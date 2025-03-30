#pragma once

#include "types.h"

#include <algorithm>
#include <vector>

namespace Utils
{
    template <typename T, typename V, typename Fn, typename Lg>
    requires requires(Fn comparator, Lg logger, const T& req, const V& avail)
    {
        { comparator(req, avail) } -> std::same_as<i32>;
        { logger(req) } -> std::same_as<void>;
    }
    bool checkArrayContainsSubArray(const std::vector<T>& required, const std::vector<V>& available,
        Fn&& comparator, Lg&& logger)
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

    template <typename T, typename Fn>
    requires requires(Fn comparator, const T& a, const T& b)
    {
        { comparator(a, b) } -> std::same_as<bool>;
    }
    T getIntersectionOrDefault(const std::vector<T>& desired, const std::vector<T>& available, Fn&& comparator)
    {
        for (auto& des : desired)
            for (auto& avail : available)
                if (comparator(des, avail))
                    return des;
        return available.front();
    }

    template <typename T, typename Cmp, typename Mrg>
    requires requires(Cmp comparator, Mrg merger, const T& a, const T& b)
    {
        { comparator(a, b) } -> std::same_as<i32>;
        { merger(a, b) } -> std::same_as<T>;
    }
    std::vector<T> mergeSets(const std::vector<T>& first, const std::vector<T>& second, Cmp&& comparator, Mrg&& merger)
    {
        if (first.empty())
            return second;

        std::vector<T> merged;
        merged.reserve(first.size() + second.size());
        
        u32 firstI = 0;
        u32 secondI = 0;
        
        while (firstI < first.size() && secondI < second.size())
        {
            auto comp = comparator(first[firstI], second[secondI]);

            if (comp == -1)
                merged.push_back(first[firstI++]);
            else if (comp == 1)
                merged.push_back(second[secondI++]);
            else
                merged.push_back(merger(first[firstI++], second[secondI++]));
        }
        
        while (firstI < first.size())
            merged.push_back(first[firstI++]);
        
        while (secondI < second.size())
            merged.push_back(second[secondI++]);

        return merged;
    }
}