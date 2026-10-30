#pragma once

#include "core/bool.h"
#include "core/context.h"
#include "core/literal.h"
#include "core/lnode.h"
#include "core/lnodeimpl.h"
#include "core/node_builder.h"
#include "core/operators.h"
#include "core/traits.h"
#include "core/uint.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace chlib {

using namespace ch::core;

// ===========================================================================
// 分支值的**类型归一化**（ADR-052 D4）
// ===========================================================================

/// port 形态检测 —— 与 `core/operators.h` 的 `to_operand` 里的特判**同款**
/// （`to_operand` 用的是 `requires { typename T::value_type; typename T::direction; }`）。
template <typename T>
concept port_like = requires(const T &t) {
    typename T::value_type;
    typename T::direction;
};

/// 分支值的**有效类型**：port ⇒ 归一化为它的 `value_type`；其余 ⇒ 原类型。
template <typename U> struct branch_value_type {
    using type = std::remove_cvref_t<U>;
};

template <typename U>
    requires port_like<std::remove_cvref_t<U>>
struct branch_value_type<U> {
    using type = typename std::remove_cvref_t<U>::value_type;
};

template <typename U>
using branch_value_type_t = typename branch_value_type<U>::type;

/// 把分支值构造成目标类型 `VT`。
///
/// ⚠️ `ch_bool` 端口**没有** `operator ch_bool()`（port 只有带约束的
/// `operator ch_uint<W>()` + 无约束的 `operator lnode<T>()`）⇒ ⇒ 走
/// `lnode` 兜底（实测可行，证据 §6.5）。
template <typename VT, typename U> VT normalize_branch(const U &v) {
    if constexpr (std::is_constructible_v<VT, const U &>) {
        return VT(v);
    } else if constexpr (requires { typename std::remove_cvref_t<U>::value_type; }) {
        return VT(lnode<typename std::remove_cvref_t<U>::value_type>(v).impl());
    } else {
        return VT(v);
    }
}

