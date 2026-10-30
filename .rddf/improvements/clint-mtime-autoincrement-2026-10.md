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
- **Status**: ✅ **已实施**（2026-10-30）⇒ change `clint-mtime-autoincrement-2026-10`
  （**新建** capability `cpu-clint-timer`，ADDED ×6 ⇒ 归档**禁** `--skip-specs`）
  ⇒ ⚠️ **实施期实测新发现第 3 条陷阱**（ADR-049 §1.4 未载）：
  `bits_update<32>` 在 **64 位 target** 上**返回 64 位值** ⇒ 用 32 位接收 ⇒
  **静默截断** ⇒ 「写高半」**完全失效**（探针实测：写 hi=7 ⇒ mtime 仍 `0x0`）
  ⇒ ⇒ 已写入 capability 契约（Requirement 6 + Scenario）
  ⇒ ⇒ ⚠️ **读时序的三个实测事实**（决定了全部用例的期望值模型）：
  ① `read_data` 是**组合**输出 ⇒ 其值在**时序更新之后**求值 ② **读的那一拍本身
  会让 `mtime` 递增** ⇒ ⇒ **任何** tick 都推进分频相位 ③ ⇒ ⇒ **「写-读同拍」
  读到精确的写值**（写的那一拍不递增）⇒ ⇒ **消除时序脆弱性**的关键手段
  ⇒ ⇒ ✅ **判别性用例 8 个**（跨 32 位边界 / 分频相位 / 相位不被写复位 / 写拍落
  递增边界 / 写优先三项 / 写后继续 / 复位后 `mtip=0`）
  ⇒ ⇒ ✅ **变异 7/7 全被抓**（每条**先确认编译成功**）
  ⇒ ⇒ ⚠️ **如实登记**：ADR-049 §5.4 的 M2（手写 `hi|lo` 漏移位）是**形态 A 专属**
  ⇒ 形态 B 下**该路径不存在** ⇒ 改用等效变异（位段位置写错）
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

---

## 7. ✅ 决策门的**关闭记录**（2026-10-30，architect）

⚠️ §6 的 D1–D5 **已全部裁决** ⇒ 落在 **ADR-049**
（`docs/adr/ADR-049-clint-mtime-semantics.md`；critic `review_profile: arch`
第 1 轮 **APPROVE_WITH_CONDITIONS**，**0 BLOCKER**）：

| 决策 | §6 的倾向 | ADR-049 的裁决 | 差异 |
|---|---|---|---|
| **D1** 递增频率 | 可配置分频（默认每拍） | ✅ 同 —— 编译期常量 `TIMER_DIVIDER`（默认 1） | 追加了**分频相位**的精确定义（写拍**照常推进**、该次递增被丢弃） |
| **D2** 写 vs 递增 | 写优先 | ✅ 同 | 追加「写的那一拍**本拍不递增**」 |
| **D3** 复位值 | 0 | ✅ 同 | — |
| **D4** 32 位原子读 | 允许撕裂、不提供分离端口 | ✅ 同 | — |
| **D5** capability 归属 | 新建 capability | ✅ 同 —— **新建 `cpu-clint-timer`** | ⚠️ **前缀**：不是 `rv32i-`（CLINT 是**外设** ⇒ 与 RV32I 微架构无关 ⇒ `rv32i-` 前缀会自反）⇒ 按仓内「前缀 = 子系统」惯例取 `cpu-`（对齐 `cpu-cache-subsystem`） |

⇒ ⇒ **本草案的 §6 决策门已关闭** ⇒ ⇒ planner 可直接建 change。

⚠️ **实施期 MUST 携带的三条实测结论**（ADR-049 §1.4，探针实测）：

1. **进位 MUST 用硬件原语**：`carry ? 1_d : 0_d` 是 **C++ 三元** ⇒ 在 `ch_bool`
   上**建图期**求值 ⇒ **恒取 0** ⇒ 计数器**永远跨不过 32 位边界**；
2. **「保高半换低半」MUST 用 `bits_update<32>`**：`<<` 在 64 位上**编译不过**
   （框架 shift 用 C++ `1 << N`）；手写 `hi | lo`（**漏移位**）⇒ **静默清零高半**；
3. ⚠️ **`test_interrupt_flow` 会变成「时序相关」的脆弱测试**：它写 `mtime=50` 后只
   tick 1 拍就断言 `mtime < mtimecmp(100)` ⇒ 新语义下 `mtime` 已是 51 ⇒ **靠 49
   的余量才通过** ⇒ ⇒ MUST 改成「**每次采样前重写 `mtime`**」。

⚠️ **ADR-049 未裁决的**（留给 change 的 `design.md`）：内部表示选形态 A
（2×32 + 进位）还是形态 B（1×64 + 切片）—— 两者**逐拍等价**（实测）⇒ ADR-049
只强制**可观测语义** + 上述两条陷阱。
