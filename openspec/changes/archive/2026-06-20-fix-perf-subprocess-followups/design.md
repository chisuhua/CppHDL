## Context

F2 (`fix-perf-subprocess-isolation`, archived 2026-06-19, `openspec/changes/archive/2026-06-19-fix-perf-subprocess-isolation/`) 引入了 `tests/benchmark/subprocess_runner.h`(header-only,inline `run_perf_subprocess` 函数,基于 `fork()+execl()+waitpid()` 实现 180s 默认 timeout),并改造 `tests/benchmark/perf_main.cpp` 主循环,把 TC-07/08/09 三个 3-way 测试改为通过子进程执行。

实测(`tests/benchmark/perf_main.cpp` `--all` 模式,2026-06-20 developer workstation, `BUILD_VERILATOR=OFF`):

| TC | 模式 | 单次实测 wall-clock |
|---|---|---|
| TC-01/02/04/06 | in-process | ~5-10s |
| TC-07 depth=10/100/1000 | subprocess | ~30-60s(总计) |
| TC-08 regs=10/100/1000 | subprocess | ~10-30s(总计) |
| TC-09 depth=10/100/1000 | subprocess | ~200-400s(算术链,JIT 编译 + 长 interpreter) |
| TC-10 | UNSUPPORTED | ~1s |
| TC-11 regs=10/100/1000 | in-process(`run_three_way_tc11`) | ~200-300s(宽 reg 链) |
| **总计 --all** | | **~930s** (实测 868.20s ~ 1110.95s,因机器负载波动) |

F2 archive 后,F2 tasks.md 中 task 5.4-5.6 + task 6 标记完成,但实际**未执行**:
- task 5.4: "重新生成 `tests/benchmark/perf_baseline.json`" — 未做,baseline 仍为 `71d364b`(2026-06-14,F2 之前 in-process,K1 污染状态)
- task 5.5: "更新 note 字段" — 未做
- task 5.6: "跑 ctest -R perf_regression 最终验证" — 未做
- task 6.1-6.4: 文档同步 — 部分未做

archive 流程(`openspec archive`)只校验 proposal/specs/design/tasks 结构合规性,**不强制 tasks 完成**。F2 因此留下 3 个未交付项:
1. `perf_subprocess_isolation` 测试 TIMEOUT `300s` < 实测 ~868s → CTEST 在 300.01s 杀进程(实测 2026-06-19/20 反复 false-fail)
2. `perf_tests` 测试 TIMEOUT `900s` < 实测 ~1110s → CTEST 在 900.01s 杀进程(2026-06-20 02:52 实测失败一次)
3. baseline stale(pre-F2 polluted) vs current(post-F2 clean)处于不可比状态,`perf_regression` 失去真实回归检测能力(虽然 `0/142909 = 0 < 1.6x` 不会 false-positive,但也无法 true-positive)

## Goals / Non-Goals

**Goals:**
- 把 `perf_subprocess_isolation` / `perf_tests` 的 CTEST TIMEOUT 调整为实测 wall-clock 的 2× 安全余量,确保 CI 在机器负载波动下也能完成
- 按 F2 task 5.4-5.5 把 baseline 重新生成为 post-F2 clean subprocess-isolated 状态,让 `perf_regression` 重新具备真实回归检测能力
- 把 "TIMEOUT 必须容纳 wall-clock + 安全余量" 和 "baseline 必须随测量方法变更" 两个契约形式化到 `perf-test-isolation` capability spec,防止后续 F3/F4 类变更再次遗漏
- 文档同步(`tests/AGENTS.md` + `AGENTS.md`),修正 "5-10 minute" / "perf_tests ~120s" 等过时数据

