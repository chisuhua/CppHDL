// src/core/types.cpp
#include "types.h"

namespace ch { namespace core { namespace constants {

// 定义常量
const sdata_type zero_1bit{0, 1};
const sdata_type zero_16bit{0, 16};
const sdata_type zero_32bit{0, 32};
const sdata_type zero_64bit{0, 64};
const sdata_type one_1bit{1, 1};
const sdata_type one_16bit{1, 16};
const sdata_type one_32bit{1, 32};
const sdata_type one_64bit{1, 64};
const sdata_type all_ones_8bit{0xFF, 8};
const sdata_type all_ones_16bit{0xFFFF, 16};
const sdata_type all_ones_32bit{0xFFFFFFFF, 32};

// ones 函数的实现
const sdata_type& ones(uint32_t width) {
    static thread_local std::unordered_map<uint32_t, sdata_type> ones_cache;
    auto it = ones_cache.find(width);
    if (it == ones_cache.end()) {
        sdata_type s(width);
        // Set all bits to 1 for small widths
        if (width <= 64) {
            s = (width == 64) ? UINT64_MAX : ((1ULL << width) - 1);
        } else {
            // For larger widths, set bits manually
            for (uint32_t i = 0; i < width && i < s.bv_.size(); ++i) {
                s.bv_.at(i) = true;
            }
        }
        it = ones_cache.emplace(width, std::move(s)).first;
    }
    return it->second;
}

}}} // namespace ch::core::constants
