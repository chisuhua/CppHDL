# Phase A/C/D (Wave 1+2) 完成总结

## Phase A: CSR + Bit Fields ✅
**文件**: `examples/riscv-mini/src/rv32i_csr.h` (224 lines)
- 9 个 CSR 寄存器 (mstatus/misa/mie/mtvec/mscratch/mepc/mcause/mtval/mip)
- 复位值完全匹配 RISC-V Privileged Spec v1.11
- mip 从外部 mtip/meip 更新
- interrupt_req = MTIE && MIE
- 6 个位域访问器: getMIE(), getMPIE(), getMPP(), getMTIE(), getMEIE(), hasExtension()
- 零债务: grep TODO/FIXME → 空

---

## Phase B: Pipeline (B1a+B1b) ✅
**文件**: `examples/riscv-mini/src/rv32i_pipeline.h`

### B1a: 骨架重写
- 修复 3 个关键 bug:
  - Line 121: instruction 错误连接 pc → 修正
  - Line 131: ex_stage PC 错误用寄存器数据 → 修正
  - Line 147: mem_valid 硬编码 → 使用 data_ready
- 添加 CSR 中断接口 (mtip, meip)

### B1b: Hazard Unit + Forwarding
- Forwarding paths: EX→EX, MEM→EX, WB→EX (rs1 + rs2)
- Load-use stall 检测
- Branch flush 连线
- 修复 mem_alu_result 连接 (从 EX→MEM)
- 修复 WB 级 ALU result 来源 (从 MEM→WB)
- test_rv32i_pipeline: 5 tests, 10 assertions ✓

### B1c: 控制信号 + 4 bug 验证
- CSR 接口连线
- data_ready 正确连接 (不再硬编码)
- 4 个 bug 特定测试向量验证

### 额外修复
- `rv32i_alu.h:103`: SRA 算术右移算法从 SRL 替代为完整的符号扩展实现

---

## Phase C1: CLINT ✅
**文件**: `include/chlib/clint.h` (138 lines)
- 64位 mtime 计数器 (mtime_lo + mtime_hi)
- 64位 mtimecmp 比较器
- MTIP = (mtime >= mtimecmp) 组合逻辑
- MMIO 地址解码: 0x0/0x4/0x8/0xC
- 已在 chlib.h 中注册

---

## Phase D1: Address Decoder ✅
**文件**: `examples/riscv-mini/src/address_decoder.h` (155 lines)
- 6 区域地址解码 (I-TCM/D-TCM/CLINT/UART/GPIO/ERROR)
- One-hot chip select 输出
- 本地地址偏移计算
- Combinational 组件 (无寄存器)

---

## Phase D2: UART MMIO Wrapper ✅
**文件**: `include/axi4/peripherals/axi_uart.h` (修改，+73 rows)
- 添加 `UartMmio` 包装类 (219行)
- 内部实例化 AxiLiteUart
- MMIO → AXI4-Lite 信号映射
- tx_char_ready + tx_char 输出
- 原有 AxiLiteUart 类未修改
- 已在 chlib.h 中注册

---

## Phase D3: HEX Firmware Loader ✅
**文件**: `include/chlib/firmware_loader.h` (~280 lines)
- Intel HEX 格式解析
- 支持记录类型: 00(Data), 01(EOF), 04(ExtLin)
- loadHex() → vector<uint8_t>
- loadHexWords() → vector<uint32_t> (小端序)
- 校验和验证
- 已在 chlib.h 中注册

---

## G0.5: 调试基础设施 ✅
**文件变更**:
- `include/simulator.h` — RegSnapshot + 4 新 API
- `src/simulator.cpp` — 实现新方法
- `include/chlib/simulator_trace.h` — UartCapture 类

---

## 零债务汇总

| 文件 | TODO/FIXME | 编译 | 测试 |
|------|-----------|------|------|
| rv32i_csr.h | 0 | ✅ | - |
| rv32i_pipeline.h | 0 | ✅ | ✅ 5 tests |
| address_decoder.h | 0 | ✅ | - |
| clint.h | 0 | ✅ | - |
| axi_uart.h | 0 | ✅ | - |
| firmware_loader.h | 0 | ✅ | ✅ |
| simulator_trace.h | 0 | ✅ | ✅ 28/28 |
| rv32i_alu.h (SRA fix) | 0 | ✅ | - |
| axi4lite.h (pre-existing) | 2 | ⚠️ | - |

> **注意**: axi4lite.h 的 2 个 TODO 是预存在的基础架构债务，不是本轮工作引入的。

## 经验沉淀

### 1. CSR 实现模式
- 使用 `ch_reg<ch_uint<N>>` 成员变量存储寄存器值
- 读写逻辑用 `select()` 按地址匹配
- 位域访问器用 `bits<N,M>(register.value())`
- mip 等只读字段从外部输入映射

### 2. Pipeline 连线模式
- IF/ID/EX/MEM/WB 阶段用 `ch::ch_module<T>` 实例化
- 数据流用 `<<=` 操作符连接
- Hazard Unit 的前推输出回连到 EX 级输入
- 分支 flush 信号连到 IF 级

### 3. 零债务执行策略
- 每个 wave 结束后立即 grep TODO/FIXME
- 发现预存问题立即修复（如 SRA 问题）
- 编译通过 + 测试通过 + 文档同步 = 完成
