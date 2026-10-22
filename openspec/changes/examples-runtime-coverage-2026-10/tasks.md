# Change: examples-runtime-coverage-2026-10 — examples 的**运行**覆盖进 CI

> **状态：草案（尚未实施）。**
> 立项自 `samples-regression-guard-2026-10` 的 Non-Goal 4。
> ⚠️ 本任务表的 verify **必须先跑基线**（Task 0）—— 本提案**尚未实测**
> examples 的运行结果 ⇒ 不能假定它们是绿的。

- [ ] ### Task 0: 基线 + 逐个定性
  - **action**:
    - 跑一遍全部 28 个被跟踪的 example，记录退出码与耗时：
      ```bash
      ./run_all_ported_tests.sh 2>&1 | tee /tmp/opencode/examples_baseline.txt
      ```
    - ⚠️ 退出码 MUST 区分：`139` = SIGSEGV，`124` = timeout
    - 既有失败**逐个**定性：能修的修；不能修的 ⇒ 在脚本里**显式豁免**
      （sample 名 + 根因 + 关联缺陷 + **失效条件**），并逐条 `::notice` 打印
      （对齐 `scripts/run_all_samples.sh` 的既有约定 ⇒ **豁免不是静默的**）
    - 产出基线清单 ⇒ 写进
      `docs/reports/verification-examples-runtime-coverage-2026-10.md`
  - **verify**:
    ```bash
    test -f docs/reports/verification-examples-runtime-coverage-2026-10.md
    # 清单 MUST 覆盖全部 28 个被跟踪的 example
    grep -c "run_test " run_all_ported_tests.sh     # 28
    ```

- [ ] ### Task 1: 脚本的「缺失二进制 = 失败」+ 可移植超时
  - **action**:
    - `run_all_ported_tests.sh`：
      - 缺失二进制由 **SKIPPED** 改为 **FAILED**（D2）
      - 超时命令做 `command -v timeout || command -v gtimeout` 回退
        （与 `scripts/run_all_samples.sh` 一致）
      - 豁免项的**已知失败**列表（数据驱动）+ **stale 检测**
        （豁免项一旦转绿 ⇒ 报 stale 并失败）
  - **verify**:
    ```bash
    bash -n run_all_ported_tests.sh
    # 变异：把某个已知失败的 example 加进豁免 ⇒ 护栏 MUST 报 stale 并失败
    ```

- [ ] ### Task 2: CI job
  - **action**:
    - `.github/workflows/ci.yml` 新增 job `examples-runtime`：
      - **Linux-only**（`ubuntu-22.04`）
      - job 级 `timeout-minutes`（28 × 30s ≈ 14 min 上界 ⇒ 20）
      - 自己构建 examples（**不**依赖别的 job 的 artifact）
      - 跑 `bash run_all_ported_tests.sh`
  - **verify**:
    ```bash
    grep -n "examples-runtime" .github/workflows/ci.yml
    grep -n "timeout-minutes" .github/workflows/ci.yml
    ```

- [ ] ### Task 3: 文档 + 全量回归
  - **action**:
    - 根 `AGENTS.md`：COMMANDS 段 + 「已消除的过时信息」条目
    - `scripts/verify_test_counts.sh` 仍绿（**CTEST_TOTAL 不变**）
  - **verify**:
    ```bash
    bash scripts/verify_test_counts.sh
    (cd build && ctest -LE perf -j4 --output-on-failure)
    ./run_all_ported_tests.sh
    ```

- [ ] ### Task 4: archive
  - **action**:
    - `openspec archive examples-runtime-coverage-2026-10 -y`
      （**MUST NOT** 用 `--skip-specs`）
    - ⚠️ 本 change **不新增 capability** ⇒ archive 后 MUST NOT 出现
      含 change 名的 Purpose 骨架
  - **verify**:
    ```bash
    openspec validate --specs --strict
    grep -rn "examples-runtime-coverage-2026-10" openspec/specs/   # MUST 命中 0 行
    ```
