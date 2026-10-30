---
name: multi-if-acceptance-surface-2026-10
priority: P0
source: ADR-052（`multi_if` 的接受面五形态裁决）
phase: phase-5
category: chlib
type: bugfix
created_at: 2026-10-30
---

# Improvement: `multi_if` 的接受面 —— 五种形态逐一处置

- **Date drafted**: 2026-10-30
- **Drafted by**: build-agent（planner role）
- **Status**: ✅ **已立项** ⇒ change
  `openspec/changes/multi-if-acceptance-surface-2026-10/`
- **架构依据**：ADR-052（Accepted）· 证据 `docs/architecture/2026-10-30-n4-n6-evidence.md`
- **归属 feature**：**待裁决**（与 `feat-dual-backend-coverage` 的「双后端」主题不直接相关；
  本项属 chlib 语义正确性）⇒ 暂挂 phase-5

## 1. Why (背景与动机)

`chlib::multi_if<T>` / `if_then` 是 ADR-051 确立的**唯一**表达式形式条件构造。
实测其接受面存在**五种**「框架放行了却无法正确执行」的形态：

| # | 形态 | 实测 | 严重度 |
|---|---|---|---|
| **A** | 首分支窄、后续宽 | 编译通过，**静默丢高位**（`assign top_bits = top__add_add[7:0]`） | 高 |
| **B** | **单分支**（无 `elif`/`else_`） | 编译通过，**条件被完全忽略** | **最高** |
| **C** | `T` 推导成 **port 类型** | 编译失败（模板深渊） | 中 |
| **D** | `T` = **纯算术类型** | ctor 放行、转换处失败（**两次**用户定义转换） | 中 |
| **E** | 显式 `T` **过窄** | 编译通过，**静默截断** | 高 |

⚠️ **形态 B 是「条件被忽略」缺陷家族的第三个实例**（`_if` 双重失效 / `uart_tx` FSM）.

**假绿机制**：截断在**进入 mux 之前**插入 ⇒ 两后端**同构** ⇒ parity **结构性失明**；
`WIDTHTRUNC` 又被**全局关闭**（`codegen_verilog.cpp:299-300`，理由正当不可撤）
⇒ 唯一能「变响」的位置是 C++ 前端。

⚠️ **规范层假绿**：capability 的 `#### Scenario: 表达式形式的分支值 MUST 同类型`
其 `THEN` 写「类型推导 MUST 失败（无法编译）」⇒ **已被实测证伪**。

## 2. What Changes (变更范围)

- `include/chlib/if.h`：`ctor`/`elif`/`else_` **模板化** + 三条判据
  （宽度静态 / 字面量值域 / port 归一化）+ 单分支拒绝（`CHFATAL_EXCEPTION`）；
- **新** `scripts/verify_multi_if_width.sh`（5 步机制，含**中性化对照** + fail-closed），
  挂 CI 两处；
- 契约：`chlib-conditional-semantics` **MODIFIED ×1 + ADDED ×1**。

## 3. Acceptance (验收标准)

- [ ] D1–D6 逐条落地，五形态各自被处置；
- [ ] 护栏 5 步机制 + **每个「拒绝」fixture 配中性化对照**；
- [ ] V7 positive 清单全绿（六种写法 ⇒ 防过度实现）；
- [ ] V4-a / V4-c 在 **GCC 与 Clang 各编一次**；
- [ ] 变异 6 条（M1–M6）全被抓；如实登记「zext 换恒等存储」**不红**；
- [ ] `ctest -LE perf` / samples（含 `conditional_demo` 自检）/ ported examples 全绿；
- [ ] 计数锚点同步；归档**禁** `--skip-specs`。

## 4. Capabilities (涉及的能力维度)

`chlib-conditional-semantics` ⇒ **MODIFIED ×1 + ADDED ×1** ⇒ 归档**禁** `--skip-specs`。

## 5. Impact (影响面分析)

- **改动文件**：`include/chlib/if.h` · 新护栏 · 测试 · spec（归档合并）· `.rddf/**`；
- **爆炸半径**：**小**（活消费者仅 `samples/conditional_demo.cpp`，在 D6 positive 清单内）；
- ⚠️ **破坏性变更**（外部使用者）：**B/D/E** 从「能编译」变「拒绝」；
  **C** 从「编译失败」变「可用」；
- **风险**：**中** —— 主风险是「过度实现」（更宽⇒拒绝 误写成 不等⇒拒绝）
  ⇒ 已用 D6 + V7 钉住。
