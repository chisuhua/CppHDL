# 提案：RV32I 的 `sb` / `sh` MUST 只写选中的字节道

## Why

2026-10-26 的 roadmap 对齐审计实测挖出：`sb` / `sh` 在 RV32I SoC 里**完全不生效**。

事实链（逐条可核）：

1. `include/cpu/pipeline/stages/mem_stage.h:182-200` **早已**按 `funct3` 正确算出
   车道掩码（`sb_be` 按 `byte_offset`、`sh_be` 按 `half_offset`）并输出
   `store_byte_enable_out`；
2. `include/cpu/pipeline/rv32i_pipeline.h:369` **早已**把它转成 `data_strbe`；
3. ⚠️ 但 `include/cpu/pipeline/rv32i_tcm.h` 的 `DataTCM` **没有 `strbe` 端口**，
   `examples/riscv-mini/src/rv32i_pipeline_top.h` 也**不接线**
   ⇒ **`data_strbe` 全仓没有任何消费者**；
4. `ch_mem::write(addr, data, en)` 只写**整字**（`include/core/mem.h:195`）
   ⇒ 字节写要么被忽略、要么整字覆盖；
5. ⚠️ **第二个缺口**：`Rv32iPipeline::data_write_data` 直接接 `exmem_rs2_data`，
   **未**按 `addr[1:0]` lane 对齐 ⇒ 即使补上 `strbe`，`sb` 到偏移 1 也会把
   `rs2` 的**低字节**写进 lane 1 ⇒ 「掩码对了、数据在错 lane」是更隐蔽的错误。

⇒ 本 change 把「字节道写」补成**符合 RISC-V 语义**的读-改-写。

**为什么现在**：C1/C2/C3 已完成（单一实现 + 32 端口观测面 + 双路 parity）⇒
有金标可对照，是修 ISA 语义的最佳时机。

## What Changes

### 1. `DataTCM` 新增 `strbe` 输入端口 + 读-改-写（`include/cpu/pipeline/rv32i_tcm.h`）

- 契约：`addr` 是**字地址**（SoC 侧已 `addr >> 2`）、`wdata` **已 lane 对齐**、
  `strbe` 是车道掩码
- 写路径：`aread`（0-cycle 组合读，与 MEM 级同拍采样一致）→
  `byte_lane_mask<32>(strbe)` → `byte_lane_apply<32,32>(old, wdata, mask)` →
  `write` 整字写回
- ⚠️ 复用 `include/axi4/byte_lane.h`（与 AXI 侧**同一份**字节道语义实现）

### 2. 流水线侧 lane 对齐（`include/cpu/pipeline/rv32i_pipeline.h`）

- `data_write_data <<= shl<32>(exmem_rs2_data, lane_shift)`
- `lane_shift = 8 * addr[1:0]`，**只对 SB/SH**（funct3 = 000/001）
- ⚠️ `sw` **保持原样**（含非对齐 `sw`）⇒ **零行为回退**
- ⚠️ 移位量是**运行期节点** ⇒ MUST 用 `shl<N>`（显式结果宽度）；
  `operator<<` 按 RHS 的**类型宽度**推算结果宽度 ⇒ 运行期移位量会把宽度算错

### 3. SoC 接线（`examples/riscv-mini/src/rv32i_pipeline_top.h`）

`dtcm.io().strbe <<= pipeline.io().data_strbe;`

### 4. 全部既有实例化点 MUST 驱动新端口

- `examples/riscv-mini/tests/test_pipeline_exec.cpp`（3 处）
- `examples/riscv-mini/tests/test_riscv_tests_pipeline.cpp`（1 处）
- ⚠️ **实测发现**：未驱动的输入端口在本框架里读到的是**全 1**（不是 0）⇒
  恰好等价于「全车道写」⇒ 整字写用例会**碰巧**通过 ⇒ 那是**未定义行为依赖**
  （interpreter 与 verilator 的未驱动初值未必相同）⇒ MUST 显式接线

### 5. 测试（`examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp`）

- 固件**尾部**追加（既有 word 索引一个都不移动）：
  `sb x2(=3) -> [0x101]`（lane 1）→ `lw x12 <- [0x100]` ⇒ `0x030A`；
  `sh x3(=13) -> [0x122]`（lane 2-3）→ `lw x13 <- [0x120]` ⇒ `0xD0000`
- golden：store 拍的**写数据**必须已 lane 对齐（`0x300` / `0xD0000`）
- 反向断言：**未对齐**的写数据（`3` / `13`）MUST NOT 出现
- store 拍计数 3 → 5

## Acceptance

- [x] `sb` / `sh` 在 SoC 级真正生效（读回的整字 = `0x030A` / `0xD0000`）
- [x] 写数据**已 lane 对齐**（store 拍上是 `0x300` / `0xD0000`）
- [x] 未对齐的写数据 MUST NOT 出现在 trace 里
- [x] 双后端逐拍全等（无掩码）
- [x] `sw` 行为零变化（既有 3 个 store 的 golden 与计数不变）
- [x] 全部 `DataTCM` 实例化点显式驱动 `strbe`
- [x] `ctest -LE perf` 零回归
- [ ] **对抗审稿**（critic）+ `openspec archive`

## Non-Goals

- 不改 `ch_mem`（不引入字节使能写 API）⇒ 合并在 `DataTCM` 内做
- 不做非对齐 `sw` 的异常处理（保持既有行为）
- 不把 SoC 的 dmem 换成 AXI 互连（那是另一个 change）
- 不改 ADR-046 **D10** 的 SoC 32 端口契约（本 change 不新增顶层端口）
