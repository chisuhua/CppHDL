---
name: get-lnode-entry-set-2026-10
priority: P1
source: ADR-053（`get_lnode` 的入口集合收敛）
phase: phase-5
category: core
type: bugfix
created_at: 2026-10-30
---

# Improvement: 删除无约束的 `get_lnode` 入口

- **Date drafted**: 2026-10-30
- **Drafted by**: build-agent（planner role）
- **Status**: ✅ **已立项** ⇒ change `openspec/changes/get-lnode-entry-set-2026-10/`
- **架构依据**：ADR-053（Accepted）· 证据 `docs/architecture/2026-10-30-n4-n6-evidence.md` §2/§4
- **归属 feature**：**待裁决**（core 层 API 收敛，不属于现有 feature）⇒ 暂挂 phase-5

## 1. Why (背景与动机)

仓内有**两个**签名完全相同的 `get_lnode(const T&)`：
`lnode.h:24-28`（**有** `requires { t.impl(); }`）与 `bundle_base.h:551-553`（**无**约束）。

② 的名义理由是「让 bundle 也能 `get_lnode`」，但**实测证明它死的且有害**：

1. **对有 `impl()` 者「死」**：C++20「约束更严者胜」偏序 ⇒ ① 对**一切**有 `impl()`
   的类型恒胜 ⇒ ② **永不选中**（`bundle_base` 继承 `logic_buffer::impl()`）；
2. **对无 `impl()` 者「有害」**（两条独立证据）：
   - **错误指向框架内部**：`samples/if_demo.cpp` 实测报
     `bundle_base.h:552: 'multi_if<ch_uint<9>>' has no member named 'impl'`
     ⇒ 用户会误判为框架 bug；
   - **SFINAE 毒化**（更硬）：② 返回类型**显式** ⇒ `requires { get_lnode(x); }`
     **不实例化函数体** ⇒ 对无 `impl()` 的类型返回 **`true`** ⇒ 能力探测**假阳性**。

⇒ ⇒ ② **可证**「对有 `impl()` 者死、对无 `impl()` 者有害」⇒ **无第三种可能**。

## 2. What Changes (变更范围)

- **删** `include/core/bundle/bundle_base.h:551-553`（3 行）⇒ 入口集合 = **2**；
- `include/core/lnode.h`：加「入口集合 = 2」注释（引用 ADR-053）+ 可执行护栏
  `static_assert(!requires(NoImplTag x) { get_lnode(x); })`；
- **V1** 存活探针（`get_lnode(bundle)` MUST 编译通过）；
- 契约：**新建** capability `core-lnode-entry-set`（ADDED ×1）。

⚠️ **MUST NOT** 用 `= delete` 兜底（实测两编译器均为**硬错误** ⇒ 把「静默假阳性」
换成「探测直接编译不过」⇒ **更坏**）或函数体 `static_assert` 陷阱（体不实例化
⇒ **真正**的重新毒化）。

## 3. Acceptance (验收标准)

- [ ] ② 已删；入口集合 = 2；
- [ ] **V1** 存活探针编译通过；**V2** SFINAE 护栏转绿（恢复 ② ⇒ 转红）；
- [ ] **V3** 诊断双侧（GCC + Clang）；**V3'** 判别性对照（恢复 ② ⇒ 报错退回指向
      `bundle_base.h:552`）；
- [ ] **V4** grep 无 `get_lnode<` / `&get_lnode`；
- [ ] **V5** 零回归；归档**禁** `--skip-specs`；
- [ ] ⚠️ **归档后补写**新 capability 的 `## Purpose`（archive **不合并** delta 的 Purpose）。

## 4. Capabilities (涉及的能力维度)

**新建** `core-lnode-entry-set`（ADDED ×1 + 4 Scenario）⇒ 归档**禁** `--skip-specs`。

## 5. Impact (影响面分析)

- **改动文件**：`include/core/bundle/bundle_base.h`（删 3 行）· `include/core/lnode.h`
  · 新 fixture · `openspec/specs/core-lnode-entry-set/`；
- **爆炸半径**：**0**（约束偏序下 ① 对一切合法类型恒胜；`lnode<Bundle>` /
  `ch_width_v<Bundle>` 的仓内使用数均为 **0**）；
- **行为变更**：`requires` 探测对无 `impl()` 类型由 `true` 变 `false`（仓内零消费者）；
- **风险**：**低** —— 主风险「误删 ①」已用 V1 钉住。

## 6. 边界

- `ch_width_impl<Derived>` 的 bundle 特化（`bundle_base.h:546-549`）⇒ **只登记不删**
  （类模板特化可被任意用户 TU 实例化 ⇒ 删除判据不同）；
- **E1**（`multi_if` 不能直接赋给 `ch_out`）⇒ **另一个 change**；
  **次序：本项先于 E1**（否则无约束版会继续毒化未来新类型的探测）。
