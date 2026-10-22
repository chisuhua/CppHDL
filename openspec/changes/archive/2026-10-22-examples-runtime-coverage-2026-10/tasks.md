# Change: examples-runtime-coverage-2026-10 — examples 的**运行**覆盖进 CI

> **状态：已实施（2026-10-22）。**
> 立项自 `samples-regression-guard-2026-10` 的 Non-Goal 4。
> ✅ **基线已实测**（2026-10-22，pristine HEAD）：`./run_all_ported_tests.sh`
> ⇒ **28/28 PASS、0 FAIL、0 SKIP、exit=0** ⇒ **无既有失败要修**；
> 豁免机制**没有现成探针** ⇒ Task 1 的验收 MUST 用**人为变异**。
> ⚠️ **Task 2 MUST 先于 Task 3**（`all-examples` 聚合 target 是 CI job 的前置）。
> ⚠️ **实施期设计细化**：跟踪集合改为**单一来源**
> （`examples/CMakeLists.txt` 的 `CH_TRACKED_EXAMPLE_TARGETS`）⇒ 脚本里
> **不再有** 28 行硬编码 `run_test` ⇒ 下方 verify 相应改为「解析单一来源」。

- [x] ### Task 0: 复核基线 + 产出基线清单
  - **action**:
    - 复核基线（框架已变 ⇒ 2026-10-22 的全绿结论可能已过期）：
      ```bash
      ./run_all_ported_tests.sh 2>&1 | tee /tmp/opencode/examples_baseline.txt
      ```
    - ⚠️ 退出码 MUST 区分：`139` = SIGSEGV，`124` = timeout
    - **2026-10-22 基线（实测）**：28/28 PASS、0 FAIL、0 SKIP、exit=0
    - 若复核出现失败 ⇒ 逐个定性：能修的修；不能修的 ⇒ 在脚本里**显式豁免**
      （example 名 + 根因 + 关联缺陷 + **失效条件**），并逐条 `::notice` 打印
      （对齐 `scripts/run_all_samples.sh` 的既有约定 ⇒ **豁免不是静默的**）
    - 产出基线清单（**28 行**，每行含 example 名 / 退出码 / 耗时）⇒ 写进
      `docs/reports/verification-examples-runtime-coverage-2026-10.md`
  - **verify**:
    ```bash
    test -f docs/reports/verification-examples-runtime-coverage-2026-10.md
    # 基线清单 MUST 覆盖全部 28 个被跟踪的 example（每行一条）
    grep -cE '^\| `[a-z0-9_]+` \|' docs/reports/verification-examples-runtime-coverage-2026-10.md  # 28
    ```

- [x] ### Task 1: 脚本的「缺失二进制」语义 + 可移植超时 + 数量守恒
  - **action**:
    - ⚠️ **根因（不是「SKIP」）**：`run_all_ported_tests.sh:14` 的 `set -euo pipefail`
      + `run_test()` 的 `:53` `return 2` ⇒ 现状是**在第一个缺失二进制处静默中止**
      （exit=2、报告被截断）。⇒ 三步改：
      1. 缺失二进制分支 MUST 与「正常失败」分支**同构**：记 `FAIL`、`FAIL_NAMES+=`、
         **返回 0 / `continue`**（**MUST NOT** 返回非零 —— 那正是中止的根因）；
      2. 加 `EXECUTED` 计数 + **C2 式数量守恒**（`EXECUTED == 跟踪数`）
         ⇒ 对齐 `scripts/run_all_samples.sh:259-266` 的既有形态；
         **否则**受护集合仍可能静默缩面；
      3. 汇总行的 `SKIP` 计数随之恒 0 ⇒ 移除或改义（不留恒 0 的「看起来像有跳过」
         的槽位）。
    - 超时命令做 `command -v timeout || command -v gtimeout` 回退
      （与 `scripts/run_all_samples.sh` 一致）
    - **失败可诊断**：`:56` 现在把 stdout/stderr **全丢 `/dev/null`** ⇒ 改为
      逐例落盘（`build/logs/examples/<name>.log`），失败时 CI 上传 artifact
    - 豁免项的**已知失败**列表（数据驱动）+ **stale 检测**
      （豁免项一旦转绿 ⇒ 报 stale 并失败）
    - 缺失二进制时的报错 MUST **指名**缺哪个二进制 + 提示「MUST 先完整构建」
      （本地开发者只做增量构建时会看到它）
    - 顺带订正 `scripts/run_all_samples.sh:10-13` 那句**错误**的注释
      （说 `run_all_ported_tests.sh` 会 SKIP 缺失二进制 —— 实为静默中止）
    - **实施期新增**：C1「注册一致性」—— 每个**已构建**的 example 程序 MUST
      要么在跟踪集合里、要么在 `examples/disabled_examples.txt` 里；
      反向检查（双重登记）同样算失败
  - **verify**:
    ```bash
    bash -n run_all_ported_tests.sh
    # ⚠️ 变异验证 MUST 用「人为移走一个二进制」（基线全绿 ⇒ 无「已知失败项」可用）：
    mv build/examples/stream/stream_mux_demo /tmp/opencode/
    ./run_all_ported_tests.sh; rc=$?
    mv /tmp/opencode/stream_mux_demo build/examples/stream/
    # MUST 同时满足三条：
    #   ① rc != 0
    #   ② 其余 27 个 example 仍被执行（报告行数 == 28，不是 1）
    #   ③ 缺失的那一个被记为 FAILED（不是 SKIP、也不是「静默中止」）
    ```

