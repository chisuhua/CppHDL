# T401: 5 级流水线实现计划

**创建日期**: 2026-04-09  
**执行人**: DevMate  
**预计工期**: 3 天  
**优先级**: P0 (核心任务)

---

## 📋 任务分解

### T401.1: IF 级 (取指) - 4 小时

**文件**: `include/riscv/stages/if_stage.h`

**功能**:
- PC 寄存器 (32 位)
- 指令存储器接口
- PC+4 计算
- 分支跳转处理

**IO 端口**:
```cpp
__io(
    // 输入
    ch_in<ch_uint<32>> branch_target;    // 分支目标地址
    ch_in<ch_bool> branch_valid;         // 分支有效
    ch_in<ch_bool> stall;                // 停顿信号
    
    // 输出
    ch_out<ch_uint<32>> pc;              // 当前 PC
    ch_out<ch_uint<32>> instruction;     // 取出的指令
    ch_out<ch_bool> valid;               // 有效信号
)
```

**验收标准**:
- [ ] PC 寄存器正确实现
- [ ] 指令读取正确
- [ ] 分支跳转功能正常
- [ ] 停顿控制正常

---

### T401.2: ID 级 (译码) - 4 小时

**文件**: `include/riscv/stages/id_stage.h`

**功能**:
- 指令译码 (Opcode + Funct3 + Funct7)
- 寄存器文件读 (2 个读端口)
- 符号扩展/零扩展
- 控制信号生成

**IO 端口**:
```cpp
__io(
    // 输入
    ch_in<ch_uint<32>> instruction;      // 来自 IF
    ch_in<ch_bool> valid;                // 有效信号
    
    // 输出
    ch_out<ch_uint<5>> rs1_addr;         // RS1 地址
    ch_out<ch_uint<5>> rs2_addr;         // RS2 地址
    ch_out<ch_uint<32>> rs1_data;        // RS1 数据
    ch_out<ch_uint<32>> rs2_data;        // RS2 数据
    ch_out<ch_uint<32>> imm;             // 立即数
    ch_out<ch_uint<4>> opcode;           // 操作码
    ch_out<ch_bool> is_branch;           // 分支指令
    ch_out<ch_bool> is_load;             // 加载指令
    ch_out<ch_bool> is_store;            // 存储指令
    ch_out<ch_bool> is_jump;             // 跳转指令
)
```

**验收标准**:
- [ ] R/I/S/B/U/J 型指令译码正确
- [ ] 寄存器文件读正确
- [ ] 立即数扩展正确
- [ ] 控制信号生成正确

---

### T401.3: EX 级 (执行) - 8 小时

**文件**: `include/riscv/stages/ex_stage.h`

**功能**:
- ALU 计算
- 分支条件判断
- 分支目标计算
- Forwarding 多选器

**IO 端口**:
```cpp
__io(
    // 输入
    ch_in<ch_uint<32>> rs1_data;         // 来自 ID
    ch_in<ch_uint<32>> rs2_data;         // 来自 ID
    ch_in<ch_uint<32>> imm;              // 来自 ID
    ch_in<ch_uint<4>> opcode;            // 来自 ID
    ch_in<ch_bool> is_branch;            // 分支标志
    ch_in<ch_bool> is_load;              // 加载标志
    ch_in<ch_bool> is_store;             // 存储标志
    ch_in<ch_uint<3>> funct3;            // 功能码
    
    // 输出
    ch_out<ch_uint<32>> alu_result;      // ALU 结果
    ch_out<ch_bool> branch_taken;        // 分支跳转
    ch_out<ch_uint<32>> branch_target;   // 分支目标
    ch_out<ch_bool> zero;                // 零标志
)
```

**验收标准**:
- [ ] ALU 支持所有 RISC-V 操作
- [ ] 分支条件判断正确
- [ ] Forwarding 多选器正确
- [ ] 零标志生成正确

---

### T401.4: MEM 级 (访存) - 4 小时

**文件**: `include/riscv/stages/mem_stage.h`

**功能**:
- 数据存储器接口
- 加载/存储处理
- 数据转发

**IO 端口**:
```cpp
__io(
    // 输入
    ch_in<ch_uint<32>> alu_result;       // 来自 EX
    ch_in<ch_uint<32>> rs2_data;         // 来自 EX (存储数据)
    ch_in<ch_bool> is_load;              // 加载标志
    ch_in<ch_bool> is_store;             // 存储标志
    ch_in<ch_uint<3>> funct3;            // 数据宽度
    
    // 输出
    ch_out<ch_uint<32>> mem_data;        // 从内存读出的数据
    ch_out<ch_bool> mem_valid;           // 内存访问有效
)
```

**验收标准**:
- [ ] LB/LH/LW/LBU/LHU正确
- [ ] SB/SH/SW 正确
- [ ] 数据存储转发正确

---

### T401.5: WB 级 (写回) - 4 小时

**文件**: `include/riscv/stages/wb_stage.h`

**功能**:
- 写回数据多选器
- 写使能控制
- 写回寄存器地址