// ===========================================================================
// 多条件选择器类 - 表达式风格
// ===========================================================================
//
// ⚠️ **契约（ADR-052）**：**一句话** ——
//   「结果宽度由**第一个分支**（或显式模板实参）声明；后续分支**只能更窄或相等**；
//     需要更宽 ⇒ 在 `if_then<ch_uint<W>>(...)` 处显式声明。」
//
// 完整契约见 capability `chlib-conditional-semantics` 的
// 「`multi_if` 的接受面 MUST 完备」Requirement（**引用**，不复制 ⇒ 防漂移）。
//
// ⚠️ **接受面里每一格 MUST 要么语义正确、要么响亮拒绝**（不允许第三态）：
//   * **D1** 硬件分支宽度 MUST ≤ `width(T)`（更窄 ⇒ 零扩展；**更宽 ⇒ 编译期拒绝**）；
//   * **D2** `ch_literal_impl<V,W>` 按 `W` 走**静态**判据（sized literal）；
//           **裸算术字面量**按**值域**在**建图期**判（unsized literal ——
//           `ch_width_v<int>` = 32 是**占位语义** ⇒ 裸比较会误拒 `elif(c, 1)`）；
//   * **D3** **单分支**（无 `elif` 且无 `else_`）⇒ **拒绝**（其条件在结构上
//           不可能参与合成 —— `operator T()` 的循环对单分支零次迭代）；
//   * **D4** 首分支为 **port** ⇒ `T` **归一化**为其 `value_type`；
//           `T` 为**纯算术** ⇒ **拒绝**（「接受但不可用」比直接拒绝更坏）；
//   * **D5** `operator T()` 的返回类型 MUST **恒为 `T`**。
template <typename T> class multi_if {
private:
    struct branch_info {
        ch_bool condition;
        T value;
    };

    std::vector<branch_info> branches_;
    bool has_else_ = false;
    std::source_location creation_loc_;

    /// 整数字面量分支的**值域**检查（建图期）。
    ///
    /// ⚠️ `ch_uint<N>(IntT v)` 会**静默截断**（`256` → `0`）⇒ ⇒ 必须自己查。
    template <typename U> static T checked_integer(const U &v, const char *where) {
        using UU = std::remove_cvref_t<U>;
        if constexpr (std::is_signed_v<UU>) {
            if (v < 0) {
                CHFATAL_EXCEPTION(
                    "multi_if: %s: negative literal (%s) is not allowed; write it "
                    "as an explicit hardware value (e.g. ch_uint<N>(...))",
                    where, std::to_string(static_cast<long long>(v)).c_str());
            }
        }
        const auto uv = static_cast<uint64_t>(v);
        if constexpr (ch_width_v<T> < 64) {
            if (uv >= (uint64_t(1) << ch_width_v<T>)) {
                CHFATAL_EXCEPTION(
                    "multi_if: %s: literal (%s) does not fit into the %u-bit result "
                    "width; declare a wider result explicitly "
                    "(if_then<ch_uint<W>>(...))",
                    where, std::to_string(uv).c_str(), ch_width_v<T>);
            }
        }
        return T(v);
    }

    /// 分支值的**分派**（D1 / D2 / D4 的三分支）。
    ///
    /// ⚠️ MUST 以**模板重载**在「转换到 `T` **之前**」拦截 —— 否则实参已在
    /// 进入函数体前被转换（宽分支已被截断 ⇒ 原始宽度**不可恢复** ⇒ 无法判定
    /// 「更宽」）。
    ///
    /// ⚠️⚠️ **调用点 MUST 让 `U` 走推导** —— **不要**写 `checked_value<T>(...)`：
    ///     那个显式实参会绑定到**函数模板参数 `U`**（不是类模板参数 `T`）
    ///     ⇒ ⇒ `U` 被强制成 `T` ⇒ 形参变成 `const T&` ⇒ ⇒ **更宽的分支被隐式
    ///     窄化**（静默截断！）⇒ ⇒ **宽度判据根本没跑到** ⇒ ⇒ D1 形同虚设。
    ///     （实施期实测踩中 ⇒ 由 fixture 抓出）
    template <typename U> T checked_value(const U &value, const char *where) {
        using UU = std::remove_cvref_t<U>;
        if constexpr (HardwareType<UU> || port_like<UU>) {
            using VT = branch_value_type_t<U>;
            static_assert(ch_width_v<VT> <= ch_width_v<T>,
                          "multi_if: branch value is WIDER than the result width. "
                          "The result width is declared by the FIRST branch (or by "
                          "the explicit template argument). Declare a wider result "
                          "explicitly: if_then<ch_uint<W>>(...)");
            (void)where;
            return normalize_branch<T>(value);
        } else if constexpr (ch::core::CHLiteral<UU>) {
            // ⚠️ **D2（sized literal）**：`ch_literal_impl<V,W>` 的 `W` 是**编译期**
            //    模板参数 ⇒ 走**静态**判据（与 Verilog 的 sized literal 同构）。
            //    ⚠️ 它**不是** `HardwareType`（无 `impl()`）⇒ 必须单独一档。
            static_assert(ch_width_v<UU> <= ch_width_v<T>,
                          "multi_if: literal branch is WIDER than the result width. "
                          "Declare a wider result explicitly: "
                          "if_then<ch_uint<W>>(...)");
            (void)where;
            return T(value);
        } else if constexpr (std::is_integral_v<UU>) {
            return checked_integer(value, where);
        } else {
            static_assert(!sizeof(UU *),
                          "multi_if: branch value must be a hardware type or an "
                          "integral literal. For a port use its value type "
                          "explicitly: ch_uint<N>(io().x) / if_then<ch_uint<N>>(...)");
        }
    }

public:
    // ⚠️ **条件形参 MUST 是模板**（2026-10-30 补修，ADR-051 E3 的**延伸**）：
    //    此前形参是 `const ch_bool &` ⇒ ⇒ `ch_in<ch_bool>` **不能**绑到它
    //    ⇒ ⇒ **首分支**（经 `if_then`）与 **`elif` 分支**的条件**行为不一致**
    //    ⇒ ⇒ 「`if_then` 的条件可以是端口，但 `.elif` 的不行」= **框架内
    //    不一致**（同一能力面 ⇒ 同一限制 ⇒ MUST 齐平）。
    //    ⇒ ⇒ 条件经 **`to_operand`** 归一化 ⇒ ⇒ 端口 / 硬件类型 / 表达式
    //    **都**可用。
    //    ⇒ ⇒ 归一化**不新建节点**（`to_operand` 对硬件类型走 `get_lnode`
    //    ⇒ 取现有节点）⇒ ⇒ 零额外节点 ⇒ ⇒ 既有行为**逐位不变**。
    template <typename C, typename U>
    multi_if(const C &condition, const U &value,
             std::source_location sloc = std::source_location::current())
        : creation_loc_(sloc) {
        static_assert(ch::core::HardwareType<T>,
                      "multi_if: T must be a hardware type. A plain arithmetic type "
                      "would make the result unusable (operator T() returns T, and "
                      "T cannot be converted back into a hardware value). Declare the "
                      "width explicitly: if_then<ch_uint<N>>(condition, value)");
        ch_bool cond(to_operand(condition).impl());
        branches_.push_back({cond, checked_value(value, "first branch")});
    }

    template <typename C, typename U>
    multi_if &elif(const C &condition, const U &value) {
        if (has_else_) {
            throw std::runtime_error("Cannot add elif after else");
        }
        ch_bool cond(to_operand(condition).impl());
        branches_.push_back({cond, checked_value(value, "elif")});
        return *this;
    }

    template <typename U>
    multi_if &else_(const U &value) {
        if (has_else_) {
            throw std::runtime_error("Else branch already added");
        }
        branches_.push_back({ch_bool(true), checked_value(value, "else_")});
        has_else_ = true;
        return *this;
    }

    // 转换操作符：将条件语句转换为T类型的值
    operator T() const {
        // ⚠️ **D3**：单分支 ⇒ 其条件在结构上**不可能**参与合成
        //    （下面的循环从 `rbegin() + 1` 起 ⇒ 对单分支是**空循环**
        //    ⇒ 生成的 Verilog 里**零 mux** ⇒ 条件被彻底忽略）。
        //    ⇒ **拒绝**（不降级为「恒等」—— 表达式形式没有「否则」的语义对象）。
        if (branches_.size() < 2) {
            CHFATAL_EXCEPTION(
                "multi_if: a single branch cannot be a conditional expression - its "
                "condition can never participate (no mux is built, so the result "
                "would silently ignore the condition). Add .else_(value) or another "
                ".elif(condition, value)");
        }

        // 从后往前构建嵌套的select语句
        T current_value = branches_.back().value;

        // 从倒数第二个开始向前处理
        for (auto it = branches_.rbegin() + 1; it != branches_.rend(); ++it) {
            // ⚠️ **D5**：结果类型 MUST 恒为 `T`
            //    （`select` 在 `result_width <= 1` 时返回 `ch_bool` ⇒ 须收敛回 `T`）
            current_value = static_cast<T>(
                ::ch::core::select(it->condition, it->value, current_value));
        }

        return current_value;
    }

    // 便捷方法：获取最终值
    T value() const { return static_cast<T>(*this); }
};

// 便捷函数：创建if-then-else表达式
//
// ⚠️ **D4**：`T` 未显式给出时 ⇒ 由**首分支**推导 ⇒ 首分支是 **port** 时
//    **归一化**为它的 `value_type`（与 `select` / `to_operand` 的既有 port 特判
//    一致 ⇒ 消除「`select` 能用 port 而 `if_then` 不能」的框架内不一致）。
//
// ⚠️ **条件参数 MUST 是模板**（E3 的修复）—— 此前形参是 `const ch_bool &` ⇒ ⇒
//    `ch_in<ch_bool>` **不能**绑到它 ⇒ ⇒ `if_then(io().en, x)` **编译不过**
//    ⇒ ⇒ 而 `select` / `_if` 对端口**都可用** ⇒ ⇒ 框架内不一致。
//    ⇒ ⇒ 现在条件经 **`to_operand`** 归一化 ⇒ ⇒ 端口 / 硬件类型 / 表达式
//    **都**可用 ⇒ ⇒ 与 `_if`（语句块形式）**可表达性对齐**。
//
// ⚠️ 归一化**不新建节点**：`to_operand` 对硬件类型走 `get_lnode`（取现有节点）
//    ⇒ ⇒ 零额外节点 ⇒ ⇒ 既有行为**逐位不变**。
template <typename T = void, typename U, typename C>
auto if_then(const C &condition, const U &value) {
    // ⚠️ `to_operand` 对 port 走 `lnode<value_type>(value.impl())` 分支
    //    ⇒ 对 `ch_in<ch_bool>` 得到 `lnode<ch_bool>` ⇒ `.impl()` 即其节点。
    //    ⇒ ⚠️ 归一化**不新建节点** ⇒ ⇒ 与 `multi_if` 内部那次归一化
    //    （`multi_if` 的 ctor / `elif` 也做同样归一化）**幂等**
    //    ⇒ ⇒ 零额外节点 ⇒ ⇒ 既有行为**逐位不变**。
    ch_bool cond(to_operand(condition).impl());
    if constexpr (std::is_same_v<T, void>) {
        return multi_if<branch_value_type_t<U>>(cond, value);
    } else {
        return multi_if<T>(cond, value);
    }
}

