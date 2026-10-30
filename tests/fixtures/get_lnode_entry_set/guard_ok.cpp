// tests/fixtures/get_lnode_entry_set/guard_ok.cpp
//
// ⚠️ 本 fixture MUST **编译通过** —— 它是 ADR-053 契约的**可执行**判据：
//     「无约束的**通用** `get_lnode` 入口 MUST NOT 存在」。
//
// 判据机制：`requires { get_lnode(x); }` 对**无** `impl()` 的类型必须求值为
// `false`。若存在**无约束**的通用入口，其返回类型**显式** ⇒ 重载决议成立 ⇒
// 函数体**不实例化** ⇒ 探测返回 **`true`**（假阳性）⇒ ⇒ 本 fixture 编译失败。
//
// ⚠️ **必须放在依赖上下文**（模板参数 `T`）：非依赖的 requires-expression 里，
//     若表达式非法，编译器 MUST 报**硬错误**（而非求值为 `false`）。

// ⚠️ **必须 include 完整头文件集合**：缺陷（无约束重载）曾住在
//     `core/bundle/bundle_base.h` ⇒ 若 fixture 只 include `lnode.h`，
//     那个重载**不可见** ⇒ ⇒ 护栏会**漏报**（实测踩中）。
#include "ch.hpp"

namespace {

/// 无 `impl()` 的类型 —— 探测目标
struct no_impl_tag {};

} // namespace

template <typename T>
concept can_get_lnode = requires(T x) { ch::core::get_lnode(x); };

// ⚠️ 核心判据：无 `impl()` 的类型 MUST **无法**被 `get_lnode` 解析
static_assert(!can_get_lnode<no_impl_tag>,
              "无约束的通用 get_lnode 入口 MUST NOT 存在"
              "（它对无 impl() 的类型产生假阳性 requires 探测）");

// ⚠️ **正向对照**：否则断言可能恒真 ⇒ 失去判别力
static_assert(can_get_lnode<ch::core::ch_uint<8>>,
              "受约束的通用入口 MUST 仍能解析有 impl() 的类型");