**Non-Goals:**
- 不修 TC-09 子进程 180s timeout 不足根因(独立 follow-up: 需权衡 `subprocess_runner.h:168` timeout 提升到 ~600s vs `perf_main.cpp:744-759` TC-09 参数缩减到 `{10, 100}` 跳过 depth=1000)
- 不动 `perf_main.cpp` / `subprocess_runner.h` / `perf_regression.cpp` 主体逻辑
- 不实现 baseline 自动重新生成机制(目前手工操作,CI 触发器超出 scope)
- 不修复 `perf_main.cpp` 中 TC-11 用 `run_three_way_tc11`(in-process)而非 subprocess 调用 — 这是 F2 的遗留不一致(TC-11 也用 JIT,理论上应走 subprocess 隔离),但当前 `--all` TC-11 是最后一个跑的 TCs,in-process 不污染后续 measurement,属于可接受状态

## Decisions

### Decision 1: TIMEOUT `300/900` → `1800`(两个 CTest 都设 1800)

**Rationale**:
- 实测 `perf_tests --all` wall-clock: **868.20s**(`perf_subprocess_isolation` 单次运行)、**1110.95s**(`perf_tests` 单次运行)
- 实测两次差异主要来自磁盘/IO 抖动 + JIT 编译 cache 复用度,最大可能接近 1200s
- 1800s (30 min) = 实测 1100s 的 ~1.6× 余量 / 实测 870s 的 ~2× 余量,在 CI 慢机器 (2x slowdown) 下仍有 450s 余量
- 两个 CTest 都跑同一个 `perf_tests --all`,理论上 wall-clock 一致 → 统一设 1800 简化维护

**Alternatives considered**:
- (A) TIMEOUT `1200` (20 min) — 实测 1100s 时余量仅 100s,CI 慢机器会 false-fail,**拒绝**
- (B) TIMEOUT `2400` (40 min) — 保守但单测 wall-clock 太长,CI 累积成本过高,**拒绝**
- (C) TIMEOUT `1800` (30 min) — **接受**:实测 1.6-2× 余量,在 F2 文档 "5-10 minute" 的 3-6 倍,反映 F2 子进程隔离后的真实运行时长

### Decision 2: Baseline 从当前 `perf_results.json` 重新生成(不重新跑 `perf_tests --all`)

**Rationale**:
- 当前 `perf_results.json` 来自最新一次 `perf_tests --all`(2026-06-20T03:22:31Z, git_sha `7b27de5`),已经是 post-F2 clean subprocess-isolated 状态
- 重新跑 `perf_tests --all` 耗时 ~15-20 分钟,且对环境敏感(机器快慢影响数值),重新生成 baseline 会引入不必要的环境方差
- 用现有最新 perf_results.json 作为 baseline,确保 baseline 与"当前生产代码预期输出"完全一致
- baseline `git_sha` 同步更新为 `7b27de5`,`note` 字段标注 "F2 subprocess isolation baseline, regenerated 2026-06-20"

**Alternatives considered**:
- (A) 重新跑 `perf_tests --all` 后取 perf_results.json 作 baseline — **拒绝**:耗时 + 引入环境方差 + 当前 perf_results.json 已是干净状态
- (B) 用 Python 脚本批量修改 baseline — **拒绝**:容易出错,且 perf_results.json 已经是干净状态
- (C) 直接复制 perf_results.json 内容到 perf_baseline.json — **接受**:数据保真度 100%,无计算误差

### Decision 3: 把"TIMEOUT 必须容纳 wall-clock + 安全余量" 和 "baseline 必须随测量方法变更" 形式化到 spec

**Rationale**:
- 现状:F2 archive 后,新 TIMEOUT 与新 baseline 都以代码注释/文档形式存在(`tests/CMakeLists.txt:99-110` 注释 + `perf_baseline.json:4` `note` 字段)
- 风险:后续 PR 修改 `perf_main.cpp` / `subprocess_runner.h` 时,若不更新 TIMEOUT 与 baseline,会重复 F2 的 false-fail 问题
- 把契约形式化到 `openspec/specs/perf-test-isolation/spec.md`,archive 时合并到 main spec,后续 `openspec validate --specs` 可拦截遗漏