- [x] ### Task 2: 跟踪集合单源 + `all-examples` 聚合 target + 未跟踪程序归属
  - **action**:
    - 在 `examples/CMakeLists.txt`（**新建**）新增**单源清单**
      `set(CH_TRACKED_EXAMPLE_TARGETS …)`（28 个名字），由它**同时**驱动
      `all-examples` 聚合 target **和**护栏脚本
      （samples 侧现成范式：`scripts/run_all_samples.sh:95-99` 的
      `registered_samples()` 解析 `CH_SAMPLE_TARGETS`）
    - 根 `CMakeLists.txt` 的 21 个 `add_subdirectory(examples/...)` **迁入**
      `examples/CMakeLists.txt` ⇒ 根只剩 `add_subdirectory(examples)`
      （⚠️ 二进制目录**完全不变**：`build/examples/<相对路径>/`）
    - 根 `CMakeLists.txt` 新增 `all-examples`：
      - MUST **显式列出**那 28 个（MUST NOT 整目录收编 ——
        `examples/riscv-mini/CMakeLists.txt` 有 **19 个 Catch2 测试 target**
        会被误纳入）；
      - MUST 定义在 `if(CPPHDL_BUILD_EXAMPLES)` 块**内**（该 option 可关 ⇒
        块外定义会让 `add_dependencies` 指向不存在的 target ⇒ configure 失败）；
      - 镜像 samples 侧形态：`add_custom_target(all-examples)` +
        `add_dependencies(all-examples ${CH_TRACKED_EXAMPLE_TARGETS})`
    - 新增 `examples/disabled_examples.txt`：登记**未被跟踪**的 main() 程序
      （3 个 verilator demo + `examples/spinalhdl-ported/testbench_template.cpp`）
      **与 19 个从不执行的 Catch2 二进制**，每项带理由 + 归位条件
      ⇒ 「新增即漏」不再静默
    - ⚠️ **19 个 Catch2 二进制的裁决**：本次**只登记**（归位条件 =「补
      `add_test` 进 CTest」）—— 因为补 `add_test` 会改变 `CTEST_TOTAL`
      ⇒ 属独立立项，不在本 change 范围
  - **verify**:
    ```bash
    cmake -B build -DCMAKE_BUILD_TYPE=Release >/dev/null && cmake --build build --target all-examples -j$(nproc)
    # 28 个二进制全部存在
    for t in $(sed -n '/set(CH_TRACKED_EXAMPLE_TARGETS/,/^)/p' examples/CMakeLists.txt \
                 | sed -n 's/^    \([A-Za-z0-9_]*\)$/\1/p'); do
      find build/examples -type f -name "$t" -perm -u+x | grep -q . || { echo "MISSING $t"; exit 1; }
    done
    # 未跟踪程序有归属（C1 式）
    test -f examples/disabled_examples.txt
    grep -v '^#' examples/disabled_examples.txt | grep -c .   # 23
    ```

