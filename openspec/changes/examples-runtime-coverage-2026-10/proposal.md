# Change: examples-runtime-coverage-2026-10 — examples 的**运行**覆盖进 CI

- Status: **草案（尚未实施）**
- 立项自 2026-10-22 的 `samples-regression-guard-2026-10`（其 Non-Goal 4）
- 关联 capability：`sample-regression-guard`（本次**不改**该 capability 的契约）

## Why

`samples/` 的运行覆盖已由 `samples-regression-guard-2026-10` 落地
（`scripts/run_all_samples.sh` + CI job `samples-regression`）。

但**同一类缺口在 `examples/` 上依然存在**，且它是 `samples-regression-guard`
的 proposal 里**显式记为 Non-Goal 4** 的那一项：

| 事实 | 取证 |
|---|---|
| `examples/` 的**编译**覆盖在 CI 里 | `ci.yml` 的 `build-and-test` job 跑 `cmake --build build`（含全部 example target） |
| `examples/` 的**运行**覆盖**不在** CI 里 | `grep -rn "run_all_ported" .github/workflows/` ⇒ **0 命中** |
| 本地有运行覆盖 | `run_all_ported_tests.sh` 覆盖 **28** 个 main()-bearing example |
| example 数量 | `grep -rl '^int main' examples --include='*.cpp' \| wc -l` = **32**（另 4 个是 demo，不由脚本跟踪） |

⇒ 与 `samples/` 同样的形态：**example 坏了，CI 不知道**（只有编译错误会被发现；
运行时崩溃 / 行为退化**无人知晓**）。

> ℹ️ 本次审计（2026-10-22）已在 `samples/` 上抓到 **2 个既有 segfault**；
> `examples/` 的 28 个程序**从未**被 CI 运行过 ⇒ 那里极可能也藏着同类问题。
> ⚠️ **本提案尚未实测 examples 的运行结果** —— 落地时必须先跑一遍基线
> （Task 0），既有失败逐个定性（修 / 显式豁免），**MUST NOT** 静默跳过。

## What Changes

### D1: 新增 CI job 跑 `run_all_ported_tests.sh`

- **Linux-only**（`ubuntu-22.04`）—— 脚本用 `timeout 30`（GNU coreutils）；
  macOS 无 `timeout`（只有 `gtimeout`）⇒ 要么把脚本改成 `timeout`/`gtimeout`
  回退（与 `scripts/run_all_samples.sh` 的既有做法一致），要么该 job 只挂 Linux。
  **裁决**：**两个都做** —— 脚本加回退（与 samples 护栏一致）+ job 挂 Linux
  （保守：macOS runner 的 example 行为与 Linux 可能有差异，先只在 Linux 上守）。
- MUST 设 job 级 `timeout-minutes`（28 × 30s ≈ 14 min 上界）。
- ⚠️ 该 job MUST 在**构建之后**跑（example 二进制由 `build-and-test` job 构建；
  新 job 需要自己构建或复用 artifact ⇒ **裁决**：新 job 自己跑
  `cmake --build build --target <examples>`，避免跨 job 的 artifact 依赖）。

### D2: 脚本的「缺失二进制 = SKIP」语义 MUST 复核

`run_all_ported_tests.sh` 当前对缺失二进制记 **SKIPPED**（不算失败）。
对**回归护栏**而言这与 `samples` 护栏的 C3 同型（缺失即静默缩面）。

⚠️ 但 examples 的 target 集合**没有**一份「注册清单」可对照 ⇒ 要引入同样的
「覆盖集合守恒」，需要先有清单。**裁决**：本次**只**把「缺失二进制」从
SKIP 改成**失败**（对已知的 28 个被跟踪 example 而言，缺失一定是构建问题），
**不**引入注册清单（那是更大的一步；samples 侧的清单机制见
`samples/disabled_samples.txt`）。

### D3: 基线先行

落地时 MUST 先跑一遍全部 28 个 example，记录退出码 ⇒ 既有失败逐个定性
（修 / 显式豁免 + 理由 + 失效条件），**MUST NOT** 让 CI 从第一天就红。

## Impact

| 项 | 影响 |
|---|---|
| `.github/workflows/ci.yml` | 新增 1 个 **Linux-only** job |
| `run_all_ported_tests.sh` | 「缺失二进制」由 SKIP 改为**失败**；加 `timeout`/`gtimeout` 回退 |
| `docs/reports/verification-examples-runtime-coverage-2026-10.md` | **新增**（基线清单 + 变异/豁免结果） |
| 根 `AGENTS.md` | COMMANDS 段 + 「已消除的过时信息」条目 |
| 计数 | **CTEST_TOTAL 不变**（examples 不是 ctest target） |
| 风险 | 🟡 中。① 首次落地**很可能**暴露既有失败（与 samples 侧同型）⇒ 必须先有基线；② 脚本语义改动（SKIP → 失败）可能让原本「绿」的 CI 变红 ⇒ 同上 |

## Non-Goals

1. **不**把 examples 改写成 ctest target（与 samples 侧同款裁决）
2. **不**给 example 加行为断言（example 的职责是「演示」；行为断言属于 `tests/`）
3. **不**引入 examples 的「注册清单 + 覆盖集合守恒」（见 D2 的裁决）
4. **不**处理 macOS runner（保守起见只守 Linux）

## 落地顺序

MUST 在 `samples-regression-guard-2026-10` **之后**落地（两者都改
`run_all_*.sh` 与 CI；串行更安全）。