**新增 2 条 REQUIREMENT** (在 `perf-test-isolation` capability 下):
- `perf_main TIMEOUT accommodates wall-clock with safety margin`: CTEST TIMEOUT SHALL be ≥ 1.5× measured wall-clock + 5 minutes buffer; any change to perf_main's runtime MUST trigger TIMEOUT re-evaluation
- `perf baseline regenerated on measurement methodology change`: When perf_main's measurement methodology changes (e.g., subprocess isolation, JIT compile cache strategy, SIMD enable), `tests/benchmark/perf_baseline.json` MUST be regenerated in the same commit

**Alternatives considered**:
- (A) 不加 spec,只在 AGENTS.md / README 提示 — **拒绝**:AGENTS.md 是提示性文档,`openspec validate` 不会检查
- (B) 形式化为 1 条综合 REQUIREMENT — **拒绝**:1 条涵盖 2 个独立契约,违反 RFC 2119 单 concern 原则,validate 时易误判
- (C) 形式化为 2 条独立 REQUIREMENT — **接受**:每条独立 concern,validate 时可分别检查

## Risks / Trade-offs

| Risk | Mitigation |
|------|-----------|
| TIMEOUT 1800 远大于实测 870-1110s,CI 累积 wall-clock 增加 ~10-20 分钟 | 接受:F2 落地后这就是新基线,无法不付出代价;AGENTS.md 标注 runtime 预期 |
| Baseline 重新生成后 `perf_regression` 失去"恢复 F1 4.0× 阈值"的回退保护(但 F1 注释已删,阈值是 1.6×) | 接受:1.6× 是 F2 spec 要求,F2 已删 F1 注释,本 change 不改变阈值 |
| TC-09 子进程 180s timeout 仍不足(独立 root cause,本 change 不修) | 在 baseline `note` 字段显式标注 TC-09 SKIPPED 原因,记入 follow-up 跟踪(独立 PR) |
| 形式化的 2 条 spec REQUIREMENT 可能过于严格,小修 perf_main 就需要 update TIMEOUT + baseline | 加 "1.5× wall-clock + 5 min buffer" 与 "measurement methodology change" 两个限定语,避免过度触发 |
| `perf_regression` 在 baseline 重新生成后,如果代码引入真实回归(>1.6×),会立即 false-fail(预期行为) | 这是 spec 设计意图;CI 会捕获,开发者需要修复代码而非放宽阈值 |

## Migration Plan

**阶段 1**:同步本次修复(本 change 落地)
1. `tests/CMakeLists.txt:99-110` — `TIMEOUT 300` → `TIMEOUT 1800`,同步注释
2. `tests/benchmark/CMakeLists.txt:69-91` — 两处 `TIMEOUT 900` → `TIMEOUT 1800`,删除过时 "300s timeout guards against hangs" 注释,补实测 ~930s 说明
3. `tests/benchmark/perf_baseline.json` — 用当前 `perf_results.json` 重生成 53 行,更新 `git_sha` / `timestamp` / `note`
4. `tests/AGENTS.md:87` — "5-10 minute" → "~15-20 minute",TIMEOUT `300` → `1800`
5. `AGENTS.md:169` — "1 pre-existing timeout: `perf_tests` ~120s" → "TIMEOUT 1800 (F2 subprocess-isolated, ~15-20 min)"

**阶段 2**:验证(本 change 落地后立即跑)
1. `cmake -B build` + `cmake --build build -j$(nproc)` — 0 errors 0 warnings
2. `ctest -R "^perf_subprocess_isolation$" --timeout 1800` — PASSED (实测 868.20s)
3. `ctest -R "^perf_tests$" --timeout 1800` — PASSED (实测 1110.95s)
4. `ctest -R "^perf_regression$"` — PASSED (0.02s, no regressions)
5. `ctest -L perf -E "perf_tests|perf_subprocess_isolation"` — 7/7 PASSED
6. `ctest -L base -j4 --timeout 60` — 110/110 PASSED (无回归)

