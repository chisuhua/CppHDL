## Tasks

> 前置依赖：`axi4-slave-byte-lane-2026-10`（archive `2026-10-20-axi4-slave-byte-lane-2026-10`）
> —— 本 change 在其产出的字节道语义之上继续收敛协议规范形态。
>
> **执行顺序**：Task 0 → Task 1 → Task 2 → Task 3 → Task 4 → Task 5 → Task 6 →
> Task 7 → Task 8 → Task 9 → Task 10 → Task 11。
> Task 0（实例化点盘点）必须最先做 —— 加 `rst_n` 端口会牵动全部实例化点。

- [ ] ### Task 0: 实例化点盘点 + 决策点确认
  - **action**:
    - 盘点 `AxiLiteSlave` / `Axi4Slave` 的**全部**实例化点，逐个确认
      `rst_n` 接线位置（未驱动的 `ch_in<ch_bool>` 读出为 0 = 永久复位）：
      `examples/axi4/{axi4_lite_example,axi4_full_example,phase3a_complete}.cpp`、
      `tests/{test_axi_lite_verilator_e2e,test_verilator_memory_e2e,test_verilator_trace_parity,
      test_axi_slave_byte_lane}.cpp`
    - 确认两处**决策点**（MUST 在动手前定，避免返工）：
      1. `NUM_REGS` 模板参数：真正生效，还是删除该参数（当前是"接口谎言"）
      2. `AxiInterconnect4x4` 的 m1/m2/m3 → s1/s2/s3 通路：补全（保持 4×4 名义）
         还是把组件降级为 1×4（则需改名 + 文档同步）
  - **verify**:
    - `grep -rn "AxiLiteSlave\|Axi4Slave" tests/ examples/ include/` → 全部实例化点
      逐个在 Task 1 的 checklist 上列出

- [ ] ### Task 1: `AxiLiteSlave` 收敛到规范形态
  - **action**:
    - 新增 `ch_in<ch_bool> rst_n` 端口；4 个 `ch_reg` 全部加
      `select(!io().rst_n, <init_val>, ...)` 复位 pattern
      （`<init_val>` MUST 带显式宽度，如 `ch_uint<DATA_WIDTH>(0_d)`）
    - 新增 `aw_addr_reg` / `ar_addr_reg`：AW/AR 握手时锁存地址；
      写/读解码 MUST 基于**锁存**地址，MUST NOT 基于活的 `io().awaddr` / `io().araddr`
    - `bvalid` 改为寄存器：写握手时置位，**保持到 `bready`**，握手后清零
      （MUST NOT 组合单拍 —— 见 `include/axi4/AGENTS.md` anti-pattern）
    - 未映射地址 → `bresp` / `rresp` = `DECERR`，寄存器值不变
  - **verify**:
    - 新增用例（双后端）：AW 握手后改写 `awaddr` → 写目标不变
    - 新增用例：master 不拉 `BREADY` → `bvalid` 保持为 1（不消失）
    - 新增用例：未映射地址 → `bresp == DECERR`
    - `grep -nE 'addr_bits = io\(\)\.aw?r?addr' include/axi4/axi4_lite_slave.h` → MUST 0 行

- [ ] ### Task 2: `AxiLiteSlave` 的 `NUM_REGS` 处理（决策点 1）
  - **action**:
    - 按 Task 0 的决策二选一：
      (a) 真正生效：寄存器组按 `NUM_REGS` 生成（模板元编程或 `std::index_sequence`），
          地址解码范围随之扩大；
      (b) 删除该模板参数（同步改所有实例化点与文档）
    - 同一处理适用于 `Axi4Slave`（见 Task 4）
  - **verify**:
    - 选用 (a) 时：`AxiLiteSlave<32,32,8>` 的第 5..8 个寄存器可写可读
    - 选用 (b) 时：`grep -rn "NUM_REGS" include/axi4/` → 0 处残留

- [ ] ### Task 3: `Axi4Slave` 加 `rst_n` + 复位 pattern
  - **action**:
    - 新增 `ch_in<ch_bool> rst_n` 端口；`SlaveState` 内**全部** `ch_reg`
      （16 个寄存器 + 状态/计数/地址寄存器）加
      `select(!io().rst_n, <init_val>, ...)` 复位 pattern
  - **verify**:
    - 新增用例：`rst_n = 0` 时所有寄存器读回初值（双后端）
    - `grep -n "rst_n" include/axi4/axi4_full.h` → 端口 + 复位 pattern 均命中

- [ ] ### Task 4: `Axi4Slave` 写路径规范形态 + burst 语义
  - **action**:
    - 新增 `aw_addr_reg`：AW 握手时锁存 `io().awaddr`；
      写目标 MUST 基于锁存地址（MUST NOT 用活的 `io().awaddr`）
    - 修写状态机：W 通道在 `aw_wait` **与** `burst_data` 两态都可握手，
      直到 `wlast` → 后续 beat MUST NOT 被静默丢弃
    - burst 地址推进：每个 beat 的目标地址按 `awburst` 推进
      （INCR → +`DATA_WIDTH/8` 字节；FIXED → 不变）
    - 字节道语义保持：每个 beat MUST 用**该 beat**的 `wstrb`
      （`byte_lane_apply<DATA_WIDTH, DATA_WIDTH>`，掩码在 W 握手拍展开）
    - 未映射地址 → `bresp` = `DECERR`（MUST NOT 静默丢弃写）
  - **verify**:
    - 新增用例（双后端）：`awlen = 1` 的 2-beat 写，两 beat 用不同 `wstrb` →
      两个 beat 各自只改选中字节；两个 beat 落到**不同**寄存器（INCR）
    - 新增用例：AW 握手后改写 `awaddr` → 写目标不变
    - 新增用例：未映射地址写 → `bresp == DECERR` 且目标寄存器不变
    - `grep -nE 'addr_index = io\(\)\.awaddr' include/axi4/axi4_full.h` → MUST 0 行

