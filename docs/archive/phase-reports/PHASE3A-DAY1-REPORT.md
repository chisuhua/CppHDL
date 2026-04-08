# Phase 3A Day 1 进度报告

**日期**: 2026-04-01  
**阶段**: Phase 3A - RISC-V SoC 集成  
**进度**: Day 1/7 (Week 1)

---

## ✅ 今日完成

### 1. 项目规划 (`docs/PHASE3A-PLAN.md`)

**内容**:
- 3 周执行计划 (Week 1-3)
- 21 个任务分解
- 技术规格定义
- 验收标准

**时间表**:
| 周次 | 阶段 | 目标 |
|------|------|------|
| Week 1 | RV32I 核心 | 指令集 + 流水线 |
| Week 2 | SoC 集成 | AXI4 + SRAM + 外设 |
| Week 3 | 系统验证 | 测试程序 + 报告 |

---

### 2. RV32I 指令集定义 (`include/riscv/rv32i_isa.h`)

**内容**:
- 5 种指令格式 (R/I/S/B/U/J)
- 操作码定义 (LOAD/STORE/OP/BRANCH 等)
- funct3/funct7 定义
- 32 个寄存器定义 (x0-x31)
- 异常码定义
- CSR 寄存器定义

**代码行数**: ~250 行

**指令格式示例**:
```cpp
// R 格式指令 (寄存器 - 寄存器运算)
struct Instr_R {
    ch_uint<7> opcode;
    ch_uint<5> rd;
    ch_uint<3> funct3;
    ch_uint<5> rs1;
    ch_uint<5> rs2;
    ch_uint<7> funct7;
};
```

---

### 3. 寄存器文件 (`include/riscv/rv32i_regs.h`)

**功能**:
- 32×32-bit 寄存器
- 双读端口，单写端口
- x0 硬连线为 0

**代码行数**: ~60 行

**接口**:
```cpp
__io(
    // 读端口 1
    ch_in<ch_uint<5>> rs1_addr;
    ch_out<ch_uint<32>> rs1_data;
    
    // 读端口 2
    ch_in<ch_uint<5>> rs2_addr;
    ch_out<ch_uint<32>> rs2_data;
    
    // 写端口
    ch_in<ch_uint<5>> rd_addr;
    ch_in<ch_uint<32>> rd_data;
    ch_in<ch_bool> rd_we;
)
```

---

### 4. ALU 单元 (`include/riscv/rv32i_alu.h`)

**功能**:
- 加法/减法
- 逻辑运算 (AND/OR/XOR)
- 移位运算 (SLL/SRL/SRA)
- 比较运算 (SLT/SLTU)
- 零标志/小于标志

**代码行数**: ~90 行

**支持的运算**:
| funct3 | 运算 | 说明 |
|--------|------|------|
| 000 | ADD/SUB | 加法/减法 |
| 001 | SLL | 逻辑左移 |
| 010 | SLT | 有符号小于 |
| 011 | SLTU | 无符号小于 |
| 100 | XOR | 异或 |
| 101 | SRL/SRA | 逻辑/算术右移 |
| 110 | OR | 或 |
| 111 | AND | 与 |

---

## 📁 Git 提交

```
9f115a4 feat: 启动 Phase 3A (RISC-V SoC) - 添加 ISA 定义和基础组件
68abfde docs: 添加 Phase 1+2 时序验证报告，为 SPI 示例添加周期断言
f94a6c3 docs: 添加 AXI4-Lite 时序验证报告
```

**新增文件**:
- `docs/PHASE3A-PLAN.md` (4KB)
- `include/riscv/rv32i_isa.h` (7KB)
- `include/riscv/rv32i_regs.h` (2KB)
- `include/riscv/rv32i_alu.h` (3KB)

**总计**: ~16KB, 400 行代码

---

## 📊 进度对比

| 任务 | 计划 | 实际 | 状态 |
|------|------|------|------|
| RV32I 指令集定义 | Day 1 AM | ✅ | 完成 |
| 寄存器文件 | Day 1 AM | ✅ | 完成 |
| ALU 单元 | Day 1 PM | ✅ | 完成 |
| 控制单元 | Day 2-3 | ⏳ | 待开始 |
| 流水线实现 | Day 4-5 | ⏳ | 待开始 |

---

## 🔧 技术亮点

### 1. 指令格式统一

```cpp
// 所有指令格式使用 ch_uint 定义
struct Instr_R {
    ch_uint<7> opcode;   // 操作码
    ch_uint<5> rd;       // 目标寄存器
    ch_uint<3> funct3;   // 功能码 3 位
    ch_uint<5> rs1;      // 源寄存器 1
    ch_uint<5> rs2;      // 源寄存器 2
    ch_uint<7> funct7;   // 功能码 7 位
};
```

### 2. 寄存器文件双读端口

```cpp
// 同时读取两个寄存器
io().rs1_data = select(io().rs1_addr == 0, 0_d, regs[rs1_addr]);
io().rs2_data = select(io().rs2_addr == 0, 0_d, regs[rs2_addr]);
```

### 3. ALU 运算选择

```cpp
// 根据 funct3 选择运算结果
alu_out = select(funct3 == ADD, arith_result, alu_out);
alu_out = select(funct3 == SLL, sll_result, alu_out);
// ...
```

---

## 📋 明日计划 (Day 2)

### 上午：指令译码器
- [ ] 指令格式解析
- [ ] 操作码译码
- [ ] 控制信号生成

### 下午：程序计数器
- [ ] PC 寄存器
- [ ] 分支目标计算
- [ ] 跳转逻辑

### 晚上：集成测试
- [ ] 寄存器文件测试
- [ ] ALU 测试
- [ ] 指令译码测试

---

## 🎯 Week 1 目标

| 组件 | 状态 | 进度 |
|------|------|------|
| ISA 定义 | ✅ 完成 | 100% |
| 寄存器文件 | ✅ 完成 | 100% |
| ALU | ✅ 完成 | 100% |
| 指令译码 | ⏳ 待开始 | 0% |
| 控制单元 | ⏳ 待开始 | 0% |
| 流水线 | ⏳ 待开始 | 0% |
| **Week 1 总计** | ⏳ | **30%** |

---

**报告生成时间**: 2026-04-01 00:15 CST  
**版本**: v1.0 (Phase 3A Day 1)

---

**Phase 3A Day 1 完成！RV32I 基础组件已就绪，明天继续指令译码器和控制单元。** 🚀
