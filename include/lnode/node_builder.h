// include/lnode/node_builder.h
#ifndef CH_LNODE_NODE_BUILDER_H
#define CH_LNODE_NODE_BUILDER_H

#include "core/node_builder.h"
#include "logger.h"
#include "traits.h"
#include <algorithm>
#include <type_traits>

namespace ch { namespace core {

// === 非核心结构定义 ===

// 优化级别枚举 - 非核心实现细节
enum class optimization_level {
    none,        // 无优化
    normal,      // 正常优化
    aggressive   // 激进优化
};

// 构建统计信息结构 - 非核心功能
struct build_statistics {
    uint32_t literals_built = 0;
    uint32_t inputs_built = 0;
    uint32_t outputs_built = 0;
    uint32_t registers_built = 0;
    uint32_t operations_built = 0;
    uint32_t total_nodes_built = 0;
    
    void reset() {
        literals_built = 0;
        inputs_built = 0;
        outputs_built = 0;
        registers_built = 0;
        operations_built = 0;
        total_nodes_built = 0;
    }
};

// === 核心实现 ===

// 单例实现
inline node_builder& node_builder::instance() {
    static node_builder instance;
    return instance;
}

// 构造函数实现
inline node_builder::node_builder() 
    : debug_mode_(false)
    , opt_level_(optimization_level::normal)
    , caching_enabled_(false)
    , statistics_enabled_(false)
    , name_prefix_("") {
}

// 配置方法实现
inline void node_builder::set_debug_mode(bool debug) { debug_mode_ = debug; }
inline void node_builder::set_optimization_level(optimization_level level) { opt_level_ = level; }
inline void node_builder::enable_caching(bool enable) { caching_enabled_ = enable; }
inline void node_builder::set_name_prefix(const std::string& prefix) { name_prefix_ = prefix; }
inline void node_builder::enable_statistics(bool enable) { statistics_enabled_ = enable; }
inline const build_statistics& node_builder::get_statistics() const { return statistics_; }
inline void node_builder::reset_statistics() { statistics_.reset(); }

// 辅助方法实现
inline uint32_t node_builder::calculate_result_size(ch_op op, uint32_t lhs_width, uint32_t rhs_width) {
    switch (op) {
        case ch_op::add:
            return std::max(lhs_width, rhs_width) + 1;
        case ch_op::sub:
        case ch_op::neg:
            return std::max(lhs_width, rhs_width);
        case ch_op::mul:
            return lhs_width + rhs_width;
        case ch_op::eq:
        case ch_op::ne:
        case ch_op::lt:
        case ch_op::le:
        case ch_op::gt:
        case ch_op::ge:
            return 1;
        case ch_op::and_:
        case ch_op::or_:
        case ch_op::xor_:
        case ch_op::shl:
        case ch_op::shr:
        case ch_op::sshr:
            return std::max(lhs_width, rhs_width);
        case ch_op::bit_sel:
            return 1;
        default:
            return std::max(lhs_width, rhs_width);
    }
}

inline std::string node_builder::prefixed_name(const std::string& name) const {
    if (name_prefix_.empty()) {
        return name;
    }
    return name_prefix_ + "_" + name;
}

// === 核心构建方法实现 ===
// ... (其余实现保持不变，如之前的代码所示)

}} // namespace ch::core

#endif // CH_LNODE_NODE_BUILDER_H
