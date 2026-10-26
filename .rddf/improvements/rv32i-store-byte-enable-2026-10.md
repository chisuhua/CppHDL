---
name: rv32i-store-byte-enable-2026-10
priority: P0
source: 2026-10-26 roadmap 审计（feat-rv32i-isa-completeness ①）
phase: phase-5
category: cpu
type: bugfix
created_at: 2026-10-26
---

# Improvement: RV32I 的 `sb` / `sh` MUST 只写选中的字节道

- **Date drafted**: 2026-10-26
- **Drafted by**: build-agent (builder role)
- **Status**: 📋 5-segment draft；change `rv32i-store-byte-enable-2026-10` 已实施（待对抗审稿 + 归档）
- **来源**: 2026-10-26 的 roadmap 对齐审计（实测挖出）—— 此前只登记在根
  `AGENTS.md §KNOWN LIMITATIONS` 的「RV32I 的 ISA 覆盖边界」条目里。

## 1. Why (背景与动机)

`mem_stage` **早已**按 `funct3` 正确算出字节道掩码并输出 `store_byte_enable_out`，
`Rv32iPipeline` 也**早已**把它转成 `data_strbe` —— 但：

- `include/cpu/pipeline/rv32i_tcm.h` 的 **`DataTCM` 没有 `strbe` 端口**
  ⇒ `data_strbe` 在 SoC 里**没有任何消费者**；
- `examples/riscv-mini/src/rv32i_pipeline_top.h` 也**不接线**。

⇒ `sb` / `sh` 在 SoC 里**完全不生效**：`ch_mem::write` 只写**整字**
（`include/core/mem.h`）⇒ 字节写会把**整字**覆盖（不是「写错车道」，是「没写」）。

更隐蔽的是**第二个缺口**：`Rv32iPipeline::data_write_data` 直接接 `exmem_rs2_data`，
**没有**按 `addr[1:0]` 做 lane 对齐 ⇒ 即使补上 `strbe`，`sb` 到偏移 1 时也会把
`rs2` 的**低字节**写进 lane 1（而不是 lane 1 拿到 `rs2[7:0]`）⇒
「掩码对了、数据在错 lane」是比「字节写没生效」**更难发现**的错误。

**Why now**：C1/C2/C3 已完成（单一实现 + 逐拍观测面 32 端口 + 双路 parity）⇒
这是修 ISA 语义的**最佳时机**（有金标可对照）；再往后每加一层都让回归面更大。

## 2. What Changes (变更范围)

1. **`DataTCM` 新增 `strbe` 输入端口**（`include/cpu/pipeline/rv32i_tcm.h`）
   - 契约：`addr` 是**字地址**、`wdata` **已 lane 对齐**、`strbe` 是车道掩码
   - 写路径改为**读-改-写**：`aread`（0-cycle 组合读）→ 按 `strbe` 合并 → 整字写回
   - 复用 `include/axi4/byte_lane.h` 的 `byte_lane_mask` / `byte_lane_apply`
     （与 AXI 侧**同一份**字节道语义实现）
2. **流水线侧 lane 对齐**（`include/cpu/pipeline/rv32i_pipeline.h`）
   - `data_write_data <<= shl<32>(exmem_rs2_data, lane_shift)`
   - `lane_shift = 8 * addr[1:0]`，**只对 SB/SH**（funct3 = 000/001）
   - ⚠️ `sw` **保持原样**（含非对齐 `sw` 的既有行为）⇒ 零行为回退
3. **SoC 接线**（`examples/riscv-mini/src/rv32i_pipeline_top.h`）：
   `dtcm.io().strbe <<= pipeline.io().data_strbe;`
4. **全部既有实例化点 MUST 驱动新端口**：`test_pipeline_exec.cpp`（3 处）、
   `test_riscv_tests_pipeline.cpp`（1 处）
   - ⚠️ 实测发现：**未驱动的输入端口在本框架里读到的是「全 1」**（不是 0）⇒
     恰好等价于整字写 ⇒ **测试会「碰巧」通过** ⇒ 未定义行为依赖
     （interpreter 与 verilator 的未驱动初值未必相同）⇒ MUST 显式接线
5. **测试**（`examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp`）
   - 固件尾部追加 `sb`（lane 1）与 `sh`（lane 2-3）各一条 + 两条 `lw` 读回整字
   - golden：store 拍的**写数据**必须已 lane 对齐（`3<<8` / `13<<16`）+
     读回的**整字**（`0x030A` / `0xD0000`）
   - 反向断言：**未对齐**的写数据（`3` / `13`）MUST NOT 出现

## 3. Acceptance (验收标准)

- [x] `sb`/`sh` 在 SoC 级真正生效：`sb x2(=3) -> [0x101]` ⇒ 字 0x100 = `0x030A`；
      `sh x3(=13) -> [0x122]` ⇒ 字 0x120 = `0xD0000`
- [x] 写数据**已 lane 对齐**（store 拍上是 `0x300` / `0xD0000`，不是 `3` / `13`）
- [x] 未对齐的写数据 MUST NOT 出现在 trace 里（反向断言）
- [x] 双后端（interpreter + verilator）**逐拍全等**（无掩码）
- [x] `sw` 行为**零变化**（既有 3 个 store 的 golden 与计数不变）
- [x] 全部 `DataTCM` 实例化点显式驱动 `strbe`
- [x] `ctest -LE perf` 零回归

## 4. Capabilities (涉及的能力维度)

- **新 capability `rv32i-isa-semantics`** —— 字节道写语义（源数据低位对齐 + 掩码合并）
- `rv32i-commit-path`（SoC 观测面契约**未变**：32 端口不变 ⇒ **无需**改 ADR-046 D10）
- `cpu-cache-subsystem`（复用 `byte_lane.h` 的字节道语义）

## 5. Impact (影响面分析)

- **改动文件**：`include/cpu/pipeline/rv32i_tcm.h`、`include/cpu/pipeline/rv32i_pipeline.h`、
  `examples/riscv-mini/src/rv32i_pipeline_top.h`、
  `examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp`、
  `examples/riscv-mini/tests/test_pipeline_exec.cpp`、
  `examples/riscv-mini/tests/test_riscv_tests_pipeline.cpp`
- **API 变更**：`DataTCM` **新增一个输入端口** ⇒ 公共 API 面变化 ⇒
  全部实例化点 MUST 审计（本 change 已逐个处理）
- **风险**：**中** —— 改的是存储写语义；但
  ① `sw` 路径零变化（掩码 0xF ⇒ 合并结果 = wdata）；
  ② 既有 golden（3 个 store 的地址/值/计数）全部保持 ⇒ 回归面被既有断言钉住
- **未做（明确非目标）**：
  - 不改 `ch_mem`（无字节使能写 API）⇒ 合并在 `DataTCM` 内做
  - 不做非对齐 `sw` 的异常处理（保持既有行为）
  - 不引入 AXI 互连（SoC 的 dmem 仍是 TCM）