- [ ] ### Task 5: `Axi4Slave` 读 burst 语义
  - **action**:
    - `read_burst_cnt` MUST 在 R_DATA 态按 R 握手递增（当前只在 idle 态递增 → 永不匹配）
    - `arlen` 匹配（MUST 含末拍计数）后回 idle
    - `rdata` MUST 按**当前 beat 的地址**更新（当前恒为首拍锁存地址的值）
    - `rlast` MUST 在最后一拍为 1（当前恒 0）
  - **verify**:
    - 新增用例（双后端）：`arlen = 1` 的 2-beat 读 → 两次 `rdata` 分别是两个寄存器的值，
      末拍 `rlast == 1`，且 slave 回到 idle（能接受下一笔事务）
    - 新增用例：读事务结束后 `rvalid` 归 0（当前 `arlen > 0` 时永久保持）

- [ ] ### Task 6: 总线层路由完整度（4×4 interconnect + 2×2 matrix）
  - **action**:
    - `include/axi4/axi_interconnect_4x4.h`：补 m1/m2/m3 → s1/s2/s3 的全通道路由，
      与 m0 通路**同构**：AW（addr/id/len/size/burst/valid）、W（data/**wstrb**/wid/wlast/valid）、
      B（ready）、AR（addr/id/len/size/burst/valid）、R（ready/data/valid/last）
    - ⚠️ **`wstrb` 通路 MUST 与 AW/W 通路同步补全** —— 否则修好的路由会让下游 slave
      收到 `wstrb == 0`，把合法写变成**空写**（静默丢数据）
    - `include/axi4/axi4_lite_matrix.h`：复核 2×2 的同类通路是否完整
    - 按 Task 0 的决策 2 处理 4×4 的"名义 vs 实际"问题
  - **verify**:
    - 新增用例：m1/m2/m3 各自经 s1/s2/s3 完成一次写 + 读，数据正确
    - 新增用例：经 interconnect 的**部分字节写**生效（`wstrb` 透传正确）
    - `grep -n "s1_wstrb" include/axi4/axi_interconnect_4x4.h` → 必须是多主 mux，
      不能只接 `m0_wstrb`

- [ ] ### Task 7: 死代码清理
  - **action**:
    - `byte_lane_write()`：删除，或补真实调用点 + 用例（当前零调用点）
    - `Axi4Slave::write_burst_cnt`：Task 4 之后若仍无用途 → 删除
  - **verify**:
    - `grep -rn "byte_lane_write" include/ tests/ examples/` → 0 处（删除）或 ≥1 处真实调用
    - ZERO-DEBT：无孤儿函数、无未用状态寄存器

- [ ] ### Task 8: 单元测试补齐（4×4 interconnect / 2×2 matrix 当前零单测）
  - **action**:
    - 新增 `tests/test_axi_interconnect_4x4.cpp`（或并入既有 AXI 测试文件）：
      路由正确性 + `wstrb` 透传 + 仲裁轮转
    - 新增 2×2 matrix 的路由用例
    - 全部走 `add_dual_backend_test`（双后端）
  - **verify**:
    - `cd build && ctest -R "test_axi_(interconnect|matrix)" --output-on-failure` → PASS
    - `--list-tests` MUST 出现 `[verilator]` 半边且非 WARN-skip

- [ ] ### Task 9: 双后端覆盖补齐
  - **action**:
    - Task 1/3/4/5 引入的新行为 MUST 有双后端用例
      （`DUAL_BACKEND_TEST_PAIR`，interpreter + verilator）
    - 既有 `tests/test_axi_slave_byte_lane.cpp` 按新规范（`rst_n` 接线、地址锁存、
      响应保持）同步更新
  - **verify**:
    - `cd build && ctest -L verilator_e2e -LE perf --output-on-failure` → 全 PASS
    - `./build/tests/test_axi_slave_byte_lane "*verilator*"` → 真实 dlopen，非 WARN-skip

- [ ] ### Task 10: 文档同步
  - **action**:
    - `include/axi4/AGENTS.md`：
      - CONVENTIONS 新增「通用 AXI slave 规范形态」小节（`rst_n` / 地址锁存 /
        响应保持 / 未映射 `DECERR` / burst 语义）
      - ANTI-PATTERNS 新增对应 verify 命令（如
        `grep -nE 'addr_bits = io\(\)\.aw?r?addr' include/axi4/axi4_{lite_slave,full}.h` → MUST 0 行）
      - STRUCTURE / KEY FILES 同步 `rst_n` 端口与新行为
    - 根 `AGENTS.md`：测试计数同步 + NOTES 新增条目
  - **verify**:
    - reviewer 复核：文档条目与 spec 的 Scenario 一一对应

- [ ] ### Task 11: 全量回归 + 计数同步 + archive
  - **action**:
    - 计数同步（若新增测试文件）：`CTEST_TOTAL` / non-perf /
      `tests/test_*.cpp` 数量与根 `AGENTS.md` NOTES §Test counts 一致
    - `openspec archive axi4-generic-slave-conformance-2026-10 --yes`
      （**禁用 `--skip-specs`** —— 本 change 含 ADDED Requirements，必须合并进主 spec）
  - **verify**:
    - `cd build && ctest -LE perf --output-on-failure` → 全 PASS
    - `./run_all_ported_tests.sh` → 28/28 PASS
    - `bash scripts/verify_test_counts.sh` → exit 0
    - `openspec validate --specs --strict` → 全 PASS
    - `ls openspec/changes/archive/ | grep generic-slave-conformance` → 命中
