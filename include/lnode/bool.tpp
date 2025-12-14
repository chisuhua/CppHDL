#include "core/node_builder.h"

namespace ch::core {

// 从编译时 ch_literal_impl 构造
template <uint64_t V, uint32_t W>
ch_bool::ch_bool(const ch_literal_impl<V, W> &val, const std::string &name,
                 const std::source_location &sloc)
    : logic_buffer<ch_bool>() {
    // 验证宽度（可选）
    static_assert(
        W <= 1,
        "ch_bool can only be constructed from literals with width <= 1");
    ch_literal_runtime runtime_lit(V, W);
    auto *node =
        node_builder::instance().build_literal(runtime_lit, name, sloc);
    this->node_impl_ = node;
}

} // namespace ch::core