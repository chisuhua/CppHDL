# ADR-053: `get_lnode` 的入口集合收敛 —— 删除无约束的通用入口

- Status: Accepted（proposed 2026-10-30）
- Date: 2026-10-30
- Author: build-agent（architect role）
- Parent: ADR-043（死 / 误导设施物理移除 —— 零消费者直接删、无别名期）· ADR-052（`multi_if` 的接受面 —— **相邻但不重叠**：`multi_if` 的 `get_lnode` 专属重载属该侧，不属本 ADR）
- 关联 OpenSpec change：`get-lnode-entry-set-2026-10`（实施本 ADR）
- 关联 capability：**待定**（本 ADR 的契约若需载体，归 `chlib-conditional-semantics` 或新建；由 planner 判）
- 证据：`docs/architecture/2026-10-30-n4-n6-evidence.md` §2 / §4

---

## 1. Context

仓内有**两个**签名完全相同的 `get_lnode(const T&)`：

| # | 位置 | 约束 | 函数体 |
|---|---|---|---|
| ① | `include/core/lnode.h:24-28` | `requires { t.impl(); }` | `return lnode<T>(t.impl());` |
| ② | `include/core/bundle/bundle_base.h:551-553` | **无** | `return lnode<Derived>(b.impl());` |

`bundle_base` **继承**了 `logic_buffer::impl()`（`bundle_base.h:67`）⇒
⇒ ② 存在的名义理由是「让 bundle 也能 `get_lnode`」。

**但 ② 是死的**：C++20 的「**约束更严者胜**」偏序 ⇒ 对**一切**有 `impl()` 的类型
（bundle / `ch_uint` / port / `ch_reg` …）① 恒胜 ⇒ ⇒ ② **永不选中**（实测，证据 §2.2）。

**而 ② 对无 `impl()` 的类型是有害的**（实测，证据 §2.2 / §4.1）：

1. **错误指向框架内部**：`samples/if_demo.cpp` 实测报
   `bundle_base.h:552: error: 'multi_if<ch_uint<9>>' has no member named 'impl'`
   ⇒ 错误帧落在**框架文件**上 ⇒ 用户误判为框架 bug；
2. **SFINAE 毒化**（更硬的理由）：② 的返回类型是**显式**的 `lnode<Derived>`
   ⇒ `requires { get_lnode(x); }` 只检查重载决议**不实例化函数体**
   ⇒ ⇒ 对无 `impl()` 的类型探测返回 **`true`**（实测）
   ⇒ ⇒ 任何基于 `requires` 的能力探测都会**误判该类型「支持 get_lnode」**。

⇒ ⇒ ② **可证**「对有 `impl()` 者**死**、对无 `impl()` 者**有害**」⇒ **无第三种可能**。

---

## 2. Decision

**删除 ②**（`include/core/bundle/bundle_base.h:551-553`）⇒ ⇒ `get_lnode` 的入口集合
收敛为**两个**：

| 入口 | 用途 |
|---|---|
| `get_lnode(const lnode<T>&)`（恒等，`lnode.h:20`） | 已是 lnode ⇒ 直接返回 |
| `get_lnode(const T&) requires { t.impl(); }`（`lnode.h:24-28`） | **唯一**通用入口（**bundle 也走它**） |

⇒ ⇒ MUST NOT 存在**无约束**的第三个入口。

**MUST NOT** 用以下替代方案：

- 「补一个 `= delete` 的兜底重载」—— ⚠️ 它**不**毒化探测（实测：GCC 在
  `requires { get_lnode(x); }` 里对 deleted function 报**硬错误**
  `use of deleted function` ⇒ 探测**根本编译不过**，而不是静默返回 `true`）
  ⇒ ⇒ 但它把「静默假阳性」换成「**探测直接编译失败**」⇒ **更坏**：能力探测应当
  返回布尔值，而不是让调用方编译不过；且 C++26 前 `= delete` 无法携带消息
  ⇒ 诊断质量也差；
- 「函数体写 `static_assert` 陷阱」—— 函数体**不实例化** ⇒ 探测仍返回 `true`
  ⇒ ⇒ **真正**的重新毒化。

**契约载体**：`lnode.h` 的 `get_lnode` 声明处加一行注释写明「入口集合 = 2，
MUST NOT 存在无约束模板入口」，并给出本 ADR 编号（**引用**，不复制论证）。

### 2.1 选项对比（现实存在的三条）

| 选项 | 结论 | 理由 |
|---|---|---|
| **A** 加 `requires is_bundle_type<Derived>` 保留 ② | ❌ 否决 | ② 本就**永不选中**（约束偏序）⇒ 加了约束仍死 ⇒ **零收益**，却留下第二个 bundle 专用入口，将来仍可能被误认为「bundle 有特殊通道」 |
| **B** 只加 `requires { b.impl(); }`（与 ① 同约束） | ❌ 否决（**不可行**） | 签名 + 约束**完全相同** ⇒ **重定义 / 歧义** ⇒ 编译不过（实测） |
| **C** **物理删除** ② | ✅ **采纳** | ① 已完全覆盖 bundle（`bundle_base` 有 `impl()`）⇒ 删除**不改变**任何合法调用点的决议（约束偏序下 ① 恒胜）⇒ **爆炸半径 0**（证据 §2.4：`lnode<Bundle>` / `ch_width_v<Bundle>` 使用数均为 **0**） |

