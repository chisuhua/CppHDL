---
name: clint-mtime-autoincrement-2026-10
priority: P2
source: riscv-mini-test-triage-2026-10（实测挖出的真实 RTL 缺口）
phase: phase-5
category: cpu
type: bugfix
created_at: 2026-10-24
---

# Improvement: CLINT 的 `mtime` 必须自动递增

- **Date drafted**: 2026-10-24
- **Drafted by**: build-agent (builder role)
- **Status**: 📋 5-segment draft，已归属 feature `feat-riscv-mini-rtl-debt` ②（2026-10-26 roadmap 审计）；无前置 ⇒ 可立项
- **来源**: change `riscv-mini-test-triage-2026-10` 实测挖出（C1 修 `test_interrupt_flow`
  的断言时发现），**原登记在该 change 的「框架缺陷」节** ⇒ 本 draft 承接之，C1 归档时移除该条。

## 1. Why (背景与动机)

`include/cpu/riscv/clint.h` 的 `mtime` 是 RISC-V 规范里的**自由运行计数器**
（machine timer），软件靠读它做时基。当前实现里它**只能由软件通过写端口显式写入**
⇒ 只构成「**静态比较器**」，不是定时器。

**Why now**：C1 为 `test_interrupt_flow` 补真实断言时发现该缺口。C1 的整改已把
`clint.h` 的文件头注释改成**如实描述**（不再声称「每个 `tick()` 递增」），并删掉了
那个**空实现**的 `tick()` 与 2 个恒返 0 的占位 getter。

⚠️ 该缺口**不阻塞** C1（C1 的职责是让测试跑起来，不是补 RTL 功能）⇒ 单独立项。

## 2. What Changes (变更范围)

- **递增机制**：`mtime` 在每个时钟沿递增（64 位）。频率 MUST 可配置
  （`chlib::Clint` 的模板参数或 `Config` struct）。
- **写端口语义**：当前 `mtime` 可被软件写 ⇒ 递增与软件写**冲突**。
  RISC-V 规范允许软件写 `mtime`（用于校时）⇒ 裁决：**写优先，写后从写值继续递增**。
- **`mtimecmp`**：保持可写（R/W，reset = 全 1）。
- **MTIP 语义**：`mtime >= mtimecmp` ⇒ `mtip` 拉高（现有比较逻辑不变，
  但因 `mtime` 现在会动 ⇒ 原有测试的期望值 MUST 重新推导）。
- **`tick()`**：**不恢复**那个空实现 —— 递增 MUST 由 RTL（`ch_reg` 的 `next`）实现，
  而非 C++ 侧的方法。

## 3. Acceptance (验收标准)

- [ ] `mtime` 在连续 N 个时钟沿后按配置频率递增（可测：读 `mtime` 的差值）
- [ ] 软件写 `mtime` 后，递增从写值继续
- [ ] `mtimecmp` 的比较与 `mtip` 输出在 `mtime` 递增下保持正确
- [ ] `test_interrupt_flow` 的期望值同步更新且仍绿
- [ ] 新增用例覆盖「递增」与「写后继续」两条
- [ ] `clint.h` 的文件头注释与实际行为一致（不再有「声称」与「实现」不符）

## 4. Capabilities (涉及的能力维度)

- `cpu-cache-subsystem`（CLINT 属 CPU 外设；若无更贴切的 capability，则归
  `rv32i-canonical-home`）
- ⚠️ 需确认归属 capability：CLINT 目前**不在**任何 capability 的覆盖范围内

## 5. Impact (影响面分析)

- **改动文件**：`include/cpu/riscv/clint.h`（+ 可能新增 `clint_config`）
- **受影响测试**：`examples/riscv-mini/tests/test_interrupt_flow.cpp`
  （期望值需重新推导）
- **风险**：**中** —— `mtime` 从「恒定」变成「会动」⇒ 任何依赖 `mtime` 稳定的
  断言都会变红（这正是要修的语义，但需逐条核对）
- **与 C2/C3 的关系**：C2（`rv32i-canonical-convergence`）会动 `include/cpu/**`；
  ⇒ **建议在 C2 之后实施**，避免同一批文件被两个 change 反复改
- **强前置**：无（可独立实施）；**建议时序**：C2 之后


---

## 6. 立项前的**决策门**（2026-10-27 补）

⚠️ **本项 MUST 先有 ADR-049 才能立项为 openspec change** ⇒ **当前没有** change 目录
（`openspec/changes/` 里**不**登记本项）⇒ 理由：

1. **契约变更**：`mtime` 从「静态比较器」变「自由运行计数器」⇒ **必然**要改 capability
   ⇒ 而 **CLINT 目前不在任何 capability 的覆盖范围内**
   （实测：`grep -rn "clint" openspec/specs/*/spec.md` 只在 `verilator-rv32i-e2e` 里
   有一句「本路径不触发」）⇒ **归属 capability 本身是待裁决项**
   ⇒ delta 无法在 ADR 落盘前写；
2. **禁 `--skip-specs`**：本项**有**契约变更 ⇒ 若先登记 change 并标 `skip_specs: true`
   ⇒ 归档会把契约变更**静默丢掉**（正是根 `AGENTS.md` 明令禁止的那类错误）；
3. ⇒ **正确顺序**：architect 落 **ADR-049**（裁决下列 D1–D4 + capability 归属）
   → planner 建 change（带 delta）→ builder 实施。

### ADR-049 MUST 裁决的 4 项（+ 1 项归属）

| # | 决策 | 选项 | 倾向（供裁决） |
|---|---|---|---|
| D1 | **递增频率** | 每拍 / 可配置分频 | 可配置分频（默认每拍 ⇒ 测试可写死期望） |
| D2 | **软件写 `mtime` 与递增的优先级** | 写优先 / 递增优先 | 写优先 ⇒ 从写值继续（RISC-V 允许校时） |
| D3 | **`mtime` 复位值** | 0 / 其它 | 0（与 `mtimecmp` 的全 1 形成对比 ⇒ 中断默认不触发） |
| D4 | **32 位原子读** | `mtimeh`/`mtimel` 分离端口 / 允许撕裂 | 允许撕裂（RISC-V 允许 hi/lo 两读）⇒ **不**提供分离端口（记为已知边界） |
| D5 | **capability 归属** | 新建 `rv32i-clint-timer` / 归 `rv32i-canonical-home` | 新建 capability（CLINT 是**外设**，语义独立） |

⚠️ D1–D3 **直接决定** `test_interrupt_flow` 的期望值 ⇒ MUST 在 ADR 里写死。