- [x] ### Task 3: CI job
  - **action**:
    - `.github/workflows/ci.yml` 新增 job `examples-runtime`：
      - **Linux-only**（`ubuntu-22.04`）
      - ⚠️ MUST **镜像 `samples-regression` job 的全套工具链步骤**
        （`ci.yml:399-437`）：g++-13（PPA 失败回退 g++-12）+ APT 的
        `llvm-${LLVM_VERSION}-dev` + `clang-` + `BUILD_VERILATOR=OFF`
        —— 否则裸 runner 上 **configure 即失败**（根 `CMakeLists.txt:13` 的
        `CH_JIT_ENABLE` 默认 ON ⇒ `:73` `find_package(LLVM REQUIRED)` 失败）
      - job 级 `timeout-minutes: 30`（28 × 30s ≈ 14 min 上界 **再加构建**
        ⇒ 20 min 在冷 ccache 下偏紧；对齐 `samples-regression` 的 30）
      - 自己构建（**不**依赖别的 job 的 artifact）：
        `cmake --build build --target all-examples`
      - 跑 `bash run_all_ported_tests.sh`
      - 失败时上传逐例日志 artifact（`build/logs/examples`）
  - **verify**:
    ```bash
    grep -n "examples-runtime" .github/workflows/ci.yml
    # job 级 timeout MUST 存在且 >= 30
    grep -n -A3 "examples-runtime:" .github/workflows/ci.yml | grep "timeout-minutes"
    # 工具链步骤 MUST 存在（LLVM）
    grep -n "llvm-.*-dev" .github/workflows/ci.yml
    # ⚠️ 不得用「grep -n timeout-minutes」当验收（既有 samples-regression job
    # 已含该串 ⇒ 恒真）
    ```

- [x] ### Task 4: 文档同步 + 全量回归
  - **action**:
    - 根 `AGENTS.md`：COMMANDS 段（`:192`）+ PHASE COMPLETION GATE 表格（`:183`）
      + NOTES 的 example 计数（`:228` / `:232`）
    - `openspec/config.yaml:50`（「测试验证命令: Examples」）同步脚本的新语义
      （5 处引用同批更新 —— ZERO-DEBT：不留孤儿引用）
    - `scripts/verify_test_counts.sh` 仍绿（**CTEST_TOTAL 不变** ——
      本次**未**给那 19 个 Catch2 target 加 `add_test`，见 Task 2）
  - **verify**:
    ```bash
    bash scripts/verify_test_counts.sh
    (cd build && ctest -LE perf -j4 --output-on-failure)
    ./run_all_ported_tests.sh
    # 5 处引用 MUST 全部指向新语义
    grep -rn "run_all_ported_tests.sh" openspec/config.yaml AGENTS.md
    ```

- [x] ### Task 5: archive
  - **action**:
    - `openspec archive examples-runtime-coverage-2026-10 -y`
      （**MUST NOT** 用 `--skip-specs`）
    - ⚠️ 本 change **新增 capability** `example-runtime-coverage`
      ⇒ archive 后 `openspec/specs/example-runtime-coverage/spec.md` MUST 存在
      ⇒ 其 `## Purpose` MUST 是**写好的**（不是 CLI 生成的占位句）
  - **verify**:
    ```bash
    # ⚠️ 不得用 `openspec validate --specs --strict` 当验收 ——
    # 它在 pristine HEAD 上就是红的（CLI 1.14.3 实测：3 passed, 15 failed，
    # 与 AGENTS.md 记录的 1.4.1 → 1.14.x 漂移一致）⇒ 不具判别力
    openspec validate examples-runtime-coverage-2026-10 --strict
    test -f openspec/specs/example-runtime-coverage/spec.md
    grep -c "## Purpose" openspec/specs/example-runtime-coverage/spec.md   # == 1
    grep -rn "examples-runtime-coverage-2026-10" openspec/specs/   # MUST 命中 0 行
    ```
