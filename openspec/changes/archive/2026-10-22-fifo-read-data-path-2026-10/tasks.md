# Change: fifo-read-data-path-2026-10 — `fifo_bundle` 的**读数据通路**

> **状态：已实施（2026-10-22）。**
> 立项自 `bundle-ecosystem-hardening-2026-10` 的 L10（用户裁决独立立项）。
> ⚠️ **公开 API 布局变更** ⇒ Task 1 的 ADR MUST **先于**任何代码改动落地。
> ⚠️ **ADR-044 编号可用**（`docs/adr/` 现存最大编号 ADR-043）。
> ⚠️ **TDD 排序**（Task 2）：行为级 RED 用例引用尚不存在的 `io.rdata` ⇒
> **编译错误**而非「测试红」⇒ 单一 target 编译断会拖垮既有 8 个用例 ⇒
> 走**路线 1**（先加字段 → 编译恢复绿 → 再加断言 ⇒ **断言红**）。

- [x] ### Task 0: 清点影响面（ADR 的输入）
  - **action**:
    - 清点 `fifo_bundle` 的**全部**使用点（字段布局变化 ⇒ 位偏移全变）：
      ```bash
      grep -rn "fifo_bundle" --include=*.cpp --include=*.h include/ tests/ examples/ samples/ src/ docs/
      ```
      （⚠️ 原命令**漏了** `src/` 与 `docs/`）
    - 清点**位偏移常量**的使用点（`kFifoPushBit` / `kFifoFullBit` / ...）：
      ```bash
      grep -rn "kFifo.*Bit\|bundle_width_v<fifo_bundle" --include=*.cpp tests/
      ```
    - 清点 `ch_stream` 的 `payload` 是否构成同病（**预期：不构成** —— stream 协议
      规定 `payload` 由 master 驱动）⇒ 在 ADR 里写明该清点结论
    - ⚠️ **实测已有结论（2026-10-22，落地前请复核）**：
      `fifo_bundle` 共 **9** 个文件 / 约 **49** 处引用；其中 **6 处是硬编码的
      宽度/字段数断言**（`tests/test_bundle_bitstream.cpp:85`/`:93`/`:99`/`:109` /
      `tests/test_bundle_advanced_features.cpp:75`（**`STATIC_REQUIRE`** ⇒ 编译期断）/
      `tests/test_bundle_ecosystem.cpp:487`）+ **1 处位偏移常量块**
      （`tests/test_bundle_master_slave.cpp:178-184`）
  - **verify**:
    ```bash
    test -f docs/reports/verification-fifo-read-data-path-2026-10.md
    ```

- [x] ### Task 1: 写 ADR-044（**6 项**裁决）
  - **action**:
    - 新建 `docs/adr/ADR-044-fifo-read-data-path.md`：
      1. **形态**裁决（A 加 `rdata` / B 拆两个 bundle / C 不可行）—— 逐项唯一；
      2. ⚠️ **`rdata` 的字段位置**裁决 —— **决定全部位偏移**：
         **追加末尾** ⇒ 既有 5 个偏移**完全不变**（`rdata` 占 bit 12..19）；
      3. **FIFO 协议**：`dout` 在 pop 沿**之后**指向**下一个**队首 ⇒ 消费者 MUST
         在 pop 沿**之前**采样；
      4. **`aread`（0-cycle）vs `sread`（1-cycle）**的选型 + 理由
         （⚠️ 现状 sample 用 `aread` —— 那是**描述**不是裁决，ADR MUST 明确裁决）；
      5. ⚠️ **`empty` 时 `rdata` 是否 don't-care** —— MUST 定义，否则消费者会
         latch 垃圾；
      6. ⚠️ **打包 API 的宽度边界** —— `W+4` → `2W+4` ⇒ `W ∈ [31,60]` 从
         「可用」变成「**显式报错**」（`include/simulator.h:236-242`）⇒ 裁决
         （接受回退 + 写明逐字段替代路径 + 在 capability 里记录）。
  - **verify**:
    ```bash
    test -f docs/adr/ADR-044-fifo-read-data-path.md
    # 6 项裁决 MUST 全部出现
    grep -c "rdata" docs/adr/ADR-044-fifo-read-data-path.md
    ```

- [x] ### Task 2: 写会失败的用例（RED）—— 走**路线 1**
  - **action**:
    - ⚠️ **问题**：行为级 RED 用例必然引用 `io.rdata` ⇒ 字段尚不存在 ⇒
      **编译错误**（不是「测试红」）⇒ 而 `test_bundle_master_slave` 是**单一
      target**（8 个 TEST_CASE）⇒ 编译断 ⇒ 既有 8 个用例**全部无法运行**。
    - **路线 1（采纳）**：**先加字段**（编译恢复绿）→ **再写行为/时序断言**
      ⇒ 断言因「读通路未接通」而**红** ⇒ 既有 8 个用例全程可运行。
    - 断言 MUST 是**行为级**（驱动 → 读值）：
      - **行为级**：TB 推入一个元素后，**master 侧 MUST 读到该元素**（读数据通路）
      - **时序级**：pop 沿**之后** `rdata` MUST 指向**下一个**队首（协议）
      - ⚠️ MUST NOT 只断言 `impl() != nullptr`
    - **变异验证**（已完成）：把 DUT 的 `io.rdata <<= mem.aread(rd_a);` 去掉
      ⇒ 该用例**转红**（1 failed / 33 assertions）⇒ 证明它是真护栏
  - **verify**:
    ```bash
    cmake --build build -j$(nproc) --target test_bundle_master_slave
    ./build/tests/test_bundle_master_slave       # 变异时 MUST 观察到红
    ```

