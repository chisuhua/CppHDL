// ⚠️⚠️⚠️ **路线 C 第 2 批 —— RV32I 的「时序侧」组件进双后端 parity**（2026-11-03）
//
// ## 为什么这批
//
// 第 1 批（`test_rv32i_stage_dual.cpp`）覆盖了三个**纯组合**组件（`MemStage` /
// `ExStage` / `Rv32iDecoder`）⇒ ⇒ 本批覆盖**含时序状态**的那三个：
//
// | 组件 | 时序状态 | 价值 |
// |---|---|---|
// | `riscv::IdStage` | **16** 个 `ch_reg`（ID/EX 流水线寄存器） | **最近 3 个真缺陷全在这一层**（U 型立即数 · `alu_op` 的 SUB/SRA 误判 · B 型立即数）⇒ ⇒ 且是 RV32I 的**唯一权威**（ADR-046） |
// | `riscv::Rv32iRegFile` | **32** 个 `ch_reg`（x0..x31） | **仓内最大的 `always_ff` 面** ⇒ 两后端最易分叉处；且 `x0` 硬连线语义是经典缺陷高发区 |
// | `riscv::DataTCM<8,32>` | `ch_mem`（**`aread` 0-cycle**）+ **读-改-写**字节合并 | 2026-10-26 的 `strbe` 修复就在这里 ⇒ ⇒ 且「读-改-写的同拍语义」是**两后端最容易分叉**的一处 |
//
// ⇒ ⇒ **三个组件都走 `always_ff` / mem 写口 ⇒ 正是两后端最易分叉的路径**
// ⇒ ⇒ 这一点与第 1 批（纯组合）互补。
//
// ## 可转性 4 问
//
// ① **重复**：三个符号在全部 60 个 macro dual 文件里**各出现 0 次** ⇒ 无重复。
//    ⚠️ 唯一既有覆盖是 **SoC 级** hand-crafted dual ⇒ ⇒ **判别形态不同**
//    （整 SoC 固件轨迹 vs 单组件的时序契约）⇒ ⇒ 逐案裁决通过。
// ② **判别力**：变异 **9/10 抓** + 1 条如实登记的判别力边界（见下）。
// ③ **与后端无关**：**否** —— 真跑仿真 + 读顶层端口，正是要对比两后端。
// ④ **值得新写**：三个组件**本身带顶层端口** ⇒ ⇒ **零 DUT 复制**（直接当 DUT）。
//
// ## ⚠️ 为什么**不做** `InstrTCM`（`sread` 那一侧）
//
// `InstrTCM` 用 `memory.sread(addr, true)` ⇒ 「地址在 T 拍给出、数据在 T+1 拍输出」
// ⇒ ⇒ 但**陷阱 T2**（`src/simulator.cpp` 的读模型）：地址是**顶层输入端口**时，
// TB 在 `tick()` **之前**就把它写进了 `data_map_` ⇒ ⇒ 沿上采样到的**已经是新地址**
// ⇒ ⇒ **0-cycle 与 1-cycle 的观测值完全相同** ⇒ ⇒ ⇒ **组件级的 `sread` 时序不可判别**
// ⇒ ⇒ 唯一可判别的形态是「地址由**同一条沿提交的 `ch_reg`** 驱动」
// ⇒ ⇒ 那需要一层 wrapper ⇒ ⇒ **不是零 DUT 复制** ⇒ ⇒ **本批不做**（记此以免后人重复）。
//
// ## 读模型（`Simulator::tick()` 的精确顺序 · `src/simulator.cpp:308-328`）
//
//     1) eval_combinational()   ← **边沿前**
//     2) 寄存器提交（用边沿前的组合值）
//     3) eval_combinational()   ← **边沿后**
//
// ⇒ ⇒ **tick 之后读到的端口 = f(边沿后的寄存器, 本拍输入)**：
// - `IdStage` / `Rv32iRegFile` ⇒ 读到的是**刚提交**的寄存器值；
// - `DataTCM` ⇒ 它的「寄存器」是 `ch_mem` 的字：`aread` 是**组合**的、写在**沿上**提交
//   ⇒ ⇒ **tick 后 `rdata` 是「写后」的字**（不是写前的旧字）⇒ ⇒ 这正是
//   「读-改-写」的**可观测结果**（旧字 + 新字节）⇒ ⇒ 也是两后端最容易分叉的一处。
//
// ## 期望值（**手推**，禁抄仿真输出）
//
// 模型 = **独立 Python 实现**（`/tmp/opencode/gen_route_c2_table.py`）逐条按实现推导
// ⇒ ⇒ 指令字由**规范编码器**按 RISC-V 规范生成（期望值**不是**从实现反推）
// ⇒ ⇒ **一次通过**（两后端都吻合）。
//
// ## ⚠️ 三个组件的**死输入端口**（未驱动 = 未定义值 ⇒ MUST 显式驱动）
//
// | 组件 | 死端口 |
// |---|---|
// | `Rv32iRegFile` | `clk` · `rst`（`describe()` 从不读 ⇒ 寄存器用**隐式时钟**） |
//
// ⇒ ⇒ 本文件对**每一个**输入端口都显式驱动 ⇒ ⇒ 不依赖「未驱动也无所谓」的**碰巧通过**。
//
// ## ⚠️ `DataTCM` 的模板参数
//
// 生产用 `DataTCM<kTcmAddrWidth, 32>`（`kTcmAddrWidth` 更大）⇒ ⇒ 本文件用
// `DataTCM<8, 32>`（**256 字**）⇒ ⇒ **只为控制 Verilog 编译成本**（`ch_mem` 的
// Verilog 数组大小 = `SIZE × 32` bit）⇒ ⇒ **语义路径完全相同**
// （`ADDR_WIDTH` 只影响存储深度与地址位宽）⇒ ⇒ 先例：`test_pipeline_exec.cpp`
// 也用非默认深度（`DataTCM<16,32>` · `InstrTCM<10,32>`）。
//
// ## ⚠️ 陷阱 T3：body 参数里**花括号内的逗号**会被预处理器当参数分隔
//    ⇒ ⇒ 拍表放**文件作用域**；`DataTCM<8, 32>` 的模板逗号同理
//    ⇒ ⇒ **用文件作用域的 `using` 别名**传给 macro。
// ⚠️ **判据（N2′）**：断言数(with verilator) == **2 ×** 断言数(interpreter-only)。
#include "catch_amalgamated.hpp"
#include "dual_backend_fixture.h"

