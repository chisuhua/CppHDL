---
name: perf-baseline-refresh-2026-10
priority: P0
source: 2026-10-27 ISA ④ 归档前的 perf 实测（perf_regression 假红 2 处）
phase: phase-5
category: tooling
type: chore
created_at: 2026-10-27
---

# Improvement: perf baseline MUST 与当前机器状态同步（重采集 + 守恒流程）

- **Date drafted**: 2026-10-27
- **Drafted by**: build-agent (builder role)
- **Status**: 📋 5-segment draft，归属 `feat-dual-backend-coverage` 的**工程纪律**项
- **来源**: 2026-10-27 实测 —— `ctest -L perf` 10/11，`perf_regression` 报
  TC-08 **regs=10** 两处回归（jit 1.89× / verilator 1.31×）

## 1. Why (背景与动机)

**实测证据（2026-10-27，四条）**：

1. **依赖闭包**：perf 源文件**零** `#include` 任何 `cpu/` 头，编译产物 `.d` 也**零**命中
   `include/cpu` ⇒ perf 路径与被改的 `include/cpu/**` **完全无关**；
2. **分档形态**：回归只在 **regs=10（最小档）**；regs=100/1000 的比率仅 **1.06–1.15×**
   ⇒ 代码级回归会**每档同比劣化**；
3. **劣化集中在固定开销**：`build_us` **5198 → 16220 µs**（JIT 编译 / 进程启动）
   ⇒ 是**机器状态**；
4. **同二进制二次测量有抖动**：verilator 行 3070 → **2734 µs**（掉回 1.3× 阈值内）。

⇒ 结论：baseline（采集于 **2026-10-25 / git_sha 7c5038e**，另一台机器状态）**已脱节**
⇒ 当前 `perf_regression` 在最小档**恒假红** ⇒ 真回归会被**噪声淹没**。

## 2. What Changes (变更范围)

- 在**空闲机器**上重采集 `tests/benchmark/perf_baseline.json`，
  走 `scripts/verify_perf_baseline_anchors.sh` 的守恒流程：
  - **C1 行集守恒**（baseline 的行集合不得变）；
  - **C2 锚点守恒**：带 `note_restored` 标记的行 MUST 断言取值与旧基准**逐字相等**
    （⚠️ 该标记曾是「降级逃逸口」⇒ 加一个 JSON 字段就能把 PASS 锚点永久摘掉）；
  - **C3 变松披露**：新 baseline 相对旧 baseline **变慢**的行 MUST 在 `note` 里点名；
- `note` 字段 MUST 记录：git_sha、采集日期、机器状态说明、**与旧 baseline 的比率**
  （沿用 2026-10-25 那次的做法：把「为什么这不是代码回归」的论证写进 note）。

⚠️ **纪律**：
- **单独批次** —— 不与任何其它改动同批 ⇒ 归因可分离；
- 采集期间**不得**有其它构建 / 测试在跑（本次实测教训：并发构建把 `perf_tests`
  拖到 **TIMEOUT**，那次 4 个测试全红是**环境**造成的）。

## 3. Acceptance (验收标准)

- [ ] `scripts/verify_perf_baseline_anchors.sh` exit 0（含其 `--self-test`）
- [ ] `ctest -L perf` **11/11**
- [ ] 新 baseline 的 `note` 含 git_sha / 日期 / 与旧 baseline 的比率 / 环境说明
- [ ] 行集守恒（`perf_regression` 仍比较 **18** 行，**skipped 0** 行）

## 4. Capabilities (涉及的能力维度)

- `perf-test-isolation` —— **可能 MODIFIED**（若「重采集的守恒流程」需要写进契约；
  若既有 Requirement 已覆盖 ⇒ 不改）⇒ **待实施时判定**

## 5. Impact (影响面分析)

- **改动文件**：`tests/benchmark/perf_baseline.json`（+ 可能 `perf_regression.cpp` 的注释）
- **计数**：**不变**
- **风险**：**中** —— 重采集本身会**抬高**阈值（baseline 变松）⇒ MUST 靠 C2 锚点守恒
  + C3 披露把「变松」**显式化**，避免把真回归一起放过
