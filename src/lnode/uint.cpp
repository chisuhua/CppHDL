#include "uint.h"
#include "context.h"
#include "ast_nodes.h"
#include <string>

namespace ch { namespace core {

// 从 uint64_t 构造函数实现
template<unsigned N>
ch_uint<N>::ch_uint(uint64_t val, const std::source_location& sloc) {
    auto* ctx = Context::get();
    if (ctx) {
        uint32_t id = ctx->next_id();
        sdata_type sval(val, N);
        auto* lit_node = new litimpl(id, sval, "lit_" + std::to_string(id), sloc, ctx);
        ctx->add_node(lit_node);
        this->node_impl_ = lit_node;
    }
}

// 从 sdata_type 构造函数实现
template<unsigned N>
ch_uint<N>::ch_uint(const sdata_type& val, const std::source_location& sloc) {
    auto* ctx = Context::get();
    if (ctx) {
        uint32_t id = ctx->next_id();
        auto* lit_node = new litimpl(id, val, "lit_" + std::to_string(id), sloc, ctx);
        ctx->add_node(lit_node);
        this->node_impl_ = lit_node;
    }
}

// 显式转换为 uint64_t
template<unsigned N>
ch_uint<N>::operator uint64_t() const {
    if (this->node_impl_ && this->node_impl_->is_const()) {
        auto* lit_node = static_cast<litimpl*>(this->node_impl_);
        return static_cast<uint64_t>(lit_node->value());
    }
    return 0;
}

}} // namespace ch::core

