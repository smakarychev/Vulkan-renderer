#pragma once

#include <algorithm>
#include <vector>

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
}
