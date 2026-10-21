## Tasks

> 前置依赖：无硬依赖。但 MUST 在 `axi4-lite-generic-slave-conformance-2026-10`
> （archive `2026-10-21-axi4-lite-generic-slave-conformance-2026-10`）之后执行 ——
> `AxiToAxiLiteConverter` 的 `rst_n` 透传与相位分离状态机在该 change 中落地，
> `examples/axi4/phase3a_complete.cpp` 的 AXI-Lite 路径只有在其之后才可能真的跑通。
>
> **执行顺序**：Task 0 → Task 1 → Task 2 → Task 3 → Task 4 → Task 5 → Task 6 →
> Task 7 → Task 8 → Task 9。
>
> **TDD 纪律**：Task 2 先写会失败的用例（RED），Task 3-6 逐条转绿（GREEN），
> Task 7-9 回归。每个 Task 的 verify 必须是**可执行命令**，
> 不接受「人工检查通过」作为唯一验收。
>
> ⚠️ **verify 必须是本机实测过的命令**（`openspec validate` 只做结构/标题校验）。
> 已知踩坑（承自既往 change）：
>
> | 踩坑 | 后果 | 正确写法 |
> |---|---|---|
> | `add_dual_backend_test` 写成位置参数 | **全仓 configure FATAL_ERROR**（该函数用 `cmake_parse_arguments`，只认 `SOURCES`） | `add_dual_backend_test(name SOURCES file.cpp TIMEOUT 300)` |
> | 多路选择写成「先赋 m0 再读回 `io().s0_awaddr`」 | 输出读自己 → 拓扑排序报 `Cycle detected in IR graph` | 每路 **单次赋值**（见 `AxiLiteMatrix2x2:226-233` 的修复注释） |
> | 用 `grep -c "io().m1_wready ="` 统计赋值 | 端口声明行也会命中 → 计数虚高 → 验收条件永真 | `grep -cE "^[[:space:]]*io\(\)\.m1_wready *="` |
> | `ctest -R "…interconnect…"` 目标不存在 | 静默跳过（假绿） | 先用 `ctest -N -R` 断言命中数 |
>
> **proposal → task 映射**：D1 → Task 3；D2 → Task 4；D3 → Task 5；D4 → Task 6；
> D5 → Task 6；D6 → Task 7；D7 → Task 2 / Task 3-6 / Task 8；D8 → Task 9。

- [ ] ### Task 0: spec 冲突消解 + 变更影响面清点
  - **action**:
    - 本 change 的 spec delta 为 **1 个新 capability**：`specs/axi-interconnect-routing/spec.md`
      （ADDED ×5）
    - 确认主 spec 中 **无** 已有 requirement 管辖互连层
      （`axi-slave-conformance` 管从设备侧、`dual-backend-test-coverage` 管双后端一致性）
      → 若发现冲突，MUST 在本 Task 记录并与 `axi-slave-conformance` 的边界写清
    - 清点 `AxiInterconnect4x4` / `AxiLiteMatrix2x2` 的实例化点
      （预期：`examples/axi4/phase3a_complete.cpp:75`、
      `examples/axi4/axi_soc_example.cpp:195`）
    - `openspec validate --strict` MUST 通过
  - **verify**:
    ```bash
    openspec validate --strict && echo VALIDATE_OK
    grep -rn "AxiInterconnect4x4\|AxiLiteMatrix2x2" --include=*.cpp .
    ```

- [ ] ### Task 1: 建立「缺陷可观测」基线（不改实现）
  - **action**:
    - 写一个**最小复现**程序（可放在 `/tmp` 或临时 testbench），
      逐条确认 C1-C4 的症状确实存在：
      1. m1 发起写 → `m1_awready` 恒 0（写不被接受）
      2. m1 发起读 → `m1_arready` 恒 0
      3. slave 1 的 `s1_arvalid` 恒 0（读地址到不了 slave 1）
      4. slave 1 的 `s1_bready` 恒 0（响应永不握手）
    - 把复现步骤记入本 Task 的完成说明（供 Task 2 的 RED 用例对齐）
  - **verify**:
    ```bash
    grep -cE "^[[:space:]]*io\(\)\.m1_(wready|arready|bready|rready) *=" include/axi4/axi_interconnect_4x4.h
    grep -cE "^[[:space:]]*io\(\)\.s1_(araddr|arvalid|bready|rready) *=" include/axi4/axi_interconnect_4x4.h
    ```
    两条 MUST 均为 **0**（确认缺陷存在，Task 3-4 之后 MUST 变为非 0）

