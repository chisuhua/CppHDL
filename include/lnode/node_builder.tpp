// include/lnode/node_builder.tpp
#ifndef CH_LNODE_NODE_BUILDER_TPP
#define CH_LNODE_NODE_BUILDER_TPP

#include "core/literal.h"
#include "logger.h"
#include "traits.h"
#include <algorithm>
#include <type_traits>
#include <bit>

namespace ch::core {

// 辅助函数实现
namespace {
    // 直接在匿名命名空间中定义 get_literal_width 函数
    template <typename T>
    requires std::is_integral_v<T> && std::is_unsigned_v<T>
    constexpr uint32_t get_literal_width(T value) {
        if (value == 0) return 1;
        return static_cast<uint32_t>(std::bit_width(value));
    }

    template <typename T>
    requires std::is_integral_v<T> && std::is_signed_v<T>
    constexpr uint32_t get_literal_width(T value) {
        using U = std::make_unsigned_t<T>;
        if (value == 0) return 1;
        return get_literal_width(static_cast<U>(value));
    }

    uint32_t calculate_result_size(ch_op op, uint32_t lhs_width, uint32_t rhs_width) {
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

    std::string prefixed_name_helper(const std::string& name, const std::string& prefix) {
        if (prefix.empty()) {
            return name;
        }
        return prefix + "_" + name;
    }
}

// 模板方法实现

// 常量节点构建实现
template<typename T>
lnodeimpl* node_builder::build_literal(T value, 
                                      const std::string& name,
                                      const std::source_location& sloc) {
    CHDBG_FUNC();
    if (instance().debug_mode_) {
        CHINFO("[node_builder] Building literal with value %s", name.c_str());
    }
    
    auto* ctx = ctx_curr_;
    if (!ctx) {
        CHERROR("[node_builder] No active context for literal creation");
        return nullptr;
    }
    
    if constexpr (std::is_arithmetic_v<T>) {
        // 处理算术字面值
        uint32_t width = get_literal_width(value);
        sdata_type sval(static_cast<uint64_t>(value), width);
        
        if (instance().statistics_enabled_) {
            ++instance().statistics_->literals_built;
            ++instance().statistics_->total_nodes_built;
        }
        
        CHDBG("[node_builder] Building literal with value %llu, width %u", 
              static_cast<unsigned long long>(value), width);
        return ctx->create_literal(sval, prefixed_name_helper(name, instance().name_prefix_), sloc);
    } else if constexpr(is_ch_literal_v<T>) {
        sdata_type sdata(value.value, value.actual_width); // 直接构造

        if (instance().statistics_enabled_) {
            ++instance().statistics_->literals_built;
            ++instance().statistics_->total_nodes_built;
        }
        
        return ctx->create_literal(sdata, prefixed_name_helper(name, instance().name_prefix_), sloc);
    } else {
        // 处理 sdata_type
        if (instance().statistics_enabled_) {
            ++instance().statistics_->literals_built;
            ++instance().statistics_->total_nodes_built;
        }
        
        CHDBG("[node_builder] Building literal from sdata_type");
        return ctx->create_literal(value, prefixed_name_helper(name, instance().name_prefix_), sloc);
    }
}

// 输入节点构建实现
template<typename T>
lnodeimpl* node_builder::build_input(const std::string& name,
                                    const std::source_location& sloc) {
    CHDBG_FUNC();
    if (instance().debug_mode_) {
        CHINFO("[node_builder] Building input '%s'", name.c_str());
    }
    
    auto* ctx = ctx_curr_;
    if (!ctx) {
        CHERROR("[node_builder] No active context for input creation");
        return nullptr;
    }
    
    uint32_t size = ch_width_v<T>;
    if (instance().statistics_enabled_) {
        ++instance().statistics_->inputs_built;
        ++instance().statistics_->total_nodes_built;
    }
    
    CHDBG("[node_builder] Building input with size %u, name '%s'", size, name.c_str());
    return ctx->create_input(size, prefixed_name_helper(name, instance().name_prefix_), sloc);
}

// 输出节点构建实现
template<typename T>
lnodeimpl* node_builder::build_output(const std::string& name,
                                     const std::source_location& sloc) {
    CHDBG_FUNC();
    if (instance().debug_mode_) {
        CHINFO("[node_builder] Building output '%s'", name.c_str());
    }
    
    auto* ctx = ctx_curr_;
    if (!ctx) {
        CHERROR("[node_builder] No active context for output creation");
        return nullptr;
    }
    
    uint32_t size = ch_width_v<T>;
    if (instance().statistics_enabled_) {
        ++instance().statistics_->outputs_built;
        ++instance().statistics_->total_nodes_built;
    }
    
    CHDBG("[node_builder] Building output with size %u, name '%s'", size, name.c_str());
    return ctx->create_output(size, prefixed_name_helper(name, instance().name_prefix_), sloc);
}

// 寄存器节点构建实现
template<typename T>
std::pair<regimpl*, proxyimpl*> node_builder::build_register(
    lnodeimpl* init_val,
    lnodeimpl* next_val,
    const std::string& name,
    const std::source_location& sloc) {
    
    CHDBG_FUNC();
    auto* ctx = ctx_curr_;
    if (!ctx) {
        CHERROR("[node_builder] No active context for register creation");
        return {nullptr, nullptr};
    }
    
    uint32_t size = ch_width_v<T>;
    if (instance().statistics_enabled_) {
        ++instance().statistics_->registers_built;
        ++instance().statistics_->total_nodes_built;
    }
    
    CHDBG("[node_builder] Building register with size %u, name '%s'", size, name.c_str());
    
    // Check if we have a default clock
    core::clockimpl* default_clk = ctx->get_default_clock();
    
    // 构建寄存器节点
    regimpl* reg_node = ctx->create_node<regimpl>(
        size, 0, default_clk, nullptr, nullptr, nullptr, init_val,
        prefixed_name_helper(name, instance().name_prefix_), sloc
    );
    
    // 构建代理节点
    proxyimpl* proxy_node = ctx->create_node<proxyimpl>(reg_node, prefixed_name_helper("_" + name, instance().name_prefix_), sloc);
    
    // Explicitly link register and proxy nodes
    if (reg_node && proxy_node) {
        reg_node->set_proxy(proxy_node);
    }
    
    // 设置 next 值
    if (next_val && reg_node) {
        reg_node->set_next(next_val);
        CHDBG("[node_builder] Set next value for register");
    }
    
    return {reg_node, proxy_node};
}

template<typename Cond, typename TrueVal, typename FalseVal>
lnodeimpl* node_builder::build_mux(
    const lnode<Cond>& cond,
    const lnode<TrueVal>& true_val,
    const lnode<FalseVal>& false_val,
    const std::string& name,
    const std::source_location& sloc) {
    
    constexpr unsigned result_width = std::max(ch_width_v<TrueVal>, ch_width_v<FalseVal>);
    
    auto* mux_node = ctx_curr_->create_node<muximpl>(
        result_width,
        cond.impl(),
        true_val.impl(),
        false_val.impl(),
        name,
        sloc
    );
    
    return mux_node;
}

// 二元操作节点构建实现
template<typename T, typename U>
lnodeimpl* node_builder::build_operation(
    ch_op op,
    const lnode<T>& lhs,
    const lnode<U>& rhs,
    bool is_signed,
    const std::string& name,
    const std::source_location& sloc) {
    
    CHDBG_FUNC();
    if (instance().debug_mode_) {
        CHINFO("[node_builder] Building operation %d", static_cast<int>(op));
    }
    
    auto* ctx = ctx_curr_;
    if (!ctx) {
        CHERROR("[node_builder] No active context for operation creation");
        return nullptr;
    }
    
    if (!lhs.impl() || !rhs.impl()) {
        CHERROR("[node_builder] Invalid operand for operation");
        return nullptr;
    }
    
    uint32_t result_size = calculate_result_size(op, ch_width_v<T>, ch_width_v<U>);
    if (instance().statistics_enabled_) {
        ++instance().statistics_->operations_built;
        ++instance().statistics_->total_nodes_built;
    }
    
    CHDBG("[node_builder] Building operation %d with result size %u", 
          static_cast<int>(op), result_size);
    
    // 构建操作节点
    opimpl* op_node = ctx->create_node<opimpl>(
        result_size, op, is_signed, lhs.impl(), rhs.impl(),
        prefixed_name_helper(name, instance().name_prefix_), sloc
    );
    
    // 构建代理节点
    proxyimpl* proxy_node = ctx->create_node<proxyimpl>(op_node, prefixed_name_helper("_" + name, instance().name_prefix_), sloc);
    
    return proxy_node;
}

// 重载 build_operation 支持一元操作
template<typename T>
lnodeimpl* node_builder::build_operation(
    ch_op op,
    const lnode<T>& operand,
    bool is_signed,
    const std::string& name,
    const std::source_location& sloc) {
    
    CHDBG_FUNC();
    if (instance().debug_mode_) {
        CHINFO("[node_builder] Building unary operation %d", static_cast<int>(op));
    }
    
    auto* ctx = ctx_curr_;
    if (!ctx) {
        CHERROR("[node_builder] No active context for operation creation");
        return nullptr;
    }
    
    if (!operand.impl()) {
        CHERROR("[node_builder] Invalid operand for unary operation");
        return nullptr;
    }
    
    uint32_t result_size = calculate_result_size(op, ch_width_v<T>, 0); // 0表示一元操作
    if (instance().statistics_enabled_) {
        ++instance().statistics_->operations_built;
        ++instance().statistics_->total_nodes_built;
    }
    
    // 构建一元操作节点
    opimpl* op_node = ctx->create_node<opimpl>(
        result_size, op, is_signed, operand.impl(), nullptr, // rhs = nullptr
        prefixed_name_helper(name, instance().name_prefix_), sloc
    );
    
    proxyimpl* proxy_node = ctx->create_node<proxyimpl>(op_node, prefixed_name_helper("_" + name, instance().name_prefix_), sloc);
    return proxy_node;
}
/*
// 一元操作节点构建实现
template<typename T>
lnodeimpl* node_builder::build_unary_operation(
    ch_op op,
    const lnode<T>& operand,
    const std::string& name,
    const std::source_location& sloc) {
    
    CHDBG_FUNC();
    return build_operation(op, operand, operand, false, name, sloc);
}
*/

// 在 node_builder.h 或相关实现文件中
template<typename T>
lnodeimpl* node_builder::build_unary_operation(
    ch_op op,
    const lnode<T>& operand,
    const std::string& name,
    const std::source_location& sloc) {
    
    CHDBG_FUNC();
    if (instance().debug_mode_) {
        CHINFO("[node_builder] Building unary operation %d", static_cast<int>(op));
    }
    
    auto* ctx = ctx_curr_;
    if (!ctx) {
        CHERROR("[node_builder] No active context for operation creation");
        return nullptr;
    }
    
    if (!operand.impl()) {
        CHERROR("[node_builder] Invalid operand for unary operation");
        return nullptr;
    }
    
    // 对于规约操作，结果宽度总是1
    uint32_t result_size = 1;
    if (instance().statistics_enabled_) {
        ++instance().statistics_->operations_built;
        ++instance().statistics_->total_nodes_built;
    }
    
    // 构建真正的一元操作节点（rhs = nullptr）
    opimpl* op_node = ctx->create_node<opimpl>(
        result_size, op, false, operand.impl(), nullptr,
        prefixed_name_helper(name, instance().name_prefix_), sloc
    );
    
    proxyimpl* proxy_node = ctx->create_node<proxyimpl>(op_node, prefixed_name_helper("_" + name, instance().name_prefix_), sloc);
    return proxy_node;
}
} // namespace ch::core

#endif // CH_LNODE_NODE_BUILDER_TPP
