# 提案：`get_lnode` 的入口集合收敛（实施 ADR-053）

- **架构依据**：**ADR-053**（`docs/adr/ADR-053-get-lnode-entry-set.md`，Accepted）
- **证据**：`docs/architecture/2026-10-30-n4-n6-evidence.md` §2 / §4 / §6
- **规格影响**：**新建** capability `core-lnode-entry-set`（ADDED ×1）
  ⇒ 归档**禁** `--skip-specs`；⚠️ archive **不合并** `## Purpose` ⇒ 归档后 MUST
  另行补写主 spec 的 `## Purpose`（先例：`axi-interconnect-routing`）
- **规模 / 风险**：**极小**（删 3 行 + 一个 `static_assert` 护栏 + 一个存活探针）

## Why

仓内有**两个**签名完全相同的 `get_lnode(const T&)`：

| # | 位置 | 约束 | 函数体 |
|---|---|---|---|
| ① | `include/core/lnode.h:24-28` | `requires { t.impl(); }` | `return lnode<T>(t.impl());` |
| ② | `include/core/bundle/bundle_base.h:551-553` | **无** | `return lnode<Derived>(b.impl());` |

② 存在的名义理由是「让 bundle 也能 `get_lnode`」，但**实测证明它是死的且有害**：

1. **对有 `impl()` 者「死」**：C++20「约束更严者胜」偏序 ⇒ 对**一切**有 `impl()`
   的类型（bundle / `ch_uint` / port / `ch_reg` …）① **恒胜** ⇒ ② **永不选中**
   （`bundle_base` 继承 `logic_buffer::impl()`，`bundle_base.h:67`）。
2. **对无 `impl()` 者「有害」**（两条独立证据）：
   - **错误指向框架内部**：`samples/if_demo.cpp` 实测报
     `bundle_base.h:552: error: 'multi_if<ch_uint<9>>' has no member named 'impl'`
     ⇒ 错误帧落在**框架文件**上 ⇒ 用户会误判为框架 bug；
   - **SFINAE 毒化**（更硬的理由）：② 的返回类型是**显式**的 `lnode<Derived>`
     ⇒ `requires { get_lnode(x); }` 只检查重载决议、**不实例化函数体**
     ⇒ ⇒ 对无 `impl()` 的类型探测返回 **`true`**（实测）⇒ ⇒ 任何基于 `requires`
     的能力探测都会**误判该类型「支持 get_lnode」**。

⇒ ⇒ ② **可证**「对有 `impl()` 者死、对无 `impl()` 者有害」⇒ **无第三种可能**。

## What Changes

1. **删除** `include/core/bundle/bundle_base.h:551-553`（3 行）⇒ `get_lnode` 的入口
   集合收敛为**两个**：
   | 入口 | 用途 |
   |---|---|
   | `get_lnode(const lnode<T>&)`（恒等，`lnode.h:20`） | 已是 lnode ⇒ 直接返回 |
   | `get_lnode(const T&) requires { t.impl(); }`（`lnode.h:24-28`） | **唯一**通用入口（bundle 也走它） |

   ⇒ MUST NOT 存在**无约束**的第三个入口。
2. **`include/core/lnode.h`**：在 `get_lnode` 声明处加一行注释写明「入口集合 = 2，
   MUST NOT 存在无约束模板入口」+ 引用 ADR-053 编号（**引用**，不复制论证）。
3. **护栏（可执行契约）—— MUST 有两层**：
   - **① 编译期**：`static_assert(!requires(NoImplTag x) { get_lnode(x); })`
     （落在 `tests/test_lnode_entry_set.cpp`）⇒ ⚠️ 该断言今天为假 ⇒ 加进去即转红；
   - **② 源码级扫描**：`scripts/lib/get_lnode_entry_set.py`（覆盖 `include/**` +
     `src/**` 的**全部**声明点）+ `scripts/verify_get_lnode_entry_set.sh`。

   ⚠️⚠️ **实施期实测的关键发现**：**只靠 ① 会漏报** —— ① **只能看到 fixture 自己
   include 的**那些头文件 ⇒ ⇒ 缺陷真正的所在地（`bundle_base.h`）若**没被**
   include ⇒ ⇒ **漏**（实测：恢复那个无约束重载后 ① **仍全绿**）⇒ ⇒ **② 是必需的**。
   ⇒ ⇒ 这正是本仓登记过的「**护栏自身有盲区**」病症的一个新实例。
