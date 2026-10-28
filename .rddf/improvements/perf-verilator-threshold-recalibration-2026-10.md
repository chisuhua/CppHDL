---
name: perf-verilator-threshold-recalibration-2026-10
priority: P2
source: perf-baseline-refresh-2026-10（3 样本实测推翻阈值表的前提假设）
phase: phase-5
category: tooling
type: chore
created_at: 2026-10-28
---

# Improvement: perf 回归阈值 MUST 与实测噪声模型一致（verilator 噪声 ≠ JIT 噪声）

- **Date drafted**: 2026-10-28
- **Drafted by**: build-agent (builder role)
- **Status**: 📋 5-segment draft，**决策权归 planner**（build 只登记，不定方案）
- **来源**：`perf-baseline-refresh-2026-10` 的 3 样本实测（同一二进制、同一旗标、空闲主机）

## 1. Why (背景与动机)

`tests/benchmark/perf_regression.cpp` 的 per-backend 阈值来自一次噪声研究，
其**前提**写在代码注释里：

> "verilator is expected to be as stable as JIT"（阈值 interpreter 2.5× / jit 1.6× / **verilator 1.3×**）

⚠️ **实测（2026-10-28，3 个独立样本）证伪了这个前提**：

| 行 | 3 个样本的 `median_us` | 离散度(max/min) | **最坏比值**(max/中位数) | 阈值 | 判定 |
|---|---|---|---|---|---|
| `TC-08 regs=100 verilator` | 2413.7 / 2409.8 / **3935.7** | **1.633×** | **1.631×** | 1.3× | ⚠️ **超阈 ⇒ 落在慢模的采样会报假回归** |
| `TC-11 regs=10 jit` | 99906.5 / 102630.8 / 54567.4 | 1.881× | 1.027× | 1.6× | 中位数落在慢模 ⇒ 不会假回归 |
| 其余 22 个可比较行 | — | ≤ 1.25× | ≤ 1.21× | — | 均在阈值内 |

⇒ ⇒ **1 行**（`TC-08 regs=100 verilator`）在当前阈值下**天然抖动**。
⇒ ⚠️ 它是**既有**性质（该行在新旧 baseline 下的门禁行为相同：2392.3 → 2413.7 = 1.009×）
⇒ `perf-baseline-refresh-2026-10` 把「改阈值」列为 **Non-Goal**（避免在重采集的
批次里顺手削弱闸门）⇒ ⇒ **需要独立立项**。

## 2. What Changes (变更范围)

⚠️ **方案未定**（planner 裁决）。候选方向（**互斥**，需选一）：

1. **重新标定 per-backend 阈值**：用 **N ≥ 5 个样本**重测噪声，取每行的
   `max/中位数`（最坏比值）作为噪声上界 ⇒ 阈值取「噪声上界 + 余量」
   ⇒ 简单、与现状同构，但**一个 backend 的阈值会被最差的那一行拉高**；
2. **per-(test, params, backend) 的噪声模型**：阈值不再只按 backend 划
   ⇒ 精确，但 `perf_regression` 的判据结构要改（且**阈值来源仍是单一权威**
   —— 守恒护栏的 C3 从 `perf_regression.cpp` 的 `kBackendThresholds` 读阈值
   ⇒ 任何重构 MUST 保持这个「单一权威」契约，否则护栏会 fail-closed）；
3. **不改阈值，改 baseline 的定位**：把基线取在「最坏模」⇒ 该行不再抖
   ⇒ **代价**：真实回归的下限被抬高（更漏报）⇒ 与仓内「宁可误报不可漏报」的
   取向相反 ⇒ **需 planner 显式裁决**。

⇒ 无论选哪个，两条**硬约束**：
- 阈值来源 MUST 保持**单一权威**（护栏 C3 依赖它 ⇒ fail-closed）；
- 任何**放宽**都 MUST **显式披露**（baseline 侧已有 C3；阈值侧目前**没有**
  等价机制 ⇒ 需一并考虑）。

## 3. Acceptance (验收标准)

- [ ] 阈值表（MUST 仍是 `perf_regression.cpp` 的单一权威）由 **N ≥ 5 样本**的
      实测噪声推导，**每个**可比较行的最坏比值 ≤ 该 backend 的阈值
      （⇒ 抖动行归零）
- [ ] 合成回归验证：对 verilator backend 注入 **1.3×** 的回归 MUST 仍被报出
      （⇒ 阈值没有被放宽到失去检出力）
- [ ] 连续 **N ≥ 5** 次 `ctest -L perf` 无因噪声导致的红（当前抖动行的最坏比值
      1.631× > 1.3× ⇒ 必须被消掉）
- [ ] `scripts/verify_perf_baseline_anchors.sh`（含 `--self-test`）仍 exit 0
      ⇒ 阈值来源契约未被破坏
- [ ] 若放宽了阈值 ⇒ 在 `perf-test-isolation` 的契约里**显式披露**（含实测依据）

## 4. Capabilities (涉及的能力维度)

- `perf-test-isolation` —— **可能 MODIFIED**（若「阈值的来源与推导方式」需要写进
  契约；当前契约只规定了 `kBackendThresholds` 是权威源，**未**规定它怎么被推导）
  ⇒ **待实施时判定**

## 5. Impact (影响面分析)

- **改动文件**：`tests/benchmark/perf_regression.cpp`（`kBackendThresholds`）
  + 可能 `openspec/specs/perf-test-isolation/spec.md` + 可能 baseline 的 `note`
- **计数**：**不变**（不新增/删除 ctest 条目）
- **风险**：**中** —— 放宽阈值 = **削弱**仓内唯一的性能闸门 ⇒ MUST 有合成回归
  的正向证据（Acceptance 第 2 条）⇒ 且 MUST 显式披露（Acceptance 第 5 条）
