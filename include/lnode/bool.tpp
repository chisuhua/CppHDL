#include "core/node_builder.h"

namespace ch::core {

// 从编译时 ch_literal_impl 构造
template <uint64_t V, uint32_t W>
inline ch_bool::ch_bool(const ch_literal_impl<V, W> &val,
                        const std::string &name,
                        const std::source_location &sloc)
    : logic_buffer<ch_bool>() {
    // 验证宽度（可选）
    static_assert(
        W <= 1,
        "ch_bool can only be constructed from literals with width <= 1");
    ch_literal_runtime runtime_lit(V, 1); // 确保宽度为1
    auto *literal_node = node_builder::instance().build_literal(
        runtime_lit, name + "_literal", sloc);

    // 然后使用 assign 操作创建一个新的节点
    if (literal_node) {
        this->node_impl_ = node_builder::instance().build_unary_operation(
            ch_op::assign, lnode<ch_bool>(literal_node), 1, name, sloc);
    } else {
        CHERROR("[ch_bool::ch_bool] Failed to create literal node from "
                "compile-time literal");
        this->node_impl_ = nullptr;
    }

    if (!this->node_impl_) {
        CHERROR("[ch_bool::ch_bool] Failed to create assign node from "
                "compile-time literal");
    }
}

} // namespace ch::core