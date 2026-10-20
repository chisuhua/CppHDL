## Tasks

> 前置依赖：`axi4-peripheral-deferred-2026-10` 必须先 ship
> （archive: `2026-10-20-axi4-peripheral-deferred-2026-10`）——
> 本 change 复用其产出的 `include/axi4/byte_lane.h` 与已登记的字节道语义。
>
> **另一前置（已 ship）**：`axi4-byte-lane-dual-parity-2026-10`
> （archive: `2026-10-20-axi4-byte-lane-dual-parity-2026-10`）——
> 字节道契约已是**双后端**契约：`tests/test_axi_byte_lane.cpp` 由 interpreter-only
> 改为 7 个 `DUAL_BACKEND_TEST_PAIR`（interpreter + verilator）。
> 本 change 新增的通用 slave 用例 MUST 沿用同一模式（见 Task 5），
> 否则 codegen 侧的宽度推导偏差仍无覆盖。
>
> 建议执行顺序：**Task 1 → Task 2 → Task 3 → Task 4 → Task 5 → Task 6 → Task 7**。
> Task 1 是其余任务的前置（掩码实现要能覆盖任意总线宽度）。

- [x] ### Task 1: 泛化 `include/axi4/byte_lane.h` 到任意总线宽度
  - **action**:
    - `byte_lane_mask` 模板化为 `byte_lane_mask<DATA_WIDTH>(wstrb)`：
      `wstrb` 宽度 = `DATA_WIDTH/8`，展开 `DATA_WIDTH/8` 条字节道
    - `byte_lane_valid_mask<W>()` / `byte_lane_apply<W>()` 语义不变
    - 32 bit 路径（现有 6 个 peripheral）行为 MUST NOT 变化
    - ⚠️ `ch_uint<DATA_WIDTH/8>` 是 **non-deduced context** → `DATA_WIDTH` 无法从实参推导，
      既有 6 个调用点 MUST 显式写成 `byte_lane_mask<32>(...)`
    - ⚠️ `ch_uint` 的 `<<` 运算符按 **RHS 的类型宽度**推算结果宽度
      （`operator<<` 的 `rhs_extra_width`）→ 用运行期节点做移位量会把结果宽度算错。
      字节道掩码 MUST 用「编译期常量 → 运行期字面量」构造
      （`ch_uint<W>(0xFFu << (8*i))`），MUST NOT 用运行期移位
  - **verify**:
    - `cd build && ctest -R "test_axi_(byte_lane|gpio|uart|timer|pwm|i2c|spi)" --output-on-failure`
      → 全 PASS（32 bit 路径回归；`test_axi_byte_lane` 现为**双后端**，
      故本条同时回归 interpreter + verilator 两侧）
    - 既有 `byte_lane.h` 的 32 bit 调用点 MUST NOT 需要改签名以外的改动

- [x] ### Task 2: `AxiLiteSlave` 写路径消费 `wstrb`
  - **action**:
    - `include/axi4/axi4_lite_slave.h:84-87` 的 4 个寄存器写路径改用
      `byte_lane_apply<DATA_WIDTH>(reg, io().wdata, wstrb_mask)`
    - 掩码在 W 握手时展开一次（`wstrb` 是组合输入）
  - **verify**:
    - `grep -c 'byte_lane_apply' include/axi4/axi4_lite_slave.h` → MUST ≥ 1
    - 新增用例：部分字节写 → 未选中字节保持原值
    - `cd build && ctest -R "test_axi_lite" --output-on-failure` → PASS
      （含 `test_axi_lite_verilator_e2e`）

