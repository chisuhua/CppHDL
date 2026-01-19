#include "core/bool.h"
#include "ast/ast_nodes.h"
#include "core/literal.h"
#include "core/node_builder.h"
#include "core/uint.h"
#include <string>

namespace ch {
namespace core {

// 从 bool 值构造
ch_bool::ch_bool(bool val, const std::string &name,
                 const std::source_location &sloc)
    : logic_buffer<ch_bool>() {
    ch_literal_runtime lit(val ? 1ULL : 0ULL, 1);
    auto *node = node_builder::instance().build_literal(lit, name, sloc);
    this->node_impl_ = node;
}

// 从运行时字面量构造
ch_bool::ch_bool(const ch_literal_runtime &val, const std::string &name,
                 const std::source_location &sloc)
    : logic_buffer<ch_bool>() {
    // 先创建字面值节点
    auto *literal_node =
        node_builder::instance().build_literal(val, name + "_literal", sloc);

    // 然后使用 assign 操作创建一个新的节点
    if (literal_node) {
        this->node_impl_ = node_builder::instance().build_unary_operation(
            ch_op::assign, lnode<ch_bool>(literal_node), 1, name, sloc);
    } else {
        CHERROR("[ch_bool::ch_bool] Failed to create literal node from "
                "sdata_type");
        this->node_impl_ = nullptr;
    }

    if (!this->node_impl_) {
        CHERROR("[ch_bool::ch_bool] Failed to create assign node from "
                "sdata_type");
    }
}
ch_bool::operator uint64_t() const {
    if (this->node_impl_ && this->node_impl_->is_const()) {
        auto *lit_node = static_cast<litimpl *>(this->node_impl_);
        return static_cast<uint64_t>(lit_node->value());
    }
    return 0;
}

ch_bool::operator bool() const {
    return static_cast<bool>(static_cast<uint64_t>(*this));
}
/*
lnode<ch_bool> get_lnode(const ch_bool& bool_val) {
    return lnode<ch_bool>(bool_val.impl());
}
*/

// 删除重复定义的make_bool_result函数，因为它已经在operators.h中内联定义了
// ch_bool make_bool_result(lnodeimpl* node) {
//     return ch_bool(node);
// }

// 显式转换为 bool
bool ch_bool::to_bool() const {
    return static_cast<bool>(static_cast<uint64_t>(*this));
}

// 支持流输出
std::ostream &operator<<(std::ostream &os, const ch_bool &b) {
    os << (b ? "true" : "false");
    return os;
}

} // namespace core
} // namespace ch