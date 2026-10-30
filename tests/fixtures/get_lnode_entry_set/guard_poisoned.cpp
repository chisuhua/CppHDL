// tests/fixtures/get_lnode_entry_set/guard_poisoned.cpp
//
// ⚠️ 本 fixture MUST **编译失败** —— 它是 ADR-053 契约的**判别性对照**：
//     「证明那个静态断言**真的有判别力**」。
//
// 机制：这里**故意**注入一个**无约束的通用** `get_lnode` 入口（模拟「有人又加
// 了一个」）⇒ ⇒ `can_get_lnode<no_impl_tag>` 变成 **`true`** ⇒ ⇒ `guard_ok.cpp`
// 里那条 `static_assert(!can_get_lnode<no_impl_tag>)` MUST **转红**。
//
// ⚠️ 若这份 fixture **编译通过** ⇒ ⇒ 说明那条静态断言**恒真**（或判据失效）
//     ⇒ ⇒ 护栏本身没有判别力 ⇒ MUST 视为**违规**。
//
// ⚠️ 注入的重载**刻意不调用 `t.impl()`** —— 这样它对 `no_impl_tag` 也是**可用**的
//     ⇒ ⇒ 探测返回 `true`（而不是因函数体非法而硬错误）⇒ ⇒ 失败点**唯一**
//     ⇒ ⇒ 错误消息 MUST 指向本文件里的 `static_assert`（而非别处）。

// ⚠️ **必须 include 完整头文件集合**（见 guard_ok.cpp 的同款说明）
#include "ch.hpp"

namespace ch::core {

// ⚠️ **故意注入**的无约束通用入口（ADR-053 禁止的形态）
template <typename T> lnode<T> get_lnode(const T &) { return lnode<T>(nullptr); }

} // namespace ch::core

namespace {

/// 无 `impl()` 的类型 —— 探测目标
struct no_impl_tag {};

} // namespace

template <typename T>
concept can_get_lnode = requires(T x) { ch::core::get_lnode(x); };

// ⚠️ 本断言 MUST **失败**（因为上面注入了一个无约束通用入口）
static_assert(!can_get_lnode<no_impl_tag>,
              "MUTATION_DETECTED: 无约束的通用 get_lnode 入口存在"
              " ⇒ 契约判据**有判别力**（本 fixture 的编译失败是**预期**结果）");