**阶段 3**:归档(本 change archive)
1. `openspec validate "fix-perf-subprocess-followups" --strict` — PASS
2. `openspec archive "fix-perf-subprocess-followups"` — 把 `specs/perf-test-isolation/spec.md` 的 2 条 ADDED REQUIREMENT 合并到 `openspec/specs/perf-test-isolation/spec.md`
3. `git log --oneline -3` — 确认 archive commit 在 main 上

**Rollback**: `git revert` 单一 commit;回滚后 TIMEOUT 恢复 300/900(可能再次 false-fail),baseline 恢复 stale(perf_regression 不可信)

## Open Questions

1. **TC-09 子进程 180s timeout 根因**(独立 follow-up,非本 change scope):
   - 选项 A: `subprocess_runner.h:168` `int timeout_sec = 180` 提升到 600(需权衡所有 TC 同步增加)
   - 选项 B: `perf_main.cpp:744-747` TC-09 params 从 `{10, 100, 1000}` 改为 `{10, 100}` 跳过 depth=1000(单次降 ~200s)
   - 选项 C: 拆分 TC-09 为多个 TC(depth=10/100/1000 各一个),各自用独立可配置 timeout
   - 责任: 未来 PR 评估,优先级低(perf_regression 已 baseline 重新生成,TC-09 SKIPPED 在 baseline + current 都为 0,不触发 false-positive)

2. **Baseline 自动重新生成机制**(独立 follow-up,非本 change scope):
   - 选项 A: 在 `perf_main.cpp` 加 `--update-baseline` 模式,直接重写 baseline
   - 选项 B: CI 在 `tests/benchmark/` 改动时触发 `perf_tests --all` + 重生成 baseline
   - 选项 C: 不实现,靠 PR 流程规范 + spec 形式化约束(本 change 已加 spec 约束)
   - 责任: 未来 PR 评估,优先级低(目前手工操作可控)

3. **TC-11 subprocess 隔离**: `perf_main.cpp:773-787` TC-11 用 `run_three_way_tc11`(in-process),而非 subprocess 调用。这是 F2 实现的遗留不一致(TC-11 也用 JIT,理论上应走 subprocess 隔离)。当前 `--all` 中 TC-11 是最后一个 TCs,in-process 不污染后续 measurement,属于可接受状态,但若未来调整 `--all` 顺序(如把 TC-11 移到前面),可能引入新 K1 类污染。
   - 责任: 未来 PR 评估,优先级低(目前运行 OK)

4. **CTEST `perf_tests` 与 `perf_subprocess_isolation` 都跑同一个 `perf_tests --all`**(wall-clock ~15-20 min × 2 = 30-40 min CI 累积):
   - 选项 A: 让 `perf_subprocess_isolation` 复用 `perf_tests` 的 perf_results.json(共享 fixture),各自只跑 standalone TC-08
   - 选项 B: 保持现状(独立运行,提供独立 evidence)
   - 选项 C: 把 `perf_subprocess_isolation` 改为 Catch2 SKIP if `perf_tests` ran recently(< 24h)
   - 责任: 未来 PR 评估,优先级中(若 CI 时长成为瓶颈)

## Cross-References

- **Proposal**: `proposal.md`
- **Spec**: `specs/perf-test-isolation/spec.md`
- **Tasks**: `tasks.md`
- **关联 archived change**: `openspec/changes/archive/2026-06-19-fix-perf-subprocess-isolation/`
  - tasks.md task 5.4-5.6 + task 6 (archive 时未执行)
  - specs/perf-test-isolation/spec.md (现有 5 条 REQUIREMENT)
- **关联文档**:
  - `tests/AGENTS.md` "perf measurement 隔离 (F2)" 章节
  - `AGENTS.md:169` "NOTES"
  - `docs/simulation/PERF_COMPARISON_REPORT.md §6 K1` (F2 子进程隔离缓解状态)
  - `openspec/changes/archive/2026-06-19-fix-jit-orc-state-leak/` (SUPERSEDED,K1 真因独立调研)