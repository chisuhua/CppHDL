#include "lnodeimpl.h"
#include "component.h"

#include <array>
#include <string_view>

namespace ch::core {

#define CH_LNODE_NAME(n) #n,
static constexpr std::size_t LNODE_TYPE_NAMES_SIZE = ch_lnode_type_count();
static constexpr std::array<std::string_view, LNODE_TYPE_NAMES_SIZE>
    lnode_type_names = {{CH_LNODE_ENUM(CH_LNODE_NAME)}};

#undef CH_LNODE_NAME

const char *to_string(lnodetype type) {
    auto index = static_cast<std::size_t>(type);
    if (index < lnode_type_names.size()) {
        return lnode_type_names[index].data();
    }
    return "unknown";
}
lnodeimpl::lnodeimpl(uint32_t id, lnodetype type, uint32_t size, context *ctx,
                     const std::string &name, const std::source_location &sloc)
    : id_(id), type_(type), size_(size), ctx_(ctx), sloc_(sloc) {
    // ADR-059 D1：节点归属的**唯一真源** —— 最内层「构造 / build 作用域」的组件。
    // ⚠️ MUST NOT 用 `Component::current()`：它在**构造期**指向构造中的祖先
    // （或 nullptr）⇒ 成员建的节点会被划进**父/祖父**（层次化输出按归属切模块
    // 会把子模块状态划进父模块）。
    parent_ = ch::Component::node_owner();
    if (parent_ != nullptr) {
        name_ = parent_->name() + "." + name;
    } else {
        name_ = name;
    }
}

ch::Component *lnodeimpl::current_component() {
    // ADR-059 D7：与归属**同源**（`include/core/io.h` 的 4 处方向守卫用它）
    // ⇒ ⇒ 守卫与归属 MUST NOT 分叉（否则构造期接线的组件上会**误报**）。
    return ch::Component::node_owner();
}

} // namespace ch::core
