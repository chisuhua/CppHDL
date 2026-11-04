# 设计：归属真源（`hierarchical-rtl-ownership-fix-2026-11`）

> 决策在 ADR-059（§2.1 核心裁决 · D1 / D2′ / D2″ / D6 / D7 / D8 / D9 / D10）；
> 本文件承载**推导与证据**。
> ⚠️ 本 change 是**归属真源**变更 ⇒ ⇒ 它**耦合**到 simulator 的指令表分流与
> `io.h` 的四处方向守卫 ⇒ ⇒ MUST 单独验收（⇒ 与 ①a / ①c 的回归面**互不相同**）。
## 1. 为什么先修归属（推导）

### 1.1 归属错误的三个可观测形态（实测）

| 形态 | 实测 | 根因 |
|---|---|---|
| 成员的 `ch_reg` 归**父**组件 | `top.m.r <- parent=top.m`（r 实属 Leaf） | 建节点时 `Component::current()` 还是父组件（成员构造早于 `build()`） |
| 成员 `ch_module` 挂**祖父** | `top.children = 4`，含两个同名 `top.leaf` | 同上 |
| 深度 ≥3 **前缀重复** | `top.top.m.lf.*` | `build_hierarchical_name()` 用 `hierarchical_name()`（后者又拼一次已是全路径的 `name_`） |

### 1.2 为什么「ctor 进 / build 出」是错的（推导）

| 路径 | `ctor 进 / build 出` 的后果 |
|---|---|
| `build()` 被调用两次（`built_` 早退，`src/component.cpp:99-103`） | 第二次会**多弹一帧** ⇒ 静默错归属 |
| `create_child` **从不 build**（`include/component.h:43-59`） | 该组件**活着期间**长期占据栈顶 ⇒ 后续**每一个**节点归属被污染 |
| `describe()` 期间（祖先尚未 build） | 栈**非空** ⇒ 「回落 `current_`」这条分支**不触发** ⇒ 归属取到构造中的祖先 ⇒ **恰好把要修的错重新引入** |

⇒ ⇒ **配对作用域**（ctor 进 / ctor 出）+ `build_internal` 另压一层 BUILD 作用域
⇒ ⇒ 四种形态全部正确，且**无泄漏**（⇒ 不需要析构兜底）。

### 1.3 为什么 `ch_module` 的 context 必须取环境值（推导）

`Component::ctx_` 在**构造函数里是 `nullptr`**（`src/component.cpp:13`），只在 `build()` 里赋值
⇒ ⇒ 成员形态 `ch_module` 的「新父组件」在构造期**没有 context**
⇒ ⇒ `include/module.h:49-53` 早退 ⇒ ⇒ **子组件直接消失** ⇒ ⇒ `io()` 时 `CHFATAL` ⇒ SIGSEGV。

⇒ ⇒ 今日之所以没炸，是因为 `Component::current()` 在那个窗口指向**祖父**（祖父已 build ⇒ 有 context）
⇒ ⇒ ⇒ **「挂祖父」这个缺陷恰好掩盖了「context 未就绪」** ⇒ ⇒ ⇒
⇒ ⇒ 改 parent 而不改 context 来源 ⇒ ⇒ **把「挂错层」升级成「整个子模块不存在」**（更严重且同样静默）。

⇒ ⇒ **修法**：`ch_module` 的 build context 取**环境 context**（`ch::core::ctx_curr_`）⇒ ⇒
- `describe()` 内创建时 = 父的 context（**与今天相同** ⇒ 零行为变化）；
- 成员形态时 = 环境 context（父的构造发生在祖父的 ctx 作用域内 ⇒ 仍是**同一个** context）
  ⇒ ⇒ **成员形态可用**，且**不需要延迟构造**（不改变节点创建顺序 ⇒ 爆炸半径最小）。

⇒ ⇒ **为什么不需要「延迟构造」**：延迟构造也能解决 context 问题，但它会**改节点创建顺序**
⇒ ⇒ 全部 Verilog 文本变 ⇒ ⇒ 「扁平产物逐字节不变」判据失效 ⇒ ⇒ ⇒ 故取环境 context 方案。

