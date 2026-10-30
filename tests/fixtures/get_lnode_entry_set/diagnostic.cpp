// tests/fixtures/get_lnode_entry_set/diagnostic.cpp
//
// ⚠️ 本 fixture MUST **编译失败** —— 它核验 ADR-053 的**诊断质量**条款：
//     「无匹配时的诊断 MUST 指向**调用点** + 列出候选并说明**约束不满足**；
//      MUST NOT 把错误帧指向**框架内部**实现文件」。
//
// 机制：这里在**非依赖**上下文里直接调用 `get_lnode`（一个无 `impl()` 的类型）
// ⇒ ⇒ 编译器 MUST 报**硬错误**（这正是用户看到的东西）⇒ ⇒ 护栏 grep 该错误文本：
//     * MUST 含「impl」（⇒ 说明约束不满足的原因被讲清楚了）；
//     * MUST **不含** `bundle_base.h`（⇒ 说明错误**没有**指向框架内部）。
//
// ⚠️ **判别性**：若有人把那个**无约束**的 `get_lnode` 加回来 ⇒ 重载决议会选中它
//     ⇒ 错误变成「`multi_if<…>` has no member named `impl`」并**指向
//     `bundle_base.h:552`** ⇒ ⇒ 护栏的「MUST NOT 含 `bundle_base.h`」判据
//     **转红** ⇒ ⇒ 无需改动源码即可完成 V3' 的判别性对照。

// ⚠️ **必须 include 完整头文件集合**（见 guard_ok.cpp 的同款说明）
#include "ch.hpp"

namespace {

/// 无 `impl()` 的类型 —— 触发「无匹配重载」
struct no_impl_tag {};

} // namespace

void probe() {
    // ⚠️ 这一行 MUST 编译失败
    (void)ch::core::get_lnode(no_impl_tag{});
}
