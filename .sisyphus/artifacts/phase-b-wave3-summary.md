# Phase B/Wave3 完成总结

## Wave 3: A3 + C2 + C3 + B2

### A3: Decoder SYSTEM 扩展 ✅
**文件**: `examples/riscv-mini/src/rv32i_decoder.h` (修改 +6 IO ports, +decode logic)
- 新增: csr_addr, is_csr, is_mret, is_ecall, is_ebreak, is_wfi, is_csr_imm
- PRIV 模式检测: funct3==0 && rd==0 && rs1==0
- MRET/ECALL/EBREAK/WFI 根据 funct12 (csr_addr) 区分
- hex 常量使用 `static_cast<uint32_t>(0xNNN)` 格式（项目风格）

### C2: Exception/Trap Unit ✅
**文件**: `examples/riscv-mini/src/rv32i_exception.h` (177 行，新增)
- Trap Entry: 保存 PC 到 mepc, 保存 cause 到 mcause, 跳转到 mtvec
- MRET: 从 mepc 恢复 PC, 恢复 mstatus.MIE/MPIE
- Direct 向量模式 (mtvec[1:0]==0)
- ExceptionUnit 完全自包含，依赖 CSR 接口但不由自己实现

### C3: Pipeline IRQ Pin + PC 修复 ✅
**文件**: `examples/riscv-mini/src/rv32i_pipeline.h` (修改) + `id_stage.h` (修改)
- **关键修复**: id_stage.h 缺少 `pc` 端口 → 添加 `pc` 输入 + ID/EX 流水线寄存器传递
- ExceptionUnit 实例化到管线
- MRET 信号从 decoder 路由到 ExceptionUnit
- 中断检查: CSR interrupt_req → pipeline flush

### B2: Control Unit ✅
**文件**: `examples/riscv-mini/src/rv32i_control.h` (142 行，新增)
- ControlUnit 生成所有管线控制信号
- 9 种指令类型的控制信号表（RegWrite/ALUSrc/MemRead/MemWrite/PCSrc/WBSel/ALU Op）
- 头文件自包含，不依赖其他管线组件

## 零债务检查
| 文件 | TODO/FIXME | 编译 | 测试 |
|------|-----------|------|------|
| rv32i_decoder.h (+A3) | 0 | ⚠️ 预存 operators.h 错误 | ✅ 现有测试通过 |
| rv32i_exception.h (C2) | 0 | ✅ cpphdl target 通过 | - |
| rv32i_control.h (B2) | 0 | ✅ | - |
| id_stage.h (+pc port) | 0 | ✅ | - |
| rv32i_pipeline.h (+C3) | 0 | ✅ | ✅ 5 tests |

> **注意**: operators.h/traits.h 的预存编译错误与本轮无关，不影响现有测试二进制。

## 经验沉淀

### PC 流水线传递架构
- IF 阶段输出 `pc` 寄存器值 → `id_stage.io().pc` 输入
- ID/EX 流水线寄存器 (`idex_pc`) 保存 PC 值传到 EX 级
- EX 级需要 PC 值用于: 分支目标计算 (PC+imm), mepc 保存
- MRET 时 PC 从 mepc 恢复到 IF 阶段的 PC 寄存器

### Exception 单元设计模式
- ExceptionUnit 是 pure combinational 组件 (不存储状态)
- CSR 状态由独立的 CsrBank 管理
- trap_entry 和 mret 分别独立输出
- PC 控制由 IF 阶段统一管理 (flush + 新 PC)

### Decoder SYSTEM 扩展模式
- 使用 bits<N,M>() 提取 csr_addr (instruction[31:20])
- PRIV 模式通过 funct3+rd+rs1 三元检测
- 具体命令 (MRET/ECALL/EBREAK/WFI) 通过 csr_addr 区分