---

### 风险（自 umbrella §7 分派）

| # | 风险 | 对策 |
|---|---|---|
| 1 | 归属修错方向 ⇒ 整棵子树归错父（**比现状更难定位**） | 本 change 的判据（归属↔语义分流回归）+ 架构文档附录 A 的探针入仓为断言 |
| 6 | 下游 ODR 缺陷未修 ⇒ 多 TU 项目链接失败 | ✅ **已解决** —— `header-odr-inline-fix-2026-11`（公共头 ODR）**已于 2026-11-04 归档** |

---

## 4. 实施期偏离记录（**文档 MUST 与代码一致**）

| # | ADR 的推导 | 实施期实测 | 处置（决策未变） |
|---|---|---|---|
| 1 | D6①「`Component::Component()` 压入 CONSTRUCT 作用域，**构造函数出口**弹出（配对）」 | **基类构造先于派生类成员构造** ⇒ 基类里的局部守卫会在**成员构造之前**析构 ⇒ 作用域帧根本盖不到成员 | 新增 `Component::ConstructionFrame`（RAII **截断**）⇒ 由**唯一知道「构件造完」**的三方（`ch_device` / `ch_module` / `create_child`）在 `make_shared`/`make_unique` **返回后**截断。⇒ 直接构造（三者之外）的帧保留到析构（`~Component` 兜底 `erase_scope_frame()`）⇒ **已知限制**：该对象存活期间「组件外建的节点」归它 |
| 2 | D9「`add_child()` MUST 补绑 `child->parent_ = this->weak_from_this()`」 | 成员形态 `ch_module` 调用 `add_child` 时**宿主自己还在构造中** ⇒ 宿主的 `weak_from_this()` 为空 ⇒ 补绑**不生效** | `add_child` 仍补绑（覆盖 `create_child` 等宿主已托管的路径）**且**在宿主 `build()` 里再补一次（`rebind_children_parents()` —— 那时 `shared_ptr` 已接管）⇒ 两处合起来覆盖全部路径 |
| 3 | D10「`context::init()` 建 clock/reset MUST 用『无归属』哨兵」（未指定落点） | 把哨兵做成 `Component` 的嵌套类型 ⇒ `src/core/context.cpp` 需 include `component.h` ⇒ 带入 `core/io.h` 的 `ch::core::core` 别名 ⇒ **破坏该 TU 既有的 `core::clockimpl` 限定名**（编译失败） | 提到 core 侧：`core::NodeOwnershipSuppression` + `node_ownership_suppressed()`（`include/core/context.h`，定义在 `src/core/context.cpp`）；`Component::node_owner()` 查询它 |

### 4.1 实施期确认的两条边界（不是偏离，是**范围界定**）

1. **环境 ctx 为空时，子组件的「成员节点」建不出来** —— 节点创建需要 active ctx
   ⇒ ⇒ D2′ 的推迟机制只保证「子组件的 **build**（端口 / describe）不丢」，
   不保证「无 ctx 时也能构造含 `ch_reg` 的组件」。⇒ 判据用**无成员节点**的子组件。
2. **in-tree 无「构造在 ctx 内却用 `build()`（无 external ctx）自建 ctx」的调用**
   ⇒ ⇒ D2″ 分支 ① 的守卫不会误伤现存代码；该形态从「静默丢子模块」变成**响亮拒绝**。

### 4.2 文本变化面（ADR-059 D5 允许：语义等价、文本有变）

**构造期**建的成员节点名现在带组件全路径（`_reg` → `top.mid.leaf._reg`，
`reg_3` → `top.mid.leaf.reg_3`）⇒ ⇒ Verilator 的 SHA-1 缓存 key 全失效
⇒ 首次 `ctest -LE perf` 的墙钟从 **357 → 714 sec*proc**（**冷编译**，非性能回归；
零测试失败）。