4. **存活探针**：显式调用 `get_lnode(bundle)` 的 fixture MUST **编译通过**（走 ①）
   ⇒ 把「零合法消费者」从 **grep 结论**升级为**可执行复核**。

### 范围外

- **MUST NOT** 用 `= delete` 兜底 —— 实测 GCC / Clang 均为**硬错误**
  （`use of deleted function` / `call to deleted function`）⇒ 把「静默假阳性」
  换成「探测直接编译不过」⇒ **更坏**；
- **MUST NOT** 用函数体 `static_assert` 陷阱 —— 体不实例化 ⇒ **真正**的重新毒化；
- `bundle_base.h:546-549` 的 `ch_width_impl<Derived>` bundle 特化 ⇒ **只登记不删**
  （它是**类模板特化**，可被任意用户 TU 实例化 ⇒ 「今天没有消费者」≠「不可能有
  合法实例化」⇒ 删除判据不同）；
- E1（`multi_if` 不能直接赋给 `ch_out`）的修复 ⇒ **另一个 change**
  （`multi-if-acceptance-surface-2026-10` 的 Non-Goal #6）。

## Acceptance

- [ ] `bundle_base.h:551-553` 已删除；`get_lnode` 入口集合 = **2**；
- [ ] **V1** 存活探针：`get_lnode(bundle)` 的 fixture **编译通过**
      （破坏判据：误删 ① / 改坏其约束 ⇒ fixture 失败 ⇒ 红）；
- [ ] **V2** SFINAE 护栏：`static_assert(!requires(NoImplTag x) { get_lnode(x); })`
      ⇒ 修复后转绿；**判别性对照**：临时恢复 ② ⇒ 该断言转红；
- [ ] **V3** 诊断改善**双侧**验证：`io().x = if_then(...)` 或 `get_lnode(multi_if)`
      ⇒ 编译失败且**指向调用点** + 含约束不满足提示 ⇒ **GCC 与 Clang 各验一次**；
      **判别性对照**：恢复 ② ⇒ 报错退回指向 `bundle_base.h:552` ⇒ 证明有区分度；
- [ ] **V4** grep 复核无 `get_lnode<` 显式模板实参 / `&get_lnode` 取址用法；
- [ ] **V5** 零回归：全量构建 · `ctest -LE perf` · samples · ported examples 全绿；
      计数锚点若变 MUST 显式更新；
- [ ] 归档（**禁** `--skip-specs`；归档后 MUST 补写新 capability 的 `## Purpose`）。

## Capabilities

- **新建** `core-lnode-entry-set` ⇒ **ADDED ×1**
  「`get_lnode` 的入口集合 MUST 限定为两个，MUST NOT 有第三个无约束入口」
  + 3 个 Scenario（入口唯一性 / SFINAE 探测准确 / 诊断指向调用点）。

## Impact

- **改动文件**：`include/core/bundle/bundle_base.h`（删 3 行）· `include/core/lnode.h`
  （注释 + `static_assert`）· **新**测试 fixture · `openspec/specs/core-lnode-entry-set/`
- **爆炸半径**：**0**（决议面**不变** —— 约束偏序下 ① 对一切合法类型恒胜；
  `lnode<Bundle>` / `ch_width_v<Bundle>` 的仓内使用数均为 **0**）
- **行为变更**：`requires { get_lnode(x); }` 对无 `impl()` 的类型由 **true** 变
  **false** ⇒ 仓内零消费者；外部使用者会得到**更正确**的答案
- **风险**：**低** —— 主风险是「误删 ①」⇒ 已用 V1 存活探针钉住

## Non-Goals

与 ADR-053 §5 一致（4 条），不在本 change 重复。
