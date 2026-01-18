#ifndef CH_LNODE_NODE_BUILDER_TPP
#define CH_LNODE_NODE_BUILDER_TPP

#include "lnodeimpl.h"
#include "logger.h"
#include <algorithm>
#include <bit>
#include <type_traits>

namespace ch::core {

// 辅助函数实现

// We need to remove functions that have undefined operations
// Only keep the build_operation functions and the build_bit_select we added

template <typename T, typename U>
lnodeimpl *node_builder::build_operation(ch_op op, const lnode<T> &lhs,
                                         const lnode<U> &rhs,
                                         uint32_t result_width, bool is_signed,
                                         const std::string &name,
                                         const std::source_location &sloc) {
    CHDBG_FUNC();
    auto *ctx = ctx_curr_;
    if (!ctx) {
        CHERROR(
            "[node_builder] No active context for binary operation creation");
        return nullptr;
    }

    if (!lhs.impl() || !rhs.impl()) {
        CHERROR("[node_builder] Invalid operands for binary operation");
        return nullptr;
    }

    if (instance().statistics_enabled_) {
        ++instance().statistics_->operations_built;
        ++instance().statistics_->total_nodes_built;
    }

    // Create the operation node
    opimpl *op_node = ctx->create_node<opimpl>(
        result_width, op, is_signed, lhs.impl(), rhs.impl(),
        instance().prefixed_name_helper(name, instance().name_prefix_), sloc);

    // Create proxy node
    proxyimpl *proxy_node = ctx->create_node<proxyimpl>(
        op_node,
        instance().prefixed_name_helper("_" + name, instance().name_prefix_),
        sloc);

    return proxy_node;
}

template <typename T>
lnodeimpl *node_builder::build_unary_operation(
    ch_op op, const lnode<T> &operand, uint32_t result_width,
    const std::string &name, const std::source_location &sloc) {
    CHDBG_FUNC();
    auto *ctx = ctx_curr_;
    if (!ctx) {
        CHERROR(
            "[node_builder] No active context for unary operation creation");
        return nullptr;
    }

    if (!operand.impl()) {
        CHERROR("[node_builder] Invalid operand for unary operation");
        return nullptr;
    }

    if (instance().statistics_enabled_) {
        ++instance().statistics_->operations_built;
        ++instance().statistics_->total_nodes_built;
    }

    // Create the unary operation node
    opimpl *op_node = ctx->create_node<opimpl>(
        result_width, op, false, // is_signed usually irrelevant for unary ops
        operand.impl(),
        nullptr, // no second operand
        instance().prefixed_name_helper(name, instance().name_prefix_), sloc);

    // Create proxy node
    // proxyimpl *proxy_node = ctx->create_node<proxyimpl>(
    //     op_node, instance().prefixed_name_helper("_" + name,
    //     instance().name_prefix_), sloc);

    // return proxy_node;
    return op_node;
}

template <typename T>
lnodeimpl *node_builder::build_operation(ch_op op, const lnode<T> &operand,
                                         uint32_t result_width, bool is_signed,
                                         const std::string &name,
                                         const std::source_location &sloc) {
    CHDBG_FUNC();
    auto *ctx = ctx_curr_;
    if (!ctx) {
        CHERROR(
            "[node_builder] No active context for unary operation creation");
        return nullptr;
    }

    if (!operand.impl()) {
        CHERROR("[node_builder] Invalid operand for unary operation");
        return nullptr;
    }

    if (instance().statistics_enabled_) {
        ++instance().statistics_->operations_built;
        ++instance().statistics_->total_nodes_built;
    }

    // Create the unary operation node
    opimpl *op_node = ctx->create_node<opimpl>(
        result_width, op, is_signed, operand.impl(), nullptr,
        instance().prefixed_name_helper(name, instance().name_prefix_), sloc);

    // Create proxy node
    proxyimpl *proxy_node = ctx->create_node<proxyimpl>(
        op_node,
        instance().prefixed_name_helper("_" + name, instance().name_prefix_),
        sloc);

    return proxy_node;
}

} // namespace ch::core

#endif // CH_LNODE_NODE_BUILDER_TPP