- [x] ### Task 3: `Axi4Slave` 写路径消费 `wstrb`（每次 W 握手取当拍值）
  - **action**:
    - `include/axi4/axi4_full.h:228-243` 的 16 个寄存器写路径改用
      `byte_lane_apply<DATA_WIDTH>(reg, io().wdata, wstrb_mask)`
    - 掩码 MUST 在**每次 W 握手**按当拍 `wstrb` 重新展开（MUST NOT 在 AW 拍锁存）
    - ⚠️ 实测结论（2026-10-20）：`Axi4Slave` 写状态机 `wready = (state == 1) && wvalid`
      —— **每笔事务只接受 1 个 W beat**，多 beat burst 的后续 beat 被丢弃。
      多 beat burst 属本 change 非目标（单独立项），用例按"每次 W 握手"验证
    - 不改 burst 的地址递增行为（当前所有 beat 写同一寄存器 —— 非目标）
  - **verify**:
    - `grep -c 'byte_lane_apply' include/axi4/axi4_full.h` → MUST ≥ 1
    - 新增用例：AW 拍与 W 拍给不同 `wstrb`，断言采用 W 拍的值（证明不锁存）
    - `cd build && ctest -R "test_verilator_(memory_e2e|trace_parity)" --output-on-failure` → PASS

- [x] ### Task 4: 文档补两条字节道语义 + 登记通用 slave 契约
  - **action**:
    - `include/axi4/AGENTS.md` CONVENTIONS → 字节道语义：补
      "字节道索引相对被访问的**字**（MUST NOT 相对寄存器重新编号）"
      与 "亚 32 位寄存器位于所在字的首字节道"
    - 同一节登记：`AxiLiteSlave` / `Axi4Slave` 与 6 个 peripheral 遵守**同一套**字节道语义
    - `include/axi4/AGENTS.md` STRUCTURE / KEY FILES 表：补 `byte_lane.h` 的
      "支持任意总线宽度" 说明（若 Task 1 改了职责）
  - **verify**:
    - reviewer 复核：两条语义与 spec 的 Scenario 一一对应

- [x] ### Task 5: 新增字节道测试（**双后端**）
  - **action**:
    - 新增 `tests/test_axi_slave_byte_lane.cpp`，覆盖：
      `AxiLiteSlave` 部分字节写 / `wstrb=0` 空写 /
      `Axi4Slave` 每次 W 握手取当拍 `wstrb`（AW 拍给不同值以证明不锁存）/
      32 bit 与 64 bit 两种总线宽度
    - 在 `tests/CMakeLists.txt` 注册：**`add_dual_backend_test`**（不是
      `add_catch_test`）—— 字节道契约已是双后端契约
      （见前置 change `axi4-byte-lane-dual-parity-2026-10`）；
      窄寄存器截断与 `byte_lane_mask<DATA_WIDTH>` 泛化后的掩码展开，
      在 Verilog codegen 路径上与 interpreter 差异最大
    - 用 typedef 规避宏参数逗号切分（`AxiLiteSlave<...>` / `Axi4Slave<...>`）
  - **verify**:
    - `cd build && ctest -R test_axi_slave_byte_lane --output-on-failure` → PASS
      （`--list-tests` MUST 出现 `[verilator]` 半边，且**不得**是 WARN-skip）
    - 既有 6 个 peripheral 的字节道用例（`test_axi_byte_lane`，双后端）MUST 仍 PASS

- [x] ### Task 6: 全量回归 + 计数同步
  - **action**:
    - 计数同步（若新增测试文件）：CTEST_TOTAL / non-perf / `tests/test_*.cpp` 数量
      MUST 与根 `AGENTS.md` NOTES §Test counts 一致
  - **verify**:
    - `cd build && ctest -LE perf --output-on-failure` → 全 PASS
    - `cd build && ctest -L verilator_e2e -LE perf --output-on-failure` → 全 PASS
    - `./run_all_ported_tests.sh` → 28/28 PASS
    - `bash scripts/verify_test_counts.sh` → exit 0

- [x] ### Task 7: openspec archive（**禁用 `--skip-specs`**）
  - **action**:
    - `openspec archive axi4-slave-byte-lane-2026-10 --yes`
  - **verify**:
    - `ls openspec/changes/archive/ | grep slave-byte-lane` → 命中
    - `openspec validate --specs --strict` → 全 PASS
