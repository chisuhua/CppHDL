// include/core/lnode.h
#pragma once

#include "core/traits.h"
#include "lnodeimpl.h"

namespace ch::core {

// --- lnode wrapper ---
template <typename T> struct lnode {
    lnodeimpl *impl() const { return impl_; }
    lnode(lnodeimpl *p) : impl_(p) {}

    virtual std::string to_string() const { return impl_->to_string(); }

private:
    lnodeimpl *impl_ = nullptr;
};

// --- get_lnode overloads ---
//
// ⚠️ **通用入口 = 1**（ADR-053，`docs/adr/ADR-053-get-lnode-entry-set.md`）：
//   ① 下面的**恒等**版；② 下面的**受约束**通用版。
//   ⇒ **类型专用**入口**允许**存在（如 `uint.h` 的 `get_lnode(const ch_uint<N>&)`，
//     实测**生效**）—— 它们只匹配特定类型 ⇒ **不**产生假阳性探测。
//
//   ⇒ MUST NOT 增加**无约束的通用**入口 ——
//     * 对**有** `impl()` 的类型，它**永不选中**（C++20「约束更严者胜」偏序 ⇒
//       ② 恒胜）⇒ ⇒ 纯死代码；
//     * 对**无** `impl()` 的类型，它**有害**：其返回类型**显式** ⇒
//       `requires { get_lnode(x); }` 只检查重载决议、**不实例化函数体**
//       ⇒ ⇒ 探测返回 **true**（**假阳性**的能力探测）；且报错会指向**框架内部**
//       （实测：`bundle_base.h: 'multi_if<…>' has no member named 'impl'`）
//       ⇒ ⇒ 用户误判为框架 bug。
//
// ⇒ **可执行护栏 MUST 有两层**（实施期实测的关键发现）：
//   ① **编译期**：`static_assert(!requires(no_impl_tag x) { get_lnode(x); })`
//      落在 `tests/test_lnode_entry_set.cpp`；
//   ② **源码级扫描**：`scripts/lib/get_lnode_entry_set.py`（覆盖 `include/**` +
//      `src/**` 的**全部**声明点）。
//   ⚠️ **只靠 ① 会漏报**：它**只能看到 fixture 自己 include 的**那些头文件
//      ⇒ 缺陷真正的所在地（`bundle_base.h`）若**没被** include ⇒ ⇒ **漏**
//      （实测：恢复那个无约束重载后 ① **仍全绿**）⇒ ⇒ ② 是必需的。
template <typename T> lnode<T> get_lnode(const lnode<T> &n) { return n; }

template <typename T>
lnode<T> get_lnode(const T &t)
    requires requires { t.impl(); }
{
    return lnode<T>(t.impl());
}

template <typename T> struct ch_width_impl<ch::core::lnode<T>, void> {
    static constexpr unsigned value = ch_width_v<T>;
};

// Forward declaration of the internal implementation
template <typename T> class ch_reg;

} // namespace ch::core

