# Design: get-lnode-entry-set-2026-10

> 实施 ADR-053。**决策在 ADR**，本文只承载实施形态与验证机制。
> 逐字证据见 `docs/architecture/2026-10-30-n4-n6-evidence.md` §2 / §4 / §6。

## 1. 改动面（极小）

### 1.1 删除（`include/core/bundle/bundle_base.h:551-553`）

```cpp
// ✗ 删除
template <typename Derived> inline lnode<Derived> get_lnode(const Derived &b) {
    return lnode<Derived>(b.impl());
}
```

⚠️ **同一段**的 `ch_width_impl<Derived>` bundle 特化（`:546-549`）**保留**
（ADR-053 §5 Non-Goal 1：它是**类模板特化**，删除判据不同）。

### 1.2 注释 + 护栏（`include/core/lnode.h`）

⚠️ **实施期订正**：初稿写「入口集合 = 2」⇒ ⇒ 实测活声明是 **3** 个
（`uint.h:197-200` 的 `get_lnode(const ch_uint<N>&)` **实测生效**）⇒ ⇒ 契约
精确化为「**通用**入口 = 1」+「MUST NOT 有无约束的**通用**入口」。

在 `get_lnode` 声明处补：

```cpp
// --- get_lnode overloads ---
// ⚠️ **入口集合 = 2**（ADR-053）：① 下面的恒等版；② 受约束的通用版。
//    MUST NOT 增加**无约束**的第三个通用入口 —— 它对有 impl() 的类型永不选中
//    （C++20 约束更严者胜），对无 impl() 的类型则产生**假阳性** requires 探测
//    （返回类型显式 ⇒ requires 不实例化函数体）。见 ADR-053。
template <typename T> lnode<T> get_lnode(const lnode<T> &n) { return n; }

// 可执行护栏：对无 impl() 的类型探测 MUST 为 false
namespace detail {
struct no_impl_tag {};   // 无 impl()
static_assert(!requires(no_impl_tag x) { get_lnode(x); },
    "ADR-053: 无约束的 get_lnode 入口 MUST NOT 存在"
    "（它对无 impl() 的类型产生假阳性 requires 探测）");
} // namespace detail
```

⚠️ **位置要求**：`static_assert` MUST 在**两个**入口都声明**之后**（否则探测不到
`lnode.h` 的版本 ⇒ 断言恒真 ⇒ 失去判别力）。

⚠️⚠️ **但编译期判据有结构性上限**：它**只能看到 fixture 自己 include 的**那些
头文件 ⇒ ⇒ 缺陷真正的所在地（`bundle_base.h`）若**没被** include ⇒ ⇒ **漏报**
（实测：恢复那个无约束重载后，编译期判据**仍全绿**）⇒ ⇒ **必须**配一个
**源码级扫描**（`scripts/lib/get_lnode_entry_set.py`）⇒ ⇒ 二者**同批**落地。

## 2. 验证机制（每条必须能**转红**）

| # | 机制 | 破坏它 ⇒ 什么红 |
|---|---|---|
| **V1** | 存活探针（独立 TU）：`get_lnode(bundle)` **必须编译通过** | 误删 ① 或改坏其约束 ⇒ 该 TU 编译失败 ⇒ 红 |
| **V2** | `lnode.h` 里的 `static_assert(!requires(...))` | 恢复 ② ⇒ 断言转红（**今天该断言为假** ⇒ 加进去即红 ⇒ 删除后转绿） |
| **V3** | 诊断双侧：对 `io().x = if_then(...)` / `get_lnode(multi_if)` 编译并 grep 报错文本 ⇒ **GCC 与 Clang 各一次** | 恢复 ② ⇒ 报错退回指向 `bundle_base.h:552` ⇒ 文本 grep 失败 ⇒ 红 |
| **V4** | grep：无 `get_lnode<` 显式模板实参 / 无 `&get_lnode` 取址 | 出现这类用法 ⇒ 绕过重载决议 ⇒ 删除后行为可能不同 ⇒ 红 |
| **V5** | 零回归（全量构建 + ctest + samples + examples） | 任何回归 ⇒ 红 |

⚠️ **V3 的判别性对照是核心**：只验「删除后消息更好」是**声明**；必须验「恢复后
消息变差」才证明探针有区分度。

⚠️ **V2 的证据等级说明**：该 `static_assert` **本身**就是护栏与契约载体 ——
它在修复前**必然**失败，因此实施 MUST 按「先删 ② 再加断言」的顺序（或一次提交
两者），否则中间态编译不过。

## 3. 实施顺序（**关键**）

1. **同一次提交**内完成「删除 ② + 加 `static_assert` + 加注释」——
   ⚠️ 若先加断言再删，中间态**编译失败**（断言为假）；若只删不加，则**无护栏**；
2. 加 V1 存活探针；
3. V3 双侧诊断验证（GCC / Clang）；
4. V4 grep 复核；
5. V5 零回归；
6. 归档 + 补写新 capability 的 `## Purpose`。

## 4. 已知边界（如实登记）

- **`ch_width_impl<Derived>` 的 bundle 特化**（`:546-549`）无消费者 ⇒ **只登记不删**
  （类模板特化可被任意用户 TU 实例化 ⇒ 「今天没有消费者」≠「不可能有合法实例化」）；
- **`lnode.h:42-58` 的注释死块** ⇒ 同批删除（进 V1 的 diff）或写明保留理由；
- **E1 的修复**（`get_lnode(const multi_if<T>&)` 专属重载 + `ch_width_impl<multi_if<T>>`
  特化）⇒ **不在本 change**。**次序：本 change 先于 E1** —— 中间态可接受
  （删除前后 `multi_if` 赋值**都**是编译期硬错误）；若先做 E1，无约束版会继续
  毒化**未来新类型**的能力探测。
