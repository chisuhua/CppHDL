# 提案：perf baseline MUST 与当前机器状态同步（重采集 + 守恒流程）

- **架构依据**：无
- **5 段草案**：`.rddf/improvements/perf-baseline-refresh-2026-10.md`
- **roadmap 归属**：工程纪律项
- **规格影响**：**可能**（`perf-test-isolation` MODIFIED —— 待实施时判定）⇒ 默认 `--skip-specs`
- **规模 / 风险**：**中**（重采集会**抬高**阈值 ⇒ 靠锚点守恒 + 变松披露把「变松」显式化）

## Why

**实测证据（2026-10-27，四条）**：

1. **依赖闭包**：perf 源文件**零** `#include` 任何 `cpu/` 头，`.d` 也**零**命中
   `include/cpu` ⇒ perf 路径与被改的 `include/cpu/**` **完全无关**；
2. **分档形态**：回归只在 **regs=10（最小档）**；regs=100/1000 的比率仅 **1.06–1.15×**
   ⇒ 代码级回归会**每档同比劣化**；
3. **劣化集中在固定开销**：`build_us` **5198 → 16220 µs**（JIT 编译 / 进程启动）
   ⇒ 是**机器状态**；
4. **同二进制二次测量有抖动**：verilator 行 3070 → **2734 µs**（掉回 1.3× 阈值内）。

⇒ baseline（采集于 **2026-10-25 / git_sha 7c5038e**，另一台机器状态）**已脱节**
⇒ 当前 `perf_regression` 在最小档**恒假红** ⇒ **真回归会被噪声淹没**。

## What Changes

1. 在**空闲机器**上重采集 `tests/benchmark/perf_baseline.json`，走
   `scripts/verify_perf_baseline_anchors.sh` 的守恒流程：
   - **C1 行集守恒**（baseline 的行集合不得变）；
   - **C2 锚点守恒**：带 `note_restored` 标记的行 MUST 断言取值与旧基准**逐字相等**
     （⚠️ 该标记曾是「降级逃逸口」⇒ 加一个 JSON 字段就能把 PASS 锚点永久摘掉）；
   - **C3 变松披露**：新 baseline 相对旧 baseline **变慢**的行 MUST 在 `note` 里点名；
2. `note` 字段 MUST 记录：git_sha、采集日期、机器状态说明、**与旧 baseline 的比率**
   （沿用 2026-10-25 那次的做法：把「为什么这不是代码回归」的论证写进 note）。

⚠️ **纪律**：
- **单独批次** —— 不与任何其它改动同批 ⇒ 归因可分离；
- 采集期间**不得**有其它构建 / 测试在跑（本次实测教训：并发构建把 `perf_tests`
  拖到 **TIMEOUT**，那次 4 个测试全红是**环境**造成的）。

## Acceptance

- [ ] `scripts/verify_perf_baseline_anchors.sh` exit 0（含其 `--self-test`）
- [ ] `ctest -L perf` **11/11**
- [ ] 新 baseline 的 `note` 含 git_sha / 日期 / 与旧 baseline 的比率 / 环境说明
- [ ] 行集守恒（`perf_regression` 仍比较 **18** 行、`skipped 0` 行）

## Capabilities

- `perf-test-isolation` —— **可能 MODIFIED ×1**（若「重采集的守恒流程」需要写进契约；
  若既有 Requirement 已覆盖 ⇒ 不改）⇒ **待实施时判定**

## Impact

- **文件面**：`tests/benchmark/perf_baseline.json`（+ 可能 `perf_regression.cpp` 的注释）
- **计数**：**不变**
- **风险**：**中** —— 重采集本身会**抬高**阈值 ⇒ MUST 靠 C2 锚点守恒 + C3 披露
  把「变松」**显式化**，避免把真回归一起放过

## Non-Goals

- 不改 perf 基准的**内容**（TC-01..TC-11 的定义不动）
- 不改阈值（1.6× / 2.5× / 1.3× 不动）
- 不在同批做任何**其它**改动
