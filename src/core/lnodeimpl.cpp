// src/core/lnodeimpl.cpp
#include "lnodeimpl.h" // 确保包含定义了 ch_lnode_type_count 的头文件
#include <array>
#include <string_view>
// #include <iostream> // 如果需要调试，可以取消注释

namespace ch { namespace core {

// --- Helper macro to generate names (similar to enum definition) ---
#define CH_LNODE_NAME(n) #n,

// --- Static array of names, indexed by lnodetype ---
// --- 使用 constexpr 函数计算大小 ---
static constexpr std::size_t LNODE_TYPE_NAMES_SIZE = ch_lnode_type_count() + 1; // +1 for sentinel

static constexpr std::array<std::string_view, LNODE_TYPE_NAMES_SIZE> lnode_type_names = {{
    CH_LNODE_ENUM(CH_LNODE_NAME)
    "invalid_type" // Sentinel for out-of-bounds access
}};

#undef CH_LNODE_NAME

const char* to_string(lnodetype type) {
    auto index = static_cast<std::size_t>(type);
    if (index < lnode_type_names.size()) {
        return lnode_type_names[index].data();
    }
    return lnode_type_names.back().data(); // Return sentinel name
}

}} // namespace ch::core
