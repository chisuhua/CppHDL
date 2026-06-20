## Why

F2 (`fix-perf-subprocess-isolation`, archived 2026-06-19)通过将 TC-07/08/09 改为独立子进程执行,消除了 K1 ORC JIT 跨 DUT 状态污染。但 F2 落地后留下 3 个未交付/未配套项,导致当前 ctest -L perf 在生产 CI 中**事实上失败**:
1. `tests/CMakeLists.txt:109` 中 `perf_subprocess_isolation` 测试的 CTEST TIMEOUT 仍为 **300s**,但 `perf_tests --all` 在 F2 子进程隔离下实测 **~930s**(独立测量 868.20s ~ 1110.95s),CTEST 在 300.01s 时发 SIGTERM 杀掉测试,F2 自身的回归测试反复 false-fail
2. `tests/benchmark/CMakeLists.txt:79,88` 中 `perf_tests` CTest 的 TIMEOUT 仍为 **900s**,与实际 930s+ 几乎贴边运行,CI 机器方差下偶发失败(2026-06-20 02:52 实测 900.01s 失败一次)
3. `tests/benchmark/perf_baseline.json` 自 2026-06-14 (git_sha `71d364b`) 以来**从未按 F2 task 5.4 重新生成**,baseline 反映 F2 之前的污染状态(`TC-08 JIT = 250,369us`),current 反映 F2 之后的干净状态(`TC-08 JIT = 2,663us`),两者处于不可比的测量状态,`perf_regression` 失去回归检测信号

根因:F2 tasks.md task 5.4-5.6 在 archive 时**未真正执行**(tasks.md 全部 `[ ]` 状态保留),archive 流程只检查结构合规不强制任务完成。本 change 是 F2 的**完成性修复**,把 F2 落地时缺失的 TIMEOUT 调整 + baseline 重新生成补齐,让 `perf_regression` 重新具备捕捉未来真实回归的能力。

## What Changes

- **CTEST TIMEOUT 调整**(实测数据驱动):
  - `tests/CMakeLists.txt:109` — `perf_subprocess_isolation` TIMEOUT `300` → `1800`(实测 868s,加 2× 安全余量)
  - `tests/benchmark/CMakeLists.txt:79,88` — `perf_tests` TIMEOUT `900` → `1800`(实测 1110s,加 1.6× 安全余量;同时删除"F1 300s timeout guards against hangs"的过时注释,改为反映实测 ~930s)
- **Baseline 重新生成**(F2 task 5.4-5.5 落地):
  - `tests/benchmark/perf_baseline.json` — 从当前 `perf_results.json`(git_sha `7b27de5`,subprocess-isolated clean state) 重新生成 53 行,`note` 字段标注 F2 重新生成 + TC-09 SKIPPED 的 180s 子进程超时根因
- **文档同步**(F2 task 6 配套):
  - `tests/AGENTS.md` — "perf subprocess tests" 文档从 "5-10 minute" 修正为实测 "~15-20 minute",TIMEOUT 提示从 `300` 修正为 `1800`
  - `AGENTS.md` — "1 pre-existing timeout: `perf_tests` ~120s" 修正为 "TIMEOUT 1800 (F2 subprocess-isolated, ~15-20 min)"
- **不再 scope 内**:
  - 不动 `perf_main.cpp` / `subprocess_runner.h` 主体逻辑(F2 已交付)
  - 不修 TC-09 180s 子进程超时根因(独立 follow-up,需权衡 `subprocess_runner.h` timeout 提升 vs `perf_main.cpp` TC-09 参数缩减)
  - 不改 F1/F2 历史 spec(`openspec/specs/perf-test-isolation/spec.md` 已 archive),仅在 capability `perf-test-isolation` 下新增 1 条 REQUIREMENT 把 TIMEOUT 与 baseline 重新生成契约形式化

## Capabilities

### New Capabilities

无。本 change 不引入新 capability。

### Modified Capabilities

- `perf-test-isolation`: 新增 2 条 REQUIREMENT,把 F2 落地时的 2 个未形式化约束(TIMEOUT 与 baseline 重新生成)固化到 spec,防止后续 F2 类变更再次遗漏 TIMEOUT 同步。

## Impact

| 类别 | 影响 |
|------|------|
| 受影响 CMake | `tests/CMakeLists.txt`(perf_subprocess_isolation TIMEOUT + 注释)、`tests/benchmark/CMakeLists.txt`(perf_tests TIMEOUT + 注释) |
| 受影响数据 | `tests/benchmark/perf_baseline.json`(重生成,~24 KB) |
| 受影响文档 | `tests/AGENTS.md`、`AGENTS.md` |
| 受影响 spec | `openspec/specs/perf-test-isolation/spec.md`(新增 2 条 REQUIREMENT,archive 时合并) |
| CI | `ctest -L perf` 中 `perf_subprocess_isolation` / `perf_tests` / `perf_regression` 三个测试从"易 false-fail / stale baseline"恢复为"可信" |
| 性能 | 单次完整 perf 测试套件 wall-clock ~15-20 分钟(无变化,与 F2 一致);新增 baseline 重新生成可在 archive 流程后由 CI 一次性跑(无需每次回归都重生成) |
| 风险 | 低。TIMEOUT 调整、baseline 重新生成均为一次性操作,且文档化测量依据 |
| 回滚 | `git revert` 单一 commit;回滚后 TIMEOUT 恢复原值(可能再次 false-fail),baseline 恢复 stale(perf_regression 不可信) |
| 关联 issue | F2 tasks.md task 5.4-5.6 + task 6(archive 时未执行);`tests/AGENTS.md` "perf measurement 隔离" 章节 |

## Non-Goals

- 不根治 K1 ORC 状态污染的真因(独立调研,见 archived `fix-jit-orc-state-leak`)
- 不修改 `subprocess_runner.h` 的 180s 默认 timeout(独立 follow-up: 需权衡子进程超时 vs perf_main.cpp TC-09 参数)
- 不改 `perf_main.cpp` 主循环或 3-way 子进程调用逻辑(F2 已交付)
- 不改 `perf_regression.cpp` 阈值(1.6x jit / 2.5x interp / 1.3x verilator 维持 F2 不变)
- 不实现 baseline 自动重新生成机制(目前手工操作,未来可作为 CI 触发器,但超出本 change scope)
- 不动 `perf_tests` 二进制本身的实现,只通过 CMake TIMEOUT + baseline 数据驱动修复
- 不实现其他 perf 框架缺陷(TC-09 子进程超时、`build_us=0`、`overhead_percent` 计算异常 — 独立 PR)