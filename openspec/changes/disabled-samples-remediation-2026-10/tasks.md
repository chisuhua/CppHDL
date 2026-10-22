# Change: disabled-samples-remediation-2026-10 — 修复 14 个**禁用** sample 的编译

> **状态：草案（尚未实施）。**
> 立项自 `samples-regression-guard-2026-10` 的 Non-Goal 5。
> ⚠️ 本 change 的每个 Task 都以「**该组的 sample 已登记进
> `CH_SAMPLE_TARGETS` 且已从禁用清单移除**」为完成判据。

- [ ] ### Task 0: 复核基线（14 个 sample 的编译错误）
  - **action**:
    - 逐个重跑编译探测，确认根因**未变**（自 2026-10-22 登记以来框架可能已变）：
      ```bash
      FLAGS=$(grep -m1 "CXX_FLAGS" build/samples/CMakeFiles/counter.dir/flags.make | sed 's/^CXX_FLAGS = //')
      INCS=$(grep -m1 "CXX_INCLUDES" build/samples/CMakeFiles/counter.dir/flags.make | sed 's/^CXX_INCLUDES = //')
      # 清单来自禁用清单（名字在第 1 列）
      for f in $(cut -f1 samples/disabled_samples.txt | grep -v '^#'); do
          echo "### $f"
          g++ $FLAGS $INCS -c "samples/$f.cpp" -o /dev/null 2>&1 | grep -m1 "error:"
      done
      ```
    - ⚠️ 若根因**已变**（框架已修）⇒ 该 sample MAY 直接编译通过 ⇒ 走 Task 2
      的「登记」路径（不需改代码）
  - **verify**:
    ```bash
    grep -c . samples/disabled_samples.txt   # 14（不含注释行）
    ```

- [ ] ### Task 1: 组 A — 字面量后缀（5 个）
  - **action**:
    - `stream_arbiter_example` / `stream_demux_example` / `stream_flow_basic_example` /
      `stream_fork_example` / `stream_join_example`：
      去掉十六进制字面量的 `_d` 后缀（`0xAB_d` → `0xAB`）
    - ⚠️ 改完 MUST 逐个跑护栏（**运行**回归可能暴露新的失败）
  - **verify**:
    ```bash
    for f in stream_arbiter_example stream_demux_example stream_flow_basic_example \
             stream_fork_example stream_join_example; do
        g++ $FLAGS $INCS -c "samples/$f.cpp" -o /dev/null || echo "FAIL $f"
    done
    ```

- [ ] ### Task 2: 登记机制（每修好一个就跑）
  - **action**:
    - ① 补进 `samples/CMakeLists.txt` 的 `CH_SAMPLE_TARGETS`
    - ② 从 `samples/disabled_samples.txt` 移除该行
    - ③ 跑护栏
  - **verify**:
    ```bash
    bash scripts/run_all_samples.sh
    # C1/C2 守恒 MUST 仍绿
    ```

- [ ] ### Task 3: 组 B / C / D / E / F（7 个）
  - **action**: 按 proposal 的 D1 分组推进（context 访问器改名 → `stream_fifo`
    签名 → 头文件/命名空间 → POD bundle 分区 API → 组件 API）
    - 每组完成后走 Task 2 的登记流程
  - **verify**:
    ```bash
    bash scripts/run_all_samples.sh
    ```

- [ ] ### Task 4: 组 G（`if_demo`，可能需要框架变更）
  - **action**:
    - 尝试修复；若需要**框架层**新能力 ⇒ **不**在本 change 范围 ⇒
      留在禁用清单，把归位条件改成「依赖 <具体框架变更>」（D3）
  - **verify**:
    ```bash
    grep -c "if_demo" samples/disabled_samples.txt   # 0 或 1（视裁决）
    ```

- [ ] ### Task 5: 全量回归 + 文档 + archive
  - **action**:
    - `bash scripts/run_all_samples.sh` ⇒ MUST 全绿（**无 stale 豁免**）
    - `bash scripts/verify_test_counts.sh`（**CTEST_TOTAL 不变**）
    - 根 `AGENTS.md` 的「40 sample programs」条目同步（26 已注册 → N 已注册）
    - `openspec archive disabled-samples-remediation-2026-10 -y`
  - **verify**:
    ```bash
    bash scripts/run_all_samples.sh
    bash scripts/verify_test_counts.sh
    openspec validate --specs --strict
    grep -rn "disabled-samples-remediation-2026-10" openspec/specs/   # MUST 命中 0 行
    ```
