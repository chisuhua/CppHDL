# Phase 0.5: Debug Infrastructure - Artifacts

## Phase 0.5 (已完成) + Phase A/Wave 1 (部分完成) 总结

### G0.5: 调试基础设施
**文件变更**:
- `include/simulator.h` — 新增 RegSnapshot 结构体, get_riscv_snapshot(), dump_state(), get_signal_by_path(), set_trace_config() 共 5 个新 API
- `src/simulator.cpp` — 实现上述 4 个方法
- `include/chlib/simulator_trace.h` — 新增 UartCapture 类 (UART 控制台捕获工具)
- `include/chlib.h` — 注册 simulator_trace.h

**经验总结**:
- Simulator 已有完整的 VCD dump 实现 (`toVCD()`)，无需重写
- 新增 API 利用现有 `data_map_` 和 `signals_` 基础设施
- UartCapture 是 header-only 工具类，无外部依赖

---

### Wave 1 (4/4 完成)

#### A1: CSR Register Bank ✅
- 文件: `examples/riscv-mini/src/rv32i_csr.h` (224 行)
- 9 个 CSR 寄存器，复位值完全符合 RISC-V Privileged Spec v1.11
- mip 从外部 mtip/meip 更新，interrupt_req = MTIE && MIE
- 测试: 0 TODO/FIXME，编译通过，现有测试无回归

#### B1a: Pipeline Skeleton ✅
- 文件: `examples/riscv-mini/src/rv32i_pipeline.h` (修改)
- 修复 3 个关键 bug:
  - Line 121: instruction 错误连接 pc → 修正为 instruction
  - Line 131: ex_stage PC 错误用寄存器数据 → 修正为 id_stage.io().pc
  - Line 147: mem_valid 硬编码 true → 修正为 data_ready
- 测试: test_rv32i_pipeline 通过 (5 tests, 10 assertions)

#### C1: CLINT Module ✅
- 文件: `include/chlib/clint.h` (138 行)
- 64位 mtime/mtimecmp，mtip = (mtime >= mtimecmp)
- MMIO 地址解码用 select() 实现
- 已在 chlib.h 中注册

#### D1: Address Decoder ✅
- 文件: `examples/riscv-mini/src/address_decoder.h` (155 行)
- 6 区域地址解码 (I-TCM/D-TCM/CLINT/UART/GPIO/ERROR)
- One-hot chip select 输出
- 已注册到 riscv-mini CMakeLists

### 零债务检查
- 编译: ✅ 0 错误，新文件 0 警告
- 测试: ✅ 所有现有测试通过
- TODO/FIXME: ✅ grep 结果为空
- 文档: ⚠️ AGENTS.md 待更新 (Phase A/B/C/D API 章节)

### 下一步: Wave 2
- A2: mstatus/misa 位域访问器
- B1b: Hazard Unit + Forwarding
- B1c: Control Signal Wiring (含 4 bug 微门验证)
- D2: UART 模型 (修改现有 axi_uart.h)
- D3: HEX Loader