---

## 3. 后果 / 代价

### 正面

- **消除一个诊断陷阱**：无 `impl()` 类型的报错从「指向 `bundle_base.h:552`」变为
  「`no matching function for call to 'get_lnode(...)'` + 候选 `lnode.h:24` 带
  `constraints not satisfied … no member named 'impl'`」⇒ ⇒ 「缺 `impl`」的信息**保留**，
  但挂在**正确的候选**上、指向**用户的调用点**；
- **消除 SFINAE 毒化**：`requires { get_lnode(NoImplTag{}); }` 由 `true` 变 `false`
  ⇒ ⇒ 可写成**机器护栏**（见 §4 V2），且该断言**今天为假** ⇒ 加进去即转红 ⇒
  护栏与修复**同批**落地，将来再加无约束入口会**立即**被抓；
- Zero-Debt：删除一段**永不选中**且**有害**的代码。

### 负面

- **对 `requires` 探测是行为变更**（`true` → `false`）：若有代码依赖「探测为 true」来
  分派，会改变分支。仓内**零消费者**（证据 §2.4）；外部使用者会得到**更正确**的答案。
- 报错文本对**不熟悉 C++20 约束**的用户可能更陌生（多了一行 `constraints not satisfied`）
  ⇒ 但它**指向正确位置**，净收益为正。

### 风险

- **误删 ①** ⇒ 一切 `get_lnode` 调用失效 ⇒ ⇒ 已用 §4 V1（合法路径存活探针）钉住；
- **消息改善只是声明未见实测** ⇒ ⇒ 已用 §4 V2 要求 **GCC + Clang 双编译器**各验一次
  （两者的 constraint 诊断文本不同 ⇒ 只验一侧 = 假验收）。

---

## 4. Verification（前瞻 · 每条必须能**转红**）

- [ ] **V1** 合法路径**存活**探针：fixture 显式调用 `get_lnode(bundle)` ⇒ **编译通过**
      （走 ①）。**破坏判据**：误删 ① 或改坏其约束 ⇒ fixture 编译失败 ⇒ 红。
      （⇒ 把「零合法消费者」从 **grep 结论**升级为**可执行复核**）
- [ ] **V2** **SFINAE 毒化消除**护栏：
      `static_assert(!requires(NoImplTag x) { get_lnode(x); }, "…");`
      ⇒ 当前工作树**该断言为假** ⇒ 删除 ② 后转绿（⇒ 天然带变异判别力）；
- [ ] **V3** 诊断改善的**双侧**验证：fixture `io().x = if_then(...)` 或
      `get_lnode(multi_if)` ⇒ 编译失败且报错**指向调用点** + 含约束不满足提示 ⇒
      **GCC 与 Clang 各验一次**。**判别性对照**：临时恢复 ② ⇒ 报错退回指向
      `bundle_base.h:552` ⇒ 证明探针有区分度；
- [ ] **V4** grep 复核无 `get_lnode<` 显式模板实参 / `&get_lnode` 取址用法
      （这两类会绕过重载决议，删除 ② 后行为可能不同）；
- [ ] **V5** 零回归：全量构建 · `ctest -LE perf` · samples · ported examples 全绿；
      计数锚点若变 MUST 显式更新。

---

## 5. Non-Goals（逐条登记，防蒸发）

| # | 事项 | 处置 |
|---|---|---|
| 1 | `include/core/bundle/bundle_base.h:546-549` 的 `ch_width_impl<Derived>` bundle 特化（无消费者） | **只登记，不删**。它**不是**过载歧义（是**类模板特化**）⇒ 删除判据不同：特化可被**任意用户 TU 实例化**（`ch_width_v<MyBundle>`），「今天没有消费者」**不等于**「不可能有合法实例化」⇒ 不可据「死」推断 |
| 2 | `include/core/lnode.h:42-58` 的注释死块 | **同批删除**（进 V1 的 diff）**或**写明保留理由；不允许沉默 |
| 3 | **E1**（`multi_if` 不能直接赋给 `ch_out`）的修复 | **不在本 ADR**。修法 = `chlib` 侧加 `get_lnode(const multi_if<T>&)`（**`core/` 零改动**）。**次序：本 ADR 先于 E1** —— 中间态可接受（删除前后 `multi_if` 赋值**都**是编译期硬错误，零「编译中」代码受影响；且受约束版在约束偏序下恒胜，与 include 顺序无关）；若先做 E1，则本 ADR 移除的地雷会继续毒化**未来新类型**的能力探测 |
| 4 | `ch_width_impl<multi_if<T>>` 特化（E1 修复的配套） | 归 **E1 侧 change** |