- [x] ### Task 3: 实现（按 ADR 的裁决）
  - **action**:
    - `include/bundle/common_bundles.h`：`fifo_bundle` 加 `rdata` 字段 + 方向
      （master IN / slave OUT）+ ADR-044 的引用注释
    - `tests/test_bundle_master_slave.cpp`：
      - 位偏移常量**追加** `kFifoRdataBit` / `kFifoRdataBits` / `kFifoRdataMask`
        （既有 5 个**不变** —— 追加末尾的收益）
      - DUT 接上读通路：`io.rdata <<= mem.aread(rd_a);`
      - **方向元数据断言补 `rdata`**（master IN / slave OUT）
      - 新增「读数据通路」用例（行为级 + 时序级）
    - `samples/fifo_bundle.cpp`：读数据通路实现（不再丢弃 `data_out`）
      + 顶层加 `ch_out<ch_uint<2>> rdata` 端口 + **写死期望值表**
      （该 sample 是 `tick()` **之后**才采样 ⇒ 极易差一拍）
    - `samples/minimal_fifo_bundle.cpp`：MUST **显式驱动** `rdata`（否则新字段恒 0）
      + 顺带修 `io.full = false; io.empty = true;`（**节点重绑定** ⇒ ADR-042 已把
      `operator=` 定性为「不是连接手段」）
    - ⚠️ **6 处硬编码断言同步**（`test_bundle_bitstream.cpp:85,93,99,109` /
      `test_bundle_advanced_features.cpp:75` / `test_bundle_ecosystem.cpp:487`）
      ⇒ 不改则**当场红**（其中 1 处是 `STATIC_REQUIRE` ⇒ 编译期断）
      ⚠️ **实测更正**：提案原表只列了 4 处 ⇒ `tuple_size_v` 那 **2 处**漏网 ⇒
      `ctest` 报 `test_bundle_bitstream` 失败（`6 == 5`）⇒ 才被补上
    - ⚠️ **生成 Verilog 的验收**（已完成）：`samples/fifo_bundle.cpp` 生成的
      `fifo_bundle.v` MUST 含 `logic [1:0] top_rdata;` +
      `assign top_fifo_inst_mem_aread_data_proxy = top_fifo_inst_fifo_mem[...];`
      + `assign top_rdata = top_fifo_inst_mem_aread_data_proxy;`
      且 MUST NOT 引用未声明信号 —— 实测**无未声明引用** ✓
  - **verify**:
    ```bash
    ./build/tests/test_bundle_master_slave       # MUST 转绿
    ./build/samples/fifo_bundle                   # MUST exit=0
    ./build/samples/minimal_fifo_bundle           # MUST exit=0
    ```

- [x] ### Task 4: capability delta + 全量回归 + archive
  - **action**:
    - `bundle-master-slave-io` 的 delta（**必然**，见 proposal D3）：
      - 「方向指认」Requirement：Scenario 补 `rdata` 行
      - **新增** MODIFIED「打包式 bundle API 的适用范围」Requirement ⇒ 写明
        `fifo_bundle` 的 `W ≥ 31` 落在该适用范围之外（ADR-044 裁决 6）
      - **新增** Scenario「读数据的**时序** MUST 与 FIFO 协议一致」
        （0-cycle aread / pop 沿前采样 / `empty` 时 don't-care）
      - ⚠️ delta MUST **删掉自认暂定的措辞** ⇒ 改为「MUST 与 ADR-044 的裁决一致；
        ADR 未裁决前 MUST NOT 归档」
      - ⚠️ delta 的「读数据 MUST 有独立字段」Scenario MUST **限定作用域**
        （否则含读写两侧的 `axi_lite_bundle` 变成违规 ⇒ 与 Non-Goal 1 冲突）
    - `ctest -LE perf` / `run_all_ported_tests.sh` / `run_all_samples.sh`
    - 计数同步（⚠️ **按新增 TEST_CASE 数**同步 `CTEST_TOTAL` + NOTES +
      `scripts/verify_test_counts.sh` —— 本次是**在既有 target 里加用例**
      ⇒ `CTEST_TOTAL` **不变**）
    - `openspec archive fifo-read-data-path-2026-10 -y`
  - **verify**:
    ```bash
    (cd build && ctest -LE perf -j4 --output-on-failure)
    ./run_all_ported_tests.sh
    bash scripts/run_all_samples.sh
    bash scripts/verify_test_counts.sh
    # ⚠️ 不得用 `openspec validate --specs --strict` 当验收 ——
    # 它在 pristine HEAD 上就是红的（CLI 1.14.3 实测：3 passed, 15 failed）⇒ 不具判别力
    openspec validate fifo-read-data-path-2026-10 --strict
    grep -rn "fifo-read-data-path-2026-10" openspec/specs/   # MUST 命中 0 行
    ```
