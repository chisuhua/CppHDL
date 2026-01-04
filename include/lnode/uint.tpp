// src/core/uint.cpp
#include "core/bool.h"
#include "core/context.h"
#include "core/literal.h"
#include "core/lnodeimpl.h"
#include "core/node_builder.h"
#include "core/uint.h"
#include "utils/logger.h"
#include <cstdint>
#include <source_location>
#include <string>

namespace ch {
namespace core {

template <unsigned N>
ch_uint<N>::ch_uint(const ch_literal_runtime &val, const std::string &name,
                    const std::source_location &sloc)
    requires(N > 1)
{
    CHDBG("[ch_uint<N>::ch_uint] Creating uint%d from sdata_type", N);

    this->node_impl_ = node_builder::instance().build_literal(val, name, sloc);

    if (!this->node_impl_) {
        CHERROR("[ch_uint<N>::ch_uint] Failed to create literal node from "
                "sdata_type");
    }
}

template <unsigned N>
template <uint64_t V, uint32_t W>
ch_uint<N>::ch_uint(const ch_literal_impl<V, W> &val, const std::string &name,
                    const std::source_location &sloc) {
    CHDBG("[ch_uint<N>::ch_uint] Creating uint%d from compile-time literal", N);

    static_assert(W <= N, "Literal width must not exceed target uint width");
    ch_literal_runtime runtime_lit(V, W);
    this->node_impl_ =
        node_builder::instance().build_literal(runtime_lit, name, sloc);

    if (!this->node_impl_) {
        CHERROR("[ch_uint<N>::ch_uint] Failed to create literal node from "
                "compile-time literal");
    }
}

template <unsigned N> ch_uint<N>::operator uint64_t() const {
    if (this->node_impl_ && this->node_impl_->is_const()) {
        auto *lit_node = static_cast<litimpl *>(this->node_impl_);
        return static_cast<uint64_t>(lit_node->value());
    }
    // 如果不是常量节点，返回 0
    CHERROR(
        "[ch_uint<N>::operator uint64_t] Attempting to convert non-constant "
        "node to uint64_t");
    return 0;
}

} // namespace core
} // namespace ch

// 显式实例化 - 放在命名空间外部
template class ch::core::ch_uint<1>;
template class ch::core::ch_uint<2>;
template class ch::core::ch_uint<3>;
template class ch::core::ch_uint<4>;
template class ch::core::ch_uint<5>;
template class ch::core::ch_uint<6>;
template class ch::core::ch_uint<7>;
template class ch::core::ch_uint<8>;
template class ch::core::ch_uint<9>;
template class ch::core::ch_uint<10>;
template class ch::core::ch_uint<11>;
template class ch::core::ch_uint<12>;
template class ch::core::ch_uint<13>;
template class ch::core::ch_uint<14>;
template class ch::core::ch_uint<15>;
template class ch::core::ch_uint<16>;
template class ch::core::ch_uint<32>;
template class ch::core::ch_uint<64>;
template class ch::core::ch_uint<24>;
template class ch::core::ch_uint<48>;