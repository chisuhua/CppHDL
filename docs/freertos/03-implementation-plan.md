# riscv-mini FreeRTOS 实施计划

**版本**: 1.1  
**创建日期**: 2026-04-13  
**评审日期**: 2026-04-14  
**状态**: 评审后修订 (Momus 7 Critical + 9 Major, Metis 战略调整)  
**总工期**: 19-22 工作日 (原 14-19, 评审后调整)

---

## 1. 概述

本文档定义从当前状态到 FreeRTOS 成功运行的**5 阶段实施计划**，每个阶段有明确的：
- 交付物
- 验收标准
- 工时估算
- 依赖关系

---

## 2. 阶段总览

> **修订 v1.1**: 根据 Momus#6.1 建议，Phase B→A 顺序调整为 A→B→C。Momus 建议 B→A 有道理但 A（CSR）是纯寄存器定义不依赖 pipeline 执行，B（Pipeline）虽然可以独立测试但 CSR 定义是前置知识。保持 A→B，但将 Phase B 估算从 3-4 天调整为 5-7 天。

```
Phase A: CSR + SYSTEM 指令    (4-5 天)  ─────┐
                                            │
Phase B: Pipeline 全集成     (5-7 天)  ─────┤
                                            ├──→ Phase C → Phase D
Phase C: CLINT + 中断         (3-4 天)  ─────┘
                                            │
Phase D: SoC + Bare-metal   (3-4 天)  ─────┘
    "Hello World"                           │
                                            │
Phase E: FreeRTOS Port (可选) (7-14 天) ←───┘
```

> **变更说明**:
> - Phase E 改为**可选**，CPU 正确性 Gate 在 Phase D (bare-metal Hello World)
> - riscv-tests ISA 套件作为 Phase D.5 验证 Gate
> - ELF Loader 延后到 Phase F (增强)，Phase D 使用 HEX/Binary 优先

### 关键路径

> **修订 v1.1**: 修正 Momus#1.1 依赖图错误 — B 和 A 是并行可独立开始的，但 C 需要 A+B 都完成。

```
A (CSR) ──→ C (Interrupts) ──→ D (SoC) ──→ E (FreeRTOS, 可选)
           ↑
B (Pipeline) ──→──────────────┘
```

---

## 3. Phase A: CSR + SYSTEM 指令

> **评审修订 v1.1**: 工时从 2-3 天调整为 4-5 天。Momus#2.1 指出这是从零开始的全新实现，不是对现有代码的修改。

**工期**: 4-5 工作日 (原 2-3)  
**前置条件**: Phase 1-3 已通过 (当前状态 ✅)  
**输出**: CSR 寄存器组 + 11 条 SYSTEM 指令

### 3.1 任务分解

| ID | 任务 | 文件 | 工时 | 依赖 |
|----|------|------|------|------|
| A1 | CSR 寄存器组定义 | `src/rv32i_csr.h` | 1d | 无 |
| A2 | misa + mstatus 实现 | `src/rv32i_csr.h` | 0.5d | A1 |
| A3 | Decoder SYSTEM 扩展 | `src/rv32i_decoder.h` | 1d | A2 |
| A4 | SYSTEM 指令执行 | `src/rv32i_system.h` | 1d | A3 |
| A5 | CSR 单元测试 | `tests/test_rv32i_csr.cpp` (新建) | 1d | A4 |

### 3.2 详细设计

#### A1: CSR 寄存器组 (`rv32i_csr.h`)

```cpp
class CsrBank : public ch::Component {
public:
    __io(
        // CSR 接口
        ch_in<ch_uint<12>> csr_addr;
        ch_in<ch_uint<32>> csr_wdata;
        ch_in<ch_bool> csr_write_en;
        ch_out<ch_uint<32>> csr_rdata;
        
        // 中断接口
        ch_in<ch_bool> mtip;  // Machine Timer
        ch_in<ch_bool> meip;  // Machine External
    )
    
    void describe() override {
        // mstatus: MIE(3), MPIE(7), MPP(11:12)
        ch_reg<ch_uint<32>> mstatus{"mstatus", 0x00001880};
        ch_reg<ch_uint<32>> mie{"mie", 0};
        ch_reg<ch_uint<32>> mtvec{"mtvec", 0x20000100};
        ch_reg<ch_uint<32>> mscratch{"mscratch", 0};
        ch_reg<ch_uint<32>> mepc{"mepc", 0};
        ch_reg<ch_uint<32>> mcause{"mcause", 0};
        ch_reg<ch_uint<32>> mtval{"mtval", 0};
        ch_reg<ch_uint<32>> mip{"mip", 0};
        ch_reg<ch_uint<32>> misa{"misa", 0x40101101};  // RV32I
        
        // 注意: mtime/mtimecmp 不在 CSR Bank 中!
        // 它们是 CLINT 内存映射寄存器 (0x40000000, 0x40000008)
        
        // CSR 读写逻辑
        auto matching = (csr_addr == 0x300);  // mstatus
        io().csr_rdata = select(matching, mstatus.read(), ch_uint<32>(0_d));
        
        when(io().csr_write_en && matching) {
            mstatus.write(io().csr_wdata);
        }
    }
};
```

#### A3: SYSTEM 指令执行 (`rv32i_system.h`)

```cpp
class SystemExecutor : public ch::Component {
public:
    __io(
        // Decoder 输出
        ch_in<ch_uint<32>> instruction;
        ch_in<ch_bool> is_system;
        
        // CSR Bank
        ch_inout<CsrBank> csr;
        
        // 寄存器文件
        ch_in<ch_uint<32>> rs1_data;
        ch_out<ch_uint<32>> rd_wdata;
        ch_out<ch_bool> rd_write_en;
        
        // PC 控制
        ch_out<ch_uint<32>> mret_pc;
        ch_out<ch_bool> is_mret;
    )
    
    void describe() override {
        // SYSTEM opcode = 0b1110011
        auto opcode = bits<6, 0>(io().instruction);
        auto funct3 = bits<14, 12>(io().instruction);
        auto csr_addr = bits<31, 20>(io().instruction);
        auto rs1 = bits<19, 15>(io().instruction);
        auto rd = bits<11, 7>(io().instruction);
        
        // CSRRW: funct3 = 001
        auto is_csrrw = (funct3 == ch_uint<3>(1_d));
        // CSRRS: funct3 = 010
        auto is_csrrs = (funct3 == ch_uint<3>(2_d));
        // CSRRC: funct3 = 011
        auto is_csrrc = (funct3 == ch_uint<3>(3_d));
        
        // MRET: opcode=SYSTEM, funct3=000, rs1=00000, rd=00000
        auto is_mret = (opcode == 0b1110011_d) && 
                       (funct3 == 0_d) && 
                       (rs1 == 0_d) && (rd == 0_d);
        
        // CSRRW 执行
        when(is_system && is_csrrw) {
            auto old_val = csr.read(csr_addr);
            csr.write(csr_addr, io().rs1_data);
            io().rd_wdata <<= old_val;
            io().rd_write_en = ch_bool(true);
        }
        
        // MRET 执行
        when(is_mret) {
            io().mret_pc <<= csr.mepc.read();
            io().is_mret = ch_bool(true);
            csr.mstatus.MIE.write(csr.mstatus.MPIE.read());
            csr.mstatus.MPIE.write(ch_bool(true));
        }
    }
};
```

### 3.3 测试用例

#### test_rv32i_csr.cpp

```cpp
TEST_CASE("CSR Read/Write", "[csr][system]") {
    ch::core::context ctx("csr_test");
    ch::core::ctx_swap swap(&ctx);
    
    CsrBank csr(nullptr, "csr");
    
    // 写 mstatus (复位值=0x00000080, MPIE=1)
    csr.write(0x300, 0x00000088);  // MIE=1, MPIE=1
    sim.tick();
    
    // 读回验证
    auto mstatus = csr.read(0x300);
    REQUIRE(mstatus.value() == 0x00000088);
}

TEST_CASE("CSRRS/Set Bits", "[csr][system]") {
    // CSRRS rd, mie, t0  (t0=0x00000080 -> set MTIE)
    csr.write(0x304, 0x00000000);  // mie=0
    // 执行 CSRRS: mie |= 0x80
    // 验证：mie == 0x80, rd == old_value(0)
}
```

### 3.4 Gate 标准

- [ ] `test_rv32i_csr.cpp`: 5 tests, 20+ assertions
- [ ] `test_mret.cpp`: MRET 后 PC==mepc, mstatus.MIE==MPIE
- [ ] 编译 0 warnings
- [ ] CI 通过

---

## 4. Phase B: Pipeline 全集成

> **评审修订 v1.1**: 工时从 3-4 天调整为 5-7 天。Momus#2.2 指出 rv32i_pipeline.h 现有 4 处严重 bug（line 121 PC≠instruction, line 131 用寄存器数据作为 PC, line 166-168 冒险检测完全 break, line 147 内存永远 ready）。这本质上是重新设计，不是修复。

**工期**: 5-7 工作日 (原 3-4)  
**前置条件**: Phase A 完成  
**输出**: 完整的 5 阶段流水线，可执行指令序列

### 4.1 任务分解

| ID | 任务 | 文件 | 工时 | 依赖 |
|----|------|------|------|------|
| B1 | Pipeline 顶层重写 | `src/rv32i_pipeline.h` | 2d | Phase A |
| B2 | 控制单元 | `src/rv32i_control.h` | 1.5d | B1 |
| B3 | I-TCM 集成 | `src/rv32i_tcm.h` | 0.5d | B1 |
| B4 | D-TCM 集成 | `src/rv32i_tcm.h` | 0.5d | B1 |
| B5 | 执行测试 (新建) | `tests/test_pipeline_execution.cpp` | 1.5d | B1-B4 |

### 4.2 关键修复

#### B1: Pipeline 顶层接线

当前 `rv32i_pipeline.h` 的问题:
- `mem_read_data` 硬编码为 `0_d`
- `io().instr_addr` 无连接
- PC 更新逻辑缺失

修复后:
```cpp
void describe() override {
    // IF 级连接
    if_stage.io().rst <<= io().rst;
    if_stage.io().stall <<= hazard.io().stall;
    if_stage.io().flush <<= hazard.io().flush;
    if_stage.io().branch_target <<= ex_stage.io().branch_target;
    if_stage.io().pc_src <<= hazard.io().pc_src;
    
    // I-TCM 连接
    if_stage.io().instr_addr <<= io().instr_addr_out;
    io().instr_addr_out <<= if_stage.io().pc;
    if_stage.io().instr_data <<= io().instr_data_in;
    
    // 级间连接
    id_stage.io().instruction <<= if_stage.io().instruction;
    ex_stage.io().rs1_data <<= id_stage.io().rs1_data;
    ex_stage.io().rs2_data <<= id_stage.io().rs2_data;
    mem_stage.io().alu_result <<= ex_stage.io().result;
    wb_stage.io().mem_result <<= mem_stage.io().read_data;
    
    // 写回 MUX
    auto wb_data = select(ex_stage.io().mem_to_wb, 
                          wb_stage.io().mem_result,
                          ex_stage.io().result);
    
    // RegFile 写入
    id_stage.io().rd_wdata <<= wb_data;
    id_stage.io().rd_write_en <<= wb_stage.io().reg_write;
}
```

#### B2: 控制单元

```cpp
class ControlUnit : public ch::Component {
public:
    __io(
        ch_in<ch_uint<7>> opcode;
        ch_in<ch_uint<3>> funct3;
        
        ch_out<ch_bool> reg_write;
        ch_out<ch_bool> alu_src;
        ch_out<ch_bool> mem_read;
        ch_out<ch_bool> mem_write;
        ch_out<ch_bool> branch;
        ch_out<ch_bool> jump;
    )
    
    void describe() override {
        // R-type: opcode=0110011
        auto is_rtype = (io().opcode == 0b0110011_d);
        // I-type: opcode=0010011
        auto is_itype = (io().opcode == 0b0010011_d);
        // Load: opcode=0000011
        auto is_load = (io().opcode == 0b0000011_d);
        // Store: opcode=0100011
        auto is_store = (io().opcode == 0b0100011_d);
        // Branch: opcode=1100011
        auto is_branch = (io().opcode == 0b1100011_d);
        // Jump: opcode=1101111 (JAL)
        auto is_jump = (io().opcode == 0b1101111_d);
        
        io().reg_write = is_rtype || is_itype || is_load || is_jump;
        io().alu_src = is_itype || is_load;
        io().mem_read = is_load;
        io().mem_write = is_store;
        io().branch = is_branch;
        io().jump = is_jump;
    }
};
```

### 4.3 Gate 标准

> **修订 v1.1**: Momus#3.1 指出 "20 条指令序列" 模糊不清。Momus#3.2 指出测试文件不存在。

- [ ] `test_pipeline_execution.cpp` (新建): 执行混合程序 (addi, add, sub, lw, sw, beq, jalr)
- [ ] 测试程序: 20 条指令执行后验证 `x3 == 3`, `x4 == 10`, 内存地址 0x20010000 处值为 `0xDEADBEEF`
- [ ] 寄存器读写 100% 正确
- [ ] 内存读写数据一致
- [ ] 编译 0 warnings
- [ ] CI 通过

---

## 5. Phase C: CLINT + 中断

**工期**: 3-4 工作日  
**前置条件**: Phase A, B 完成  
**输出**: MTIP 中断触发→trap 入口→MRET 返回

### 5.1 任务分解

| ID | 任务 | 文件 | 工时 | 依赖 |
|----|------|------|------|------|
| C1 | CLINT 模块 | `include/chlib/clint.h` | 1d | 无 |
| C2 | 异常单元 | `src/rv32i_exception.h` | 1d | Phase A |
| C3 | Pipeline IRQ pin | `src/rv32i_pipeline.h` | 0.5d | 无 |
| C4 | 中断流测试 | `tests/test_exception_flow.cpp` | 1d | C1-C3 |

### 5.2 Gate 标准

- [ ] mtip 触发后 4 cycles 内执行 trap handler 第一条指令
- [ ] mepc 保存正确
- [ ] MRET 后返回原 PC
- [ ] CI 通过

---

## 6. Phase D: SoC + Bare-metal "Hello World"

> **修订 v1.1**: 从 "SoC + ELF Loader" 改为 "SoC + Bare-metal Hello World"。Metis 指出 CPU 正确性 Gate 应该是 bare-metal 而非 FreeRTOS。ELF Loader 延后到 Phase F。Momus#5.2 ELF loader over-engineered.

**工期**: 3-4 工作日  
**前置条件**: Phase C 完成  
**输出**: 可加载 HEX 固件的完整 SoC，UART 打印 "Hello from riscv-mini"

### 6.1 任务分解

| ID | 任务 | 文件 | 工时 | 依赖 |
|----|------|------|------|------|
| D1 | Address Decoder | `src/address_decoder.h` | 1d | 无 |
| D2 | UART 模型 | `include/axi4/peripherals/axi_uart.h` | 0.5d | 无 |
| D3 | HEX/Binary Loader | `include/chlib/firmware_loader.h` | 1d | 无 |
| D4 | 集成测试 (新建) | `tests/test_soc_integration.cpp` | 1d | D1-D3 |

### 6.2 Gate 标准

> **修订 v1.1**: Momus#3.1 "UART 打印 Hello" 模糊 → 明确输出期望。

- [ ] HEX 固件加载到 0x20000000
- [ ] UART TX 输出 "Hello from riscv-mini\r\n"
- [ ] PC 从 0x20000000 (复位向量) 开始执行
- [ ] CLINT mtime 递增正常
- [ ] 编译 0 warnings

### 6.3 Phase D.5: riscv-tests ISA 套件 (可选)

**工期**: 1-2 工作日  
**输出**: RV32I ISA 标准测试套件验证

- [ ] 加载 riscv-tests RV32I 编译的 HEX
- [ ] tohost 检查 PASS
- [ ] 覆盖率 ≥90% RV32I 指令
- [ ] riscv-tests 全部通过 (add, sub, lw, sw, branch, jal, auipc, lui)

> **Metis 建议**: 这是 CPU 正确性最强证明。推荐作为 Phase D 的可选 Gate。

---

## 7. Phase E: FreeRTOS Port (可选)

> **修订 v1.1**: 从必选改为**可选**。Metis 指出 FreeRTOS 是演示不是 Gate。工期从 3-4 天调整为 7-14 天 (Momus#2.3)。

**工期**: 7-14 工作日 (原 3-4)  
**前置条件**: Phase D 完成 (含 bare-metal Hello World Gate)  
**输出**: FreeRTOS 运行，2 任务切换 (演示)

### 7.1 任务分解

| ID | 任务 | 文件 | 工时 | 依赖 |
|----|------|------|------|------|
| E1 | FreeRTOSConfig.h | `freertos/config/` | 0.5d | 无 |
| E2 | port.c (上下文切换) | `freertos/port/port.c` | 2d | Phase C (CLINT) |
| E3 | portASM.S (trap handler) | `freertos/port/portASM.S` | 1.5d | E2 |
| E4 | 双任务 Demo | `freertos/demo/demo_main.c` | 1d | E3 |
| E5 | Boot 测试 (新建) | `tests/test_freertos_boot.cpp` | 1d | D4, E4 |
| E6 | 性能调优 | — | 2-8d | E5 |

### 7.2 Gate 标准

- [ ] FreeRTOS 编译通过 (riscv32-unknown-elf-gcc)
- [ ] 双任务运行，UART 输出交替打印
- [ ] Tick 频率 1kHz (±1%, 或降级到 100Hz 如 Momus#6.3 建议)
- [ ] 连续运行 10 秒无崩溃

> **注**: Phase E 是可选演示。CPU 正确性验证在 Phase D.

---

## 8. 风险管理

> **修订 v1.1**: Momus#4.1-4.3 指出缺少 pipeline hazards、timer/clock domain、toolchain 兼容风险。

| 风险 | 影响 | 概率 | 缓解 |
|------|------|------|------|
| Pipeline wiring 复杂 | Phase B 延期 | **高** | 分 stage 单元测试，每 stage 独立验证 |
| Load-use hazard 死锁 | Pipeline stall 无法恢复 | **高** | 显式 stall/flush 信号路径测试 |
| CSR/异常耦合 | Phase A/C 阻塞 | 中 | A 只做读写，C 做异常流 |
| FreeRTOS 兼容性 | Phase E 失败 | 中 | 参考 VexRiscv port |
| Timer clock domain | CLINT 频率不匹配 | **高** (新增) | mtime 递增与 CPU clock 同步校验 |
| Toolchain 兼容性 | GCC 内置函数不可用 | 中 (新增) | 先用 Clang RISC-V 目标测试 |
| 无 Debug 基础设施 | Phase E crash 无法调试 | **高** (新增) | Phase 0.5: VCD 波形 dump + UART console |
| 1kHz tick 未验证 | FreeRTOS timing 失败 | 中 (新增) | 先测 100Hz，确认无误后提升 |

### Phase 0.5: 验证基础设施 (建议)

> Momus#5.3 建议添加验证基础设施作为前置步骤。

**工期**: 0.5-1 工作日 (强烈建议)

- [ ] VCD 波形 dump 框架 (`ch::Simulator` trace 配置)
- [ ] UART console 捕获工具
- [ ] 寄存器状态快照 API

---

## 9. 里程碑

> **修订 v1.1**: 调整时间线反映新的估算。

里程碑|预计完成|验收标准
|---|---|---|
Phase 0.5: 验证基础设施 | Day 1 | VCD dump + UART console |
Phase A 完成 | Day 5 | CSR 读写 (5 tests), MRET 流程通过 |
Phase B 完成 | Day 12 | Pipeline 执行 20 条混合指令正确 |
Phase C 完成 | Day 16 | MTIP 中断触发→trap 入口→MRET 返回 |
Phase D: bare-metal Hello World | Day 20 | UART 打印 "Hello" + riscv-tests 通过 |
Phase E (可选): FreeRTOS | Day 23-34 | 双任务切换，10 秒无崩溃 |

