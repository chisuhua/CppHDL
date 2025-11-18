#ifndef CH_LNODE_REG_H
#define CH_LNODE_REG_H

#include "core/operators.h" // 确保 to_operand 可见

namespace ch { namespace core {

// --- next_assignment_proxy implementation ---
template<typename T>
template<typename U>
void next_assignment_proxy<T>::operator=(const U& value) const {
    // 使用 to_operand 统一处理所有 ValidOperand 类型
    auto operand = to_operand(value);
    lnodeimpl* src_impl = operand.impl();

    CHDBG("[next_assignment_proxy::operator=] Assigning value (node ID: %d) to regimpl node ID %d", 
          (src_impl ? static_cast<int>(src_impl->id()) : -1), 
          (regimpl_node_ ? static_cast<int>(regimpl_node_->id()) : -1));
    
    if (regimpl_node_ && src_impl && regimpl_node_->type() == lnodetype::type_reg) {
        regimpl* reg_node_impl = static_cast<regimpl*>(regimpl_node_);
        reg_node_impl->set_next(src_impl);
        CHDBG("[next_assignment_proxy::operator=] Successfully set next value");
    } else {
        CHERROR("[next_assignment_proxy::operator=] Error: regimpl_node_ is null, not a regimpl, or src_impl is null!");
    }
}

// --- ch_reg implementation ---

// Template constructor for general types (handles ch_literal, ch_uint, ch_reg, etc.)
template<typename T>
template<typename U>
ch_reg<T>::ch_reg(const U& initial_value,
                  const std::string& name,
                  const std::source_location& sloc) {
    CHDBG("  [ch_reg] Creating register with general initial value");

    // 使用 to_operand 统一转换操作数
    auto operand = to_operand(initial_value);
    lnodeimpl* init_node_impl = operand.impl();

    // 使用 node_builder 创建寄存器节点
    auto [reg_node, proxy_node] = node_builder::instance().build_register<T>(
        init_node_impl, nullptr, name, sloc
    );

    // Initialize base class (logic_buffer<T>)
    static_cast<logic_buffer<T>*>(this)->node_impl_ = proxy_node;

    // Extract regimpl node for next assignment
    if (reg_node) {
        regimpl_node_ = reg_node;
    } else {
        CHERROR("[ch_reg] Error: Could not get regimpl node!");
        regimpl_node_ = nullptr;
    }
    __next__ = std::make_unique<next_type>(regimpl_node_);

    CHDBG("  [ch_reg] Created regimpl and next_proxy.");
}

// Default constructor (no initial value)
template<typename T>
ch_reg<T>::ch_reg(const std::string& name,
                  const std::source_location& sloc) {
    CHDBG("  [ch_reg] Creating register without initial value.");

    // 使用 node_builder 创建寄存器节点（无初始值）
    auto [reg_node, proxy_node] = node_builder::instance().build_register<T>(
        nullptr, nullptr, name, sloc
    );

    static_cast<logic_buffer<T>*>(this)->node_impl_ = proxy_node;

    if (reg_node) {
        regimpl_node_ = reg_node;
    } else {
        CHERROR("[ch_reg] Error: Could not get regimpl node (no init)!");
        regimpl_node_ = nullptr;
    }
    __next__ = std::make_unique<next_type>(regimpl_node_);

    CHDBG("  [ch_reg] Created regimpl (no init) and next_proxy.");
}

// Conversion operators
template<typename T>
ch_reg<T>::operator lnode<T>() const {
    return lnode<T>(this->impl());
}

template<typename T>
lnode<T> ch_reg<T>::as_ln() const {
    return static_cast<lnode<T>>(*this);
}

}} // namespace ch::core

#endif // CH_LNODE_REG_H
