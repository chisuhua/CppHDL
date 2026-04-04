/**
 * @file rv32i_core.h
 * @brief RISC-V RV32I 基础指令集处理器核心（简化版）
 * 
 * 注意：这是 CppHDL 框架下的简化实现，用于演示 RV32I 核心架构
 * 完整实现需要处理所有 40 条指令的时序和控制逻辑
 */

#pragma once

#include "ch.hpp"
#include "component.h"

using namespace ch::core;

namespace riscv {

// ============================================================================
// RV32I 常量定义
// ============================================================================

constexpr int RV32I_REG_COUNT = 32;
constexpr int RV32I_REG_WIDTH = 32;
constexpr int RV32I_ADDR_WIDTH = 32;
constexpr int RV32I_DATA_WIDTH = 32;

// ============================================================================
// RV32I 指令操作码 (opcode) - 使用标准 uint8_t
// ============================================================================

enum class RvOpcode : uint8_t {
    LUI     = 0b0110111,
    AUIPC   = 0b0010111,
    JAL     = 0b1101111,
    JALR    = 0b1100111,
    BRANCH  = 0b1100011,
    LOAD    = 0b0000011,
    STORE   = 0b0100011,
    OPIMM   = 0b0010011,
    OP      = 0b0110011,
    SYSTEM  = 0b1110011
};

// ============================================================================
// RV32I funct3 字段
// ============================================================================

enum class RvFunct3 : uint8_t {
    // BRANCH
    BEQ  = 0b000, BNE  = 0b001, BLT  = 0b100, BGE  = 0b101, BLTU = 0b110, BGEU = 0b111,
    // LOAD/STORE
    LB   = 0b000, LH   = 0b001, LW   = 0b010, LBU  = 0b100, LHU  = 0b101,
    SB   = 0b000, SH   = 0b001, SW   = 0b010,
    // OPIMM/OP
    ADD  = 0b000, SLL  = 0b001, SLT  = 0b010, SLTU = 0b011,
    XOR  = 0b100, SRL  = 0b101, OR   = 0b110, AND  = 0b111
};

// ============================================================================
// RV32I 寄存器文件
// ============================================================================

class RvRegFile : public ch::Component {
public:
    __io(
        ch_in<ch_uint<5>>  read_addr_a;
        ch_out<ch_uint<32>> read_data_a;
        ch_in<ch_uint<5>>  read_addr_b;
        ch_out<ch_uint<32>> read_data_b;
        ch_in<ch_uint<5>>  write_addr;
        ch_in<ch_uint<32>> write_data;
        ch_in<ch_bool>     write_enable;
        ch_in<ch_bool>     clk;
        ch_in<ch_bool>     rst;
    )
    
    RvRegFile(ch::Component* parent = nullptr, const std::string& name = "rv_regfile")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 使用 ch_reg 数组实现寄存器文件
        ch_reg<ch_uint<32>> regs[RV32I_REG_COUNT];
        for (int i = 0; i < RV32I_REG_COUNT; i++) {
            regs[i] = ch_reg<ch_uint<32>>(0, "reg" + std::to_string(i));
        }
        
        // 读端口 A (组合逻辑)
        CH_IF(read_addr_a() == 0) {
            read_data_a() = 0;  // x0 硬连线为 0
        } CH_ELSE {
            read_data_a() = regs[read_addr_a()];
        }
        
        // 读端口 B (组合逻辑)
        CH_IF(read_addr_b() == 0) {
            read_data_b() = 0;  // x0 硬连线为 0
        } CH_ELSE {
            read_data_b() = regs[read_addr_b()];
        }
        
        // 写端口 (时序逻辑)
        CH_PROCESS(clk, rst) {
            CH_IF(rst()) {
                for (int i = 0; i < RV32I_REG_COUNT; i++) {
                    regs[i] <<= 0;
                }
            } CH_ELSE {
                CH_IF(write_enable() && write_addr() != 0) {
                    regs[write_addr()] <<= write_data();
                }
            }
        }
    }
};

// ============================================================================
// RV32I ALU
// ============================================================================

enum class AluOp : uint8_t {
    ADD, SUB, SLL, SLT, SLTU, XOR, SRL, SRA, OR, AND, PASS_A, COPY
};

class RvAlu : public ch::Component {
public:
    __io(
        ch_in<ch_uint<32>>  operand_a;
        ch_in<ch_uint<32>>  operand_b;
        ch_in<ch_uint<8>>   operation;  // AluOp
        ch_out<ch_uint<32>> result;
        ch_out<ch_bool>     zero_flag;
    )
    
    RvAlu(ch::Component* parent = nullptr, const std::string& name = "rv_alu")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // ALU 操作（组合逻辑）
        CH_SWITCH(operation()) {
            CH_CASE(0) { result() = operand_a() + operand_b(); }  // ADD
            CH_CASE(1) { result() = operand_a() - operand_b(); }  // SUB
            CH_CASE(2) { result() = operand_a() << operand_b().slice<4, 0>(); }  // SLL
            CH_CASE(3) { result() = ch_cast<ch_uint<32>>(ch_cast<ch_int<32>>(operand_a()) < ch_cast<ch_int<32>>(operand_b())); }  // SLT
            CH_CASE(4) { result() = ch_cast<ch_uint<32>>(operand_a() < operand_b()); }  // SLTU
            CH_CASE(5) { result() = operand_a() ^ operand_b(); }  // XOR
            CH_CASE(6) { result() = operand_a() >> operand_b().slice<4, 0>(); }  // SRL
            CH_CASE(7) { result() = ch_cast<ch_uint<32>>(ch_cast<ch_int<32>>(operand_a()) >> operand_b().slice<4, 0>()); }  // SRA
            CH_CASE(8) { result() = operand_a() | operand_b(); }  // OR
            CH_CASE(9) { result() = operand_a() & operand_b(); }  // AND
            CH_CASE(10) { result() = operand_a(); }  // PASS_A
            CH_CASE(11) { result() = operand_b(); }  // COPY
            CH_DEFAULT { result() = 0; }
        }
        
        zero_flag() = (result() == 0);
    }
};

// ============================================================================
// RV32I 处理器核心 (简化版 - 仅框架)
// ============================================================================

class Rv32iCore : public ch::Component {
public:
    __io(
        // AXI4 指令接口
        ch_out<ch_uint<32>> ifetch_addr;
        ch_in<ch_uint<32>>  ifetch_data;
        ch_out<ch_bool>     ifetch_valid;
        ch_in<ch_bool>      ifetch_ready;
        
        // AXI4 数据接口
        ch_out<ch_uint<32>> dmem_addr;
        ch_out<ch_uint<32>> dmem_wdata;
        ch_in<ch_uint<32>>  dmem_rdata;
        ch_out<ch_bool>     dmem_read;
        ch_out<ch_bool>     dmem_write;
        ch_out<ch_uint<4>>  dmem_strbe;
        ch_in<ch_bool>      dmem_ready;
        
        // 控制
        ch_in<ch_bool>      clk;
        ch_in<ch_bool>      rst;
        ch_out<ch_bool>     running;
    )
    
    Rv32iCore(ch::Component* parent = nullptr, const std::string& name = "rv32i_core")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 程序计数器
        ch_reg<ch_uint<32>> pc{"pc", 0};
        ch_reg<ch_uint<32>> next_pc{"next_pc", 4};
        
        // 寄存器文件
        ch::ch_module<RvRegFile> regfile{"regfile"};
        regfile.io().clk = clk();
        regfile.io().rst = rst();
        
        // ALU
        ch::ch_module<RvAlu> alu{"alu"};
        
        // 当前指令锁存
        ch_reg<ch_uint<32>> instruction{"instruction", 0};
        
        // 提取指令字段
        ch_var<ch_uint<7>> opcode{"opcode"};
        ch_var<ch_uint<5>> rd{"rd"};
        ch_var<ch_uint<3>> funct3{"funct3"};
        ch_var<ch_uint<5>> rs1{"rs1"};
        ch_var<ch_uint<5>> rs2{"rs2"};
        ch_var<ch_uint<7>> funct7{"funct7"};
        
        opcode = instruction().slice<6, 0>();
        rd = instruction().slice<11, 7>();
        funct3 = instruction().slice<14, 12>();
        rs1 = instruction().slice<19, 15>();
        rs2 = instruction().slice<24, 20>();
        funct7 = instruction().slice<31, 25>();
        
        // 立即数 (符号扩展)
        ch_var<ch_uint<32>> imm{"imm"};
        
        // ALU 操作数
        ch_var<ch_uint<32>> alu_a{"alu_a"};
        ch_var<ch_uint<32>> alu_b{"alu_b"};
        ch_var<ch_uint<8>> alu_op{"alu_op"};
        
        alu_a = regfile.io().read_data_a;
        
        // 简化：默认使用寄存器操作数 B
        alu_b = regfile.io().read_data_b;
        alu_op = 0;  // ADD
        
        // ALU 连接
        alu.io().operand_a = alu_a;
        alu.io().operand_b = alu_b;
        alu.io().operation = alu_op;
        
        // 写回数据
        ch_var<ch_uint<32>> write_data{"write_data"};
        write_data = alu.io().result;
        
        // 寄存器文件连接
        regfile.io().read_addr_a = rs1;
        regfile.io().read_addr_b = rs2;
        regfile.io().write_addr = rd;
        regfile.io().write_data = write_data;
        
        // 主状态机 - 简化的取指 - 执行循环
        CH_PROCESS(clk, rst) {
            CH_IF(rst()) {
                pc <<= 0;
                instruction <<= 0;
                running() = false;
                ifetch_valid() = false;
                dmem_read() = false;
                dmem_write() = false;
            } CH_ELSE {
                running() = true;
                
                // 取指阶段
                ifetch_addr() = pc;
                ifetch_valid() = true;
                
                CH_IF(ifetch_ready()) {
                    instruction <<= ifetch_data();
                    pc <<= pc + 4;
                    
                    // 译码和执行阶段（简化）
                    CH_SWITCH(opcode) {
                        CH_CASE(static_cast<uint32_t>(RvOpcode::OPIMM)) {
                            // I-type 立即数操作
                            alu_op = 0;  // ADD
                            regfile.io().write_enable = true;
                        }
                        CH_CASE(static_cast<uint32_t>(RvOpcode::OP)) {
                            // R-type 寄存器操作
                            alu_op = 0;  // ADD
                            regfile.io().write_enable = true;
                        }
                        CH_CASE(static_cast<uint32_t>(RvOpcode::LOAD)) {
                            // 加载指令
                            dmem_read() = true;
                            alu_op = 0;  // ADD for address calculation
                        }
                        CH_CASE(static_cast<uint32_t>(RvOpcode::STORE)) {
                            // 存储指令
                            dmem_write() = true;
                            alu_op = 0;  // ADD for address calculation
                        }
                        CH_DEFAULT {
                            regfile.io().write_enable = false;
                        }
                    }
                } CH_ELSE {
                    ifetch_valid() = false;
                    dmem_read() = false;
                    dmem_write() = false;
                    regfile.io().write_enable = false;
                }
            }
        }
        
        // 数据内存接口
        dmem_addr() = alu.io().result;
        dmem_wdata() = regfile.io().read_data_b;
        dmem_strbe() = 0b1111;  // 默认全字节使能
    }
};

} // namespace riscv