- [ ] ### Task 2: 写会失败的路由用例（RED）
  - **action**:
    - 新建 `tests/test_axi_interconnect_4x4.cpp` 与
      `tests/test_axi_lite_matrix_2x2.cpp`，用
      `tests/dual_backend_fixture.h::DUAL_BACKEND_TEST_PAIR` macro
    - 先写下列用例（此时**全部应失败**）：
      1. 4 个 master 各自到达全部 4 个 slave（16 对全扇出）
      2. 地址映射正确（每个 slave 拿到自己窗口内的地址）
      3. 写事务 AW → W → B 全链路，**数据断言**
      4. 读事务 AR → R 全链路，**数据断言**
      5. 未映射地址 → `DECERR`
      6. 写/读并发（不同 master 同时发起）不串扰
      7. 路由锁存：AW 握手后改写 AWADDR，数据仍落到原 slave
      8. W 通道不被仲裁轮转改投
      9. `WSTRB` 完整转发
      10. 读事务不被写请求阻塞
    - 在 `tests/CMakeLists.txt` 用 `add_dual_backend_test` 注册
      （**MUST 用 `SOURCES` 关键字形式**）
  - **verify**:
    ```bash
    cmake -B build && cmake --build build -j$(nproc) --target test_axi_interconnect_4x4 test_axi_lite_matrix_2x2
    cd build && ctest -N -R "test_axi_interconnect_4x4|test_axi_lite_matrix_2x2" | tail -3
    ```
    MUST 命中预期数量；**此时测试 MUST 是红的**（RED 基线）

- [ ] ### Task 3: 引入路由锁存（写侧 + 读侧）
  - **action**:
    - `AxiInterconnect4x4` 每个 master 引入：
      `write_route`（2 bit）+ `write_active`、`read_route`（2 bit）+ `read_active`
    - 路由 MUST 在 AW / AR 握手当拍由**该通道自己的地址**解码并锁存
    - 写侧与读侧 MUST 各自独立锁存
    - 形态参照 `AxiLiteMatrix2x2` 已验证的 `m0_route_s1` / `m1_route_s1`
      （`include/axi4/axi4_lite_matrix.h:146-149`、`:212-213`），但 MUST 覆盖写读两个方向
  - **verify**:
    ```bash
    grep -cE "route" include/axi4/axi_interconnect_4x4.h
    cd build && ctest -R test_axi_interconnect_4x4 --output-on-failure
    ```
    Task 2 的用例 7 MUST 转绿

- [ ] ### Task 4: 16 对全扇出（AW / W / AR 正向 + B / R 反向）
  - **action**:
    - 补齐 C1-C4 的全部缺失赋值：
      - `m1/m2/m3` 的 `wready` / `arready` / `bready` / `rready`
      - `s1/s2/s3` 的 `araddr` / `arid` / `arlen` / `arsize` / `arburst` / `arvalid`
      - `s1/s2/s3` 的 `bready` / `rready`
    - AW / W / AR 三个正向通道 MUST 覆盖全部 16 对
    - B / R 反向通道 MUST 按**写/读路由锁存**反查来源
    - 每次 mux MUST **单次赋值**（MUST NOT 出现输出读自己的自环）
    - `WSTRB` MUST 随 W 通道一起转发
  - **verify**:
    ```bash
    grep -cE "^[[:space:]]*io\(\)\.m1_(wready|arready|bready|rready) *=" include/axi4/axi_interconnect_4x4.h
    grep -cE "^[[:space:]]*io\(\)\.s1_(araddr|arvalid|bready|rready) *=" include/axi4/axi_interconnect_4x4.h
    cd build && ctest -R test_axi_interconnect_4x4 --output-on-failure
    ```
    前两条 MUST 由 0 变为非 0；Task 2 的用例 1 / 2 / 3 / 4 / 6 / 9 MUST 转绿

- [ ] ### Task 5: 独立仲裁器（AW / AR 各一套）
  - **action**:
    - 拆成 2 个 `ch_reg<ch_uint<2>>` 指针 + 各自的 grant：
      写仲裁用 `awvalid`，读仲裁用 `arvalid`
    - 轮转条件 MUST 是**事务真正完成**
      （写：slave 的 `bvalid && bready`；读：slave 的 `rvalid && rready`），
      MUST NOT 用「某个 slave 的 `awready` 为 1」
    - 在途事务的 master MUST 保持授权（`active` 优先于轮询）
    - MUST NOT 出现「所有 grant 恒为 0」的饿死状态
  - **verify**:
    ```bash
    cd build && ctest -R test_axi_interconnect_4x4 --output-on-failure
    ```
    Task 2 的用例 8 / 10 MUST 转绿

- [ ] ### Task 6: 地址映射参数化 + DECERR + 死代码清理
  - **action**:
    - 把硬编码的 `awaddr >> 30` 改为**编译期可配置**的窗口映射
      （base / size 或等价形式）
    - 默认值 MUST 与当前 `>>30` 行为**等价**（避免破坏
      `examples/axi4/phase3a_complete.cpp`）
    - MUST 存在未映射区域 → 返回 `DECERR`
    - 读通道 MUST 用**读地址**解码
    - 删除 `build_decoder()` 空壳死代码（`include/axi4/axi_interconnect_4x4.h:210-221`）
    - 删除本 change 中被替代的任何旧路径（MUST NOT 留死代码 —— ZERO-DEBT POLICY）
  - **verify**:
    ```bash
    grep -c "build_decoder" include/axi4/axi_interconnect_4x4.h
    cd build && ctest -R test_axi_interconnect_4x4 --output-on-failure
    ```
    第 1 条 MUST 为 0；Task 2 的用例 5 MUST 转绿

- [ ] ### Task 7: `AxiLiteMatrix2x2` 的活地址解码修正
  - **action**:
    - `m0_awready` / `m1_awready`（`include/axi4/axi4_lite_matrix.h:251-252`）
      MUST 基于**锁存的写路由**，MUST NOT 基于活的 `awaddr`
    - 读侧引入 `ar_route` 锁存（2x2 当前完全没有）；
      `m*_rdata` / `m*_rresp` / `m*_rvalid` / `s*_rready` MUST 基于该锁存
    - `s*_rready` MUST NOT 用**写**通道的 grant（`:326-329`）
  - **verify**:
    ```bash
    cd build && ctest -R test_axi_lite_matrix_2x2 --output-on-failure
    ```
    Task 2 的 2x2 用例 MUST 全绿

- [ ] ### Task 8: 回归 —— 既有测试与 examples 必须仍然全绿
  - **action**:
    - `examples/axi4/phase3a_complete.cpp`（4×4）与
      `examples/axi4/axi_soc_example.cpp`（2×2）MUST 仍可编译
    - `phase3a_complete` 的 `main()` 当前**只构建不仿真** ——
      本 change MUST 至少让它的 AXI-Lite 路径**真的跑起来**并断言结果
      （否则改了也无法验证）
    - `run_all_ported_tests.sh` 跟踪的 examples MUST 全绿
  - **verify**:
    ```bash
    cmake --build build -j$(nproc) && ./run_all_ported_tests.sh
    cd build && ctest -L verilator_e2e -R "interconnect|matrix" --output-on-failure
    ```
    MUST 全绿

- [ ] ### Task 9: 文档同步 + 测试计数更新
  - **action**:
    - `include/axi4/AGENTS.md`：
      - 新增「互连层」段（地址映射、路由锁存、仲裁、DECERR 语义）
      - STRUCTURE 表中 `axi_interconnect_4x4.h` / `axi4_lite_matrix.h` 的行注 MUST 更新
      - TESTS 表 MUST 列出两个新测试文件
    - 根 `AGENTS.md`：
      - NOTES 中「`axi4-interconnect-routing-2026-10` —— 4×4 全路由」MUST 改写为已实施
      - `CTEST_TOTAL` / non-perf 计数 MUST 与 `cd build && ctest -N | tail -1` 一致
    - `scripts/verify_test_counts.sh` MUST 通过
  - **verify**:
    ```bash
    ./scripts/verify_test_counts.sh && cd build && ctest -N | tail -1
    ```
    MUST 全绿且计数一致
