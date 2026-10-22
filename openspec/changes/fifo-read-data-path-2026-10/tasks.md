# Change: fifo-read-data-path-2026-10 — `fifo_bundle` 的**读数据通路**

> **状态：草案（尚未实施）。**
> 立项自 `bundle-ecosystem-hardening-2026-10` 的 L10（用户裁决独立立项）。
> ⚠️ **公开 API 布局变更** ⇒ Task 1 的 ADR MUST **先于**任何代码改动落地。

- [ ] ### Task 0: 清点影响面（ADR 的输入）
  - **action**:
    - 清点 `fifo_bundle` 的**全部**使用点（字段布局变化 ⇒ 位偏移全变）：
      ```bash
      grep -rn "fifo_bundle" --include=*.cpp --include=*.h include/ tests/ examples/ samples/
      ```
    - 清点**位偏移常量**的使用点（`kFifoPushBit` / `kFifoFullBit` / ...）：
      ```bash
      grep -rn "kFifo.*Bit\|bundle_width_v<fifo_bundle" --include=*.cpp tests/
      ```
    - 清点 `ch_stream` 的 `payload` 是否构成同病（**预期：不构成** —— stream 协议
      规定 `payload` 由 master 驱动）⇒ 在 ADR 里写明该清点结论
  - **verify**:
    ```bash
    test -f docs/reports/verification-fifo-read-data-path-2026-10.md
    ```

- [ ] ### Task 1: 写 ADR-044（形态裁决 + 时序协议）
  - **action**:
    - 新建 `docs/adr/ADR-044-fifo-bundle-read-data-path.md`：
      - 形态裁决（A 加 `rdata` / B 拆两个 bundle / C 不可行）—— **逐项唯一**
      - **FIFO 协议**：`dout` 在 pop 沿**之后**指向**下一个**队首 ⇒ 消费者 MUST
        在 pop 沿**之前**采样
      - `aread`（0-cycle）vs `sread`（1-cycle）的选择 + 理由
      - 位宽变化（`W+4` → `2W+4`）的**全部**连带影响
  - **verify**:
    ```bash
    test -f docs/adr/ADR-044-fifo-read-data-path.md
    ```

- [ ] ### Task 2: 写会失败的用例（RED）
  - **action**:
    - 在 `tests/test_bundle_master_slave.cpp`（或新建专门文件）加：
      - **行为级**：TB 推入一个元素后，**master 侧 MUST 读到该元素**（读数据通路）
      - **时序级**：pop 沿**之后** `dout` MUST 指向**下一个**队首（协议）
      - ⚠️ MUST 是行为级（驱动 → 读值），MUST NOT 只断言 `impl() != nullptr`
  - **verify**:
    ```bash
    cmake --build build -j$(nproc) --target test_bundle_master_slave
    ./build/tests/test_bundle_master_slave       # MUST 观察到红
    ```

- [ ] ### Task 3: 实现（按 ADR 的裁决）
  - **action**:
    - `include/bundle/common_bundles.h`：`fifo_bundle` 加 `rdata` 字段 + 方向
    - `samples/fifo_bundle.cpp`：读数据通路实现（不再丢弃 `data_out`）
    - `tests/test_bundle_master_slave.cpp`：位偏移常量更新 + 新断言
  - **verify**:
    ```bash
    ./build/tests/test_bundle_master_slave       # MUST 转绿
    ./build/samples/fifo_bundle                   # MUST exit=0
    ```

- [ ] ### Task 4: capability delta + 全量回归 + archive
  - **action**:
    - `bundle-master-slave-io` 的 delta（若字段布局相关的 Requirement 需改）
    - `ctest -LE perf` / `run_all_ported_tests.sh` / `run_all_samples.sh`
    - 计数同步（`CTEST_TOTAL`）
    - `openspec archive fifo-read-data-path-2026-10 -y`
  - **verify**:
    ```bash
    (cd build && ctest -LE perf -j4 --output-on-failure)
    ./run_all_ported_tests.sh
    bash scripts/run_all_samples.sh
    bash scripts/verify_test_counts.sh
    openspec validate --specs --strict
    grep -rn "fifo-read-data-path-2026-10" openspec/specs/   # MUST 命中 0 行
    ```