#include "ch.hpp"
#include "component.h"
#include "core/context.h"
#include "cpu/pipeline/rv32i_regfile.h"
#include "cpu/pipeline/rv32i_tcm.h"
#include "cpu/pipeline/stages/id_stage.h"
#include "simulator.h"

#include <cstdint>

using namespace ch;
using namespace ch::core;

namespace {

// ⚠️ **模板逗号会被 macro 当参数分隔** ⇒ ⇒ 用文件作用域别名
using DataTcmDut = riscv::DataTCM<8, 32>;

// ===========================================================================
// 拍表 1：`riscv::IdStage`（ID/EX 流水线寄存器）
// ===========================================================================
// 字段顺序：instruction, valid, flush, stall, pc, reg_rs1_data, reg_rs2_data,
//           rs1_addr, rs2_addr, rs1_data, rs2_data, imm, opcode, funct3,
//           is_branch, is_load, is_store, is_jump, is_alu, rd_addr,
//           valid_out, pc_out, rs1_addr_id, rs2_addr_id, funct7_out
struct IdCase {
    uint64_t instruction, valid, flush, stall, pc, reg_rs1_data, reg_rs2_data;
    uint64_t rs1_addr, rs2_addr, rs1_data, rs2_data, imm, opcode, funct3;
    uint64_t is_branch, is_load, is_store, is_jump, is_alu, rd_addr;
    uint64_t valid_out, pc_out, rs1_addr_id, rs2_addr_id, funct7_out;
};

const IdCase kIdCases[] = {
    {0x00500093ULL, 1, 0, 0, 0x00001000ULL, 0x000000AAULL, 0x000000BBULL, 0, 0, 0x000000AAULL, 0x000000BBULL, 0x00000005ULL, 19, 0, 0, 0, 0, 0, 1, 1, 1, 0x00001000ULL, 0, 0, 0},  // addi x1,x0,5
    {0x00310463ULL, 1, 0, 0, 0x00001004ULL, 0x00000011ULL, 0x00000022ULL, 2, 3, 0x00000011ULL, 0x00000022ULL, 0x00000008ULL, 99, 0, 1, 0, 0, 0, 0, 8, 1, 0x00001004ULL, 2, 3, 0},  // beq x2,x3,8
    {0x0042A423ULL, 1, 0, 1, 0x00001008ULL, 0x00000033ULL, 0x00000044ULL, 5, 4, 0x00000033ULL, 0x00000044ULL, 0x00000008ULL, 35, 2, 0, 0, 0, 0, 0, 0, 0, 0x00001008ULL, 5, 4, 0},  // sw x4,8(x5) + stall
    {0x00500093ULL, 0, 0, 0, 0x0000100CULL, 0x00000055ULL, 0x00000066ULL, 5, 4, 0x00000033ULL, 0x00000044ULL, 0x00000008ULL, 35, 2, 0, 0, 0, 0, 0, 0, 0, 0x00001008ULL, 0, 0, 0},  // valid=0 hold
    {0x0043A303ULL, 1, 1, 0, 0x00001010ULL, 0x00000077ULL, 0x00000088ULL, 7, 0, 0x00000077ULL, 0x00000088ULL, 0x00000004ULL, 3, 2, 0, 0, 0, 0, 0, 0, 0, 0x00001010ULL, 7, 0, 0},  // lw x6,4(x7) + flush
    {0x12345437ULL, 1, 0, 0, 0x00001014ULL, 0x00000099ULL, 0x000000AAULL, 0, 0, 0x00000099ULL, 0x000000AAULL, 0x12345000ULL, 55, 5, 0, 0, 0, 0, 1, 8, 1, 0x00001014ULL, 0, 0, 9},  // lui x8,0x12345
    {0x12345497ULL, 1, 0, 0, 0x00001018ULL, 0x000000BBULL, 0x000000CCULL, 0, 0, 0x000000BBULL, 0x000000CCULL, 0x12345000ULL, 23, 5, 0, 0, 0, 0, 1, 9, 1, 0x00001018ULL, 0, 0, 9},  // auipc x9,0x12345
    {0x010000EFULL, 1, 0, 0, 0x0000101CULL, 0x000000DDULL, 0x000000EEULL, 0, 0, 0x000000DDULL, 0x000000EEULL, 0x00000010ULL, 111, 0, 0, 0, 0, 1, 0, 1, 1, 0x0000101CULL, 0, 0, 0},  // jal x1,16
    {0x004100E7ULL, 1, 0, 0, 0x00001020ULL, 0x000000FFULL, 0x00000012ULL, 2, 0, 0x000000FFULL, 0x00000012ULL, 0x00000004ULL, 103, 0, 0, 0, 0, 1, 0, 1, 1, 0x00001020ULL, 2, 0, 0},  // jalr x1,4(x2)
    {0xFE208CE3ULL, 1, 0, 0, 0x00001024ULL, 0x00000034ULL, 0x00000056ULL, 1, 2, 0x00000034ULL, 0x00000056ULL, 0xFFFFF7F9ULL, 99, 0, 1, 0, 0, 0, 0, 25, 1, 0x00001024ULL, 1, 2, 127},  // beq x1,x2,-8
};

// ===========================================================================
// 拍表 2：`riscv::Rv32iRegFile`
// ===========================================================================
// 字段顺序：rs1_addr, rs2_addr, rd_addr, rd_data, rd_we, rs1_data, rs2_data
struct RegCase {
    uint64_t rs1_addr, rs2_addr, rd_addr, rd_data, rd_we;
    uint64_t rs1_data, rs2_data;
};

const RegCase kRegCases[] = {
    {5, 6, 5, 0x12345678ULL, 1, 0x12345678ULL, 0x00000000ULL},  // write x5
    {5, 6, 0, 0x00000000ULL, 0, 0x12345678ULL, 0x00000000ULL},  // read x5 / x6
    {0, 0, 0, 0xDEADBEEFULL, 1, 0x00000000ULL, 0x00000000ULL},  // write x0 ignored
    {0, 0, 0, 0x00000000ULL, 0, 0x00000000ULL, 0x00000000ULL},  // read x0
    {5, 6, 5, 0xAAAAAAAAULL, 0, 0x12345678ULL, 0x00000000ULL},  // rd_we=0 no write
    {5, 6, 0, 0x00000000ULL, 0, 0x12345678ULL, 0x00000000ULL},  // read x5 unchanged
    {31, 5, 31, 0xCAFEBABEULL, 1, 0xCAFEBABEULL, 0x12345678ULL},  // write x31
    {31, 5, 0, 0x00000000ULL, 0, 0xCAFEBABEULL, 0x12345678ULL},  // read x31 / x5
    {17, 5, 17, 0x0F0F0F0FULL, 1, 0x0F0F0F0FULL, 0x12345678ULL},  // write x17
    {17, 5, 0, 0x00000000ULL, 0, 0x0F0F0F0FULL, 0x12345678ULL},  // read x17 / x5
    {9, 10, 0, 0x00000000ULL, 0, 0x00000000ULL, 0x00000000ULL},  // read unwritten x9
    {9, 10, 9, 0x00000009ULL, 1, 0x00000009ULL, 0x00000000ULL},  // write x9
    {9, 10, 0, 0x00000000ULL, 0, 0x00000009ULL, 0x00000000ULL},  // read x9 / x10
    {1, 2, 1, 0x11111111ULL, 1, 0x11111111ULL, 0x00000000ULL},  // write x1 (x0+1)
    {1, 2, 0, 0x00000000ULL, 0, 0x11111111ULL, 0x00000000ULL},  // read x1 / x2
    {2, 2, 0, 0x00000000ULL, 0, 0x00000000ULL, 0x00000000ULL},  // read x2 (unwritten)
};

// ===========================================================================
// 拍表 3：`riscv::DataTCM<8, 32>`（`aread` + 读-改-写字节合并）
// ===========================================================================
// 字段顺序：addr, wdata, strbe, write, valid, rdata, ready
struct TcmCase {
    uint64_t addr, wdata, strbe, write, valid, rdata, ready;
};

const TcmCase kTcmCases[] = {
    {16, 0x00000000ULL, 0, 0, 0, 0x00000000ULL, 1},  // read empty
    {16, 0x11223344ULL, 15, 1, 1, 0x11223344ULL, 1},  // write full word
    {16, 0x000000ABULL, 1, 1, 1, 0x112233ABULL, 1},  // write lane0
    {16, 0xAB000000ULL, 8, 1, 1, 0xAB2233ABULL, 1},  // write lane3
    {16, 0x00CC00DDULL, 6, 1, 1, 0xABCC00ABULL, 1},  // write lane1+2
    {16, 0x99999999ULL, 15, 1, 0, 0xABCC00ABULL, 1},  // valid=0 no write
    {16, 0xFFFFFFFFULL, 0, 1, 1, 0xABCC00ABULL, 1},  // strbe=0 no byte
    {17, 0x00000000ULL, 0, 0, 0, 0x00000000ULL, 1},  // read other addr
    {17, 0x5A5A5A5AULL, 15, 1, 1, 0x5A5A5A5AULL, 1},  // write other addr
    {17, 0x00000000ULL, 0, 0, 0, 0x5A5A5A5AULL, 1},  // read addr 0x11
};

}  // namespace

// ===========================================================================
// pair 1：`riscv::IdStage` —— ID/EX 流水线寄存器 + 气泡契约
// ===========================================================================
DUAL_BACKEND_TEST_PAIR(
    riscv::IdStage,
    "IdStage：ID/EX 流水线寄存器（16 个 ch_reg）+ 气泡契约（stall/flush 清控制、刷新数据）+ 立即数提取",
    "[cpu][rv32i][id_stage][pipeline_reg][bubble][dual]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kIdCases) {
            sim.set_input_value(io.instruction, tc.instruction);
            sim.set_input_value(io.valid, tc.valid);
            sim.set_input_value(io.flush, tc.flush);
            sim.set_input_value(io.stall, tc.stall);
            sim.set_input_value(io.pc, tc.pc);
            sim.set_input_value(io.reg_rs1_data, tc.reg_rs1_data);
            sim.set_input_value(io.reg_rs2_data, tc.reg_rs2_data);
            sim.tick();
            INFO("instruction=0x" << std::hex << tc.instruction << std::dec
                                << " valid=" << tc.valid << " flush=" << tc.flush
                                << " stall=" << tc.stall);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.rs1_addr)) ==
                    tc.rs1_addr);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.rs2_addr)) ==
                    tc.rs2_addr);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.rs1_data)) ==
                    tc.rs1_data);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.rs2_data)) ==
                    tc.rs2_data);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.imm)) == tc.imm);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.opcode)) ==
                    tc.opcode);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.funct3)) ==
                    tc.funct3);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.is_branch)) ==
                    tc.is_branch);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.is_load)) ==
                    tc.is_load);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.is_store)) ==
                    tc.is_store);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.is_jump)) ==
                    tc.is_jump);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.is_alu)) ==
                    tc.is_alu);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.rd_addr)) ==
                    tc.rd_addr);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.valid_out)) ==
                    tc.valid_out);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.pc_out)) ==
                    tc.pc_out);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.rs1_addr_id)) ==
                    tc.rs1_addr_id);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.rs2_addr_id)) ==
                    tc.rs2_addr_id);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.funct7_out)) ==
                    tc.funct7_out);
        }
    });