// ⚠️ **ADR-051 D8**：`priority_if` / `priority_if_then` 已**物理移除** ——
//    它们与 `multi_if` 的 `operator T()` **逐行等价**（两者都从后往前构链
//    ⇒ **先匹配者胜**）⇒ ⇒ 「优先级 / 互斥优化」的承诺**未兑现**
//    ⇒ ⇒ 名字 over-promise ⇒ 依 Zero-Debt + ADR-043 先例（零消费者 ⇒ 直接删、
//    无 deprecation 别名期）⇒ ⇒ `multi_if` / `if_then` 是**唯一**的表达式形式。

} // namespace chlib

// ===========================================================================
// E1 的修复：`multi_if` MUST 能**直接赋给 `ch_out`**
// ===========================================================================
//
// ⚠️ **缺陷形态**：`ch_out<T>::operator=(const U &)` 走 `get_lnode(value)`
//    ⇒ ⇒ 而 `multi_if<T>` **无** `impl()` ⇒ ⇒ **无匹配重载** ⇒ ⇒
//    `io().out = if_then(c, a).else_(b);` **编译不过**
//    （实测：`include/core/io.h:115: error: no matching function for call to
//    'get_lnode(const chlib::multi_if<ch::core::ch_uint<9> >&)'`）。
//    ⇒ ⇒ 而 `select` 对同类操作数**可用** ⇒ ⇒ 框架内不一致。
//
// ⚠️⚠️ **为什么必须写在 `namespace chlib`（而不是 `ch::core`）**：
//    `core/io.h` 的 `get_lnode(value)` 是**非限定**调用 ⇒ ⇒ 候选集 =
//    「定义点的普通查找」+「**实例化点的 ADL**」⇒ ⇒ `multi_if` 在 `chlib`
//    ⇒ ⇒ ADL 会找到 `chlib::get_lnode` ⇒ ⇒ **核心层零改动**。
//    ⇒ ⚠️ 写成 `namespace ch::core { ... get_lnode ... }` ⇒ ⇒ ADL **找不到**
//    ⇒ ⇒ 修复**失效**（实测踩过）。
//
// ⚠️ **形参 MUST 是**具体类型**（`const multi_if<T> &`）⇒ ⇒ 它是**类型专用**
//    入口 ⇒ ⇒ **不**违反 ADR-053（「MUST NOT 有**无约束的通用**入口」）。
//
// ⚠️ **为什么需要 `ch_width_impl` 特化**：`lnode<T>` 的 `ch_width_impl`
//    （`include/core/lnode.h:38`）用 `ch_width_v<T>` ⇒ ⇒ `multi_if<T>` 必须
//    有宽度 ⇒ ⇒ 结果类型宽度 = `ch_width_v<T>`（与 `multi_if` 的结果类型一致）。
//
// ⚠️ **语义**：`get_lnode(multi_if)` MUST 触发 `multi_if` 的**合成**
//    （`operator T()`）⇒ ⇒ 返回的节点就是**合成后的值** ⇒ ⇒ 与
//    `ch_uint<8> v = if_then(...); io().out = v;` **逐位等价**。

namespace ch::core {
// ⚠️ 特化 MUST 在 `ch::core`（主模板所在命名空间）⇒ ⇒ 在 `chlib` 内部写
//    `namespace ch::core { ... }` **不成立** ⇒ ⇒ 必须写在**文件末尾**。
template <typename T>
struct ch_width_impl<::chlib::multi_if<T>, void> {
    static constexpr unsigned value = ch_width_v<T>;
};
} // namespace ch::core

namespace chlib {
/// `multi_if` → `lnode`（**类型专用**入口；见上文的 E1 说明）
template <typename T>
inline ::ch::core::lnode<multi_if<T>> get_lnode(const multi_if<T> &m) {
    // ⚠️ `operator T()` 是**建图期**的合成（它遍历分支体并合成一条 select 链）
    //    ⇒ ⇒ 这里调用它就得到**合成后的节点**。
    return ::ch::core::lnode<multi_if<T>>(static_cast<T>(m).impl());
}
} // namespace chlib
