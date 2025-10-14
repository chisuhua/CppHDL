#include "core/bool.h"
#include "core/uint.h"
#include "core/node_builder.h"
#include "core/literal.h"
#include <string>

namespace ch { namespace core {

// 从 bool 值构造
ch_bool::ch_bool(bool val, 
                 const std::string& name,
                 const std::source_location& sloc)
    : logic_buffer<ch_bool>() 
{
    ch_literal lit(val ? 1ULL : 0ULL, 1);
    auto* node = node_builder::instance().build_literal(lit, name, sloc);
    this->node_impl_ = node;
}

// 从 ch_literal 构造
ch_bool::ch_bool(const ch_literal& val,
                 const std::string& name,
                 const std::source_location& sloc)
    : logic_buffer<ch_bool>()
{
    // 验证宽度（可选）
    if (val.actual_width > 1) {
        // 可以警告或截断
    }
    auto* node = node_builder::instance().build_literal(val, name, sloc);
    this->node_impl_ = node;
}

ch_bool::operator ch_uint<1>() const {
    return ch_uint<1>(this->impl());
}

ch_bool::operator uint64_t() const {
    if (this->node_impl_ && this->node_impl_->is_const()) {
        auto* lit_node = static_cast<litimpl*>(this->node_impl_);
        return static_cast<uint64_t>(lit_node->value());
    }
    return 0;
    // 注意：运行时转换可能需要求值器，这里假设仅用于常量
    // 实际项目中可能需要更复杂的实现
    //return static_cast<bool>(static_cast<uint64_t>(*this));
}

ch_bool::operator bool() const {
    return static_cast<bool>(static_cast<uint64_t>(*this));
}

lnode<ch_bool> get_lnode(const ch_bool& bool_val) {
    return lnode<ch_bool>(bool_val.impl());
}

// make_bool_result 的定义（如果未在别处定义）
ch_bool make_bool_result(lnodeimpl* node) {
    return ch_bool(node);
}

}} // namespace ch::core