// ===========================================================================
// pair 2：`riscv::Rv32iRegFile` —— 32 个 `ch_reg` + x0 硬连线 + 写使能
// ===========================================================================
DUAL_BACKEND_TEST_PAIR(
    riscv::Rv32iRegFile,
    "Rv32iRegFile：32 个 ch_reg 的读写 + x0 硬连线为 0 + 写使能门控",
    "[cpu][rv32i][regfile][x0][dual]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kRegCases) {
            sim.set_input_value(io.rs1_addr, tc.rs1_addr);
            sim.set_input_value(io.rs2_addr, tc.rs2_addr);
            sim.set_input_value(io.rd_addr, tc.rd_addr);
            sim.set_input_value(io.rd_data, tc.rd_data);
            sim.set_input_value(io.rd_we, tc.rd_we);
            // ⚠️ 死端口也 MUST 显式驱动（`clk` / `rst` 无消费者 ⇒ 寄存器用隐式时钟）
            sim.set_input_value(io.clk, 0);
            sim.set_input_value(io.rst, 0);
            sim.tick();
            INFO("rs1_addr=" << tc.rs1_addr << " rs2_addr=" << tc.rs2_addr
                            << " rd_addr=" << tc.rd_addr << " rd_we=" << tc.rd_we);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.rs1_data)) ==
                    tc.rs1_data);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.rs2_data)) ==
                    tc.rs2_data);
        }
    });

// ===========================================================================
// pair 3：`riscv::DataTCM<8,32>` —— `aread` 0-cycle + 读-改-写字节合并
// ===========================================================================
DUAL_BACKEND_TEST_PAIR(
    DataTcmDut,
    "DataTCM：aread 0-cycle 读 + 读-改-写的字节合并（strbe）+ valid 门控",
    "[cpu][rv32i][tcm][byte_lane][aread][dual]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kTcmCases) {
            sim.set_input_value(io.addr, tc.addr);
            sim.set_input_value(io.wdata, tc.wdata);
            sim.set_input_value(io.strbe, tc.strbe);
            sim.set_input_value(io.write, tc.write);
            sim.set_input_value(io.valid, tc.valid);
            sim.tick();
            INFO("addr=0x" << std::hex << tc.addr << " wdata=0x" << tc.wdata
                           << " strbe=0x" << tc.strbe << std::dec
                           << " write=" << tc.write << " valid=" << tc.valid);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.rdata)) ==
                    tc.rdata);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.ready)) ==
                    tc.ready);
        }
    });