**IO 端口**:
```cpp
__io(
    // 输入
    ch_in<ch_uint<32>> alu_result;       // 来自 MEM
    ch_in<ch_uint<32>> mem_data;         // 来自 MEM
    ch_in<ch_bool> is_load;              // 加载标志
    ch_in<ch_bool> is_jump;              // 跳转标志
    ch_out<ch_uint<5>> rd_addr;          // 目标寄存器地址
    ch_out<ch_uint<32>> write_data;      // 写回数据
    ch_out<ch_bool> write_en;            // 写使能
)
```

**验收标准**:
- [ ] ALU 结果/加载数据选择正确
- [ ] 写使能控制正确
- [ ] x0 寄存器不写入保护

---

### T401.6: 冒险检测 - 8 小时

**文件**: `include/riscv/hazard_unit.h`

**功能**:
- 数据冒险检测
- Forwarding 逻辑
- 停顿 (Stall) 生成
- 刷新 (Flush) 生成

**冒险类型**:
1. **RAW (Read After Write)**: 
   - EX/MEM/WB 级结果转发到 EX 级输入
2. **Load-Use Hazard**:
   - 加载指令后需要停顿 1 周期
3. **Control Hazard**:
   - 分支判断后刷新 IF/ID 级

**验收标准**:
- [ ] Forwarding 逻辑正确
- [ ] Load-Use 停顿正确
- [ ] 分支刷新正确
- [ ] 无冒险相关错误

---

## 📁 文件结构

```
include/riscv/
├── stages/
│   ├── if_stage.h          # 取指级
│   ├── id_stage.h          # 译码级
│   ├── ex_stage.h          # 执行级
│   ├── mem_stage.h         # 访存级
│   └── wb_stage.h          # 写回级
├── hazard_unit.h           # 冒险检测单元
└── rv32i_pipeline.h        # 顶层流水线模块
```

---

## 🔌 接口设计

### 流水线寄存器

```cpp
// IF/ID 寄存器
struct IF_ID_Reg {
    ch_uint<32> pc;
    ch_uint<32> instruction;
    ch_bool valid;
};

// ID/EX 寄存器
struct ID_EX_Reg {
    ch_uint<32> pc;
    ch_uint<32> rs1_data;
    ch_uint<32> rs2_data;
    ch_uint<32> imm;
    ch_uint<5> rd_addr;
    ch_uint<4> opcode;
    ch_uint<3> funct3;
    ch_bool is_branch;
    ch_bool is_load;
    ch_bool is_store;
    ch_bool is_jump;
    ch_bool valid;
};

// EX/MEM 寄存器
struct EX_MEM_Reg {
    ch_uint<32> alu_result;
    ch_uint<32> rs2_data;
    ch_uint<5> rd_addr;
    ch_bool is_load;
    ch_bool is_store;
    ch_bool branch_taken;
    ch_bool valid;
};

// MEM/WB 寄存器
struct MEM_WB_Reg {
    ch_uint<32> alu_result;
    ch_uint<32> mem_data;
    ch_uint<5> rd_addr;
    ch_bool is_load;
    ch_bool is_jump;
    ch_bool valid;
};
```

---

## 🧪 测试计划

### 单元测试

```cpp
// test_if_stage.cpp
TEST_CASE("IF Stage - Basic Fetch") {
    // 测试基本取指
}

TEST_CASE("IF Stage - Branch") {
    // 测试分支跳转
}

// test_id_stage.cpp
TEST_CASE("ID Stage - R-Type Decode") {
    // 测试 R 型指令译码
}

TEST_CASE("ID Stage - I-Type Decode") {
    // 测试 I 型指令译码
}

// test_pipeline.cpp
TEST_CASE("Pipeline - ADD Sequence") {
    // 测试 ADD 指令序列
}

TEST_CASE("Pipeline - Data Hazard") {
    // 测试数据冒险处理
}

TEST_CASE("Pipeline - Load-Use Hazard") {
    // 测试 Load-Use 冒险
}

TEST_CASE("Pipeline - Branch") {
    // 测试分支指令
}
```

### 性能基准

```cpp
// 测试 IPC 提升
TEST_CASE("Pipeline - IPC Measurement") {
    // 单周期：IPC ~0.5
    // 5 级流水线：IPC ~0.8
}
```

---

## 📊 验收标准

### 功能验收

- [ ] 所有 RISC-V 基础指令正确执行
- [ ] 数据冒险正确处理
- [ ] 控制冒险正确处理
- [ ] Load-Use 冒险正确停顿
- [ ] 无死锁/活锁

### 性能验收

- [ ] IPC 提升 >50% (0.5 → 0.8)
- [ ] 频率目标 >100MHz
- [ ] 流水线吞吐率 ~1 IPC

### 代码质量

- [ ] 编译无警告
- [ ] 测试覆盖率 >90%
- [ ] 代码符合 CppHDL 规范

---

## 📅 执行时间表

| 日期 | 任务 | 状态 |
|------|------|------|
| Day 1 AM | T401.1: IF 级 | ⏳ |
| Day 1 PM | T401.2: ID 级 | ⏳ |
| Day 2 AM | T401.3: EX 级 | ⏳ |
| Day 2 PM | T401.4: MEM 级 | ⏳ |
| Day 3 AM | T401.5: WB 级 + T401.6: 冒险检测 | ⏳ |
| Day 3 PM | 集成测试 + 性能基准 | ⏳ |

---

**创建人**: DevMate  
**创建日期**: 2026-04-09  
**状态**: 🟡 准备执行
