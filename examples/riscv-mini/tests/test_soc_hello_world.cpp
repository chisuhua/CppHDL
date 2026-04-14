/**
 * @file test_soc_hello_world.cpp
 * @brief RV32I SoC Hello World 测试 - UART 输出捕获
 * 
 * 测试目标:
 * 1. 将简单程序加载到 I-TCM
 * 2. 模拟 CPU 取指和执行 UART 写指令
 * 3. 捕获 UART MMIO 写入
 * 4. 验证输出为 "Hello World!\n" 或类似内容
 * 
 * 程序功能:
 * 将 "Hello World!" 字符串通过 UART MMIO 地址 (0x40001000) 输出
 * 
 * 程序汇编 (RISC-V RV32I):
 *     li x1, 0x40001000    # UART MMIO 基地址
 *     li x2, 'H'           # 字符 'H'
 *     sw x2, 0(x1)         # 写入 UART TX
 *     li x2, 'e'           # 字符 'e'
 *     sw x2, 0(x1)         # 写入 UART TX
 *     ... (继续写入其他字符)
 *     li x2, '\n'          # 换行符
 *     sw x2, 0(x1)         # 写入 UART TX
 * 
 * 作者: DevMate
 * 日期: 2026-04-14
 */

#include "../../../tests/catch_amalgamated.hpp"
#include "ch.hpp"
#include "simulator.h"
#include "../src/rv32i_tcm.h"

#include <cstdint>
#include <string>
#include <vector>

using namespace ch;
using namespace ch::core;
using namespace riscv;

// UART MMIO 基地址 (来自 address_decoder.h)
constexpr uint32_t UART_BASE_ADDR = 0x40001000;

// ============================================================================
// RISC-V 指令编码辅助函数
// ============================================================================

/**
 * R 型指令编码
 * @param funct7 功能码 (高 7 位)
 * @param rs2 源寄存器 2
 * @param rs1 源寄存器 1
 * @param funct3 功能码 (低 3 位)
 * @param rd 目标寄存器
 * @param opcode 操作码
 */
inline uint32_t encode_r_type(uint8_t funct7, uint8_t rs2, uint8_t rs1,
                               uint8_t funct3, uint8_t rd, uint8_t opcode) {
    return (static_cast<uint32_t>(funct7) << 25) |
           (static_cast<uint32_t>(rs2) << 20) |
           (static_cast<uint32_t>(rs1) << 15) |
           (static_cast<uint32_t>(funct3) << 12) |
           (static_cast<uint32_t>(rd) << 7) |
           static_cast<uint32_t>(opcode);
}

/**
 * I 型指令编码 (包括立即数操作)
 * @param imm 立即数 (12 位)
 * @param rs1 源寄存器 1
 * @param funct3 功能码
 * @param rd 目标寄存器
 * @param opcode 操作码
 */
inline uint32_t encode_i_type(uint16_t imm, uint8_t rs1, uint8_t funct3,
                               uint8_t rd, uint8_t opcode) {
    return (static_cast<uint32_t>(imm & 0xFFF) << 20) |
           (static_cast<uint32_t>(rs1) << 15) |
           (static_cast<uint32_t>(funct3) << 12) |
           (static_cast<uint32_t>(rd) << 7) |
           static_cast<uint32_t>(opcode);
}

/**
 * S 型指令编码 (存储指令)
 * @param imm 立即数 (12 位)
 * @param rs2 源寄存器 2 (要存储的值)
 * @param rs1 基地址寄存器
 * @param funct3 功能码
 * @param opcode 操作码
 */
inline uint32_t encode_s_type(uint16_t imm, uint8_t rs2, uint8_t rs1,
                               uint8_t funct3, uint8_t opcode) {
    uint32_t imm_hi = (imm >> 5) & 0x7F;
    uint32_t imm_lo = imm & 0x1F;
    return (imm_hi << 25) |
           (static_cast<uint32_t>(rs2) << 20) |
           (static_cast<uint32_t>(rs1) << 15) |
           (static_cast<uint32_t>(funct3) << 12) |
           (imm_lo << 7) |
           static_cast<uint32_t>(opcode);
}

// ============================================================================
// RISC-V 操作码和功能码定义
// ============================================================================

namespace riscv_opcode {
    constexpr uint8_t OP_OP    = 0x33;  // 寄存器-寄存器操作
    constexpr uint8_t OP_OPIMM = 0x13;  // 立即数操作
    constexpr uint8_t OP_LOAD  = 0x03;  // 加载指令
    constexpr uint8_t OP_STORE = 0x23;  // 存储指令
    constexpr uint8_t OP_BRANCH = 0x63; // 分支指令
    constexpr uint8_t OP_JALR  = 0x67;  // 跳转寄存器链接
    constexpr uint8_t OP_JAL   = 0x6F;  // 跳转链接
    constexpr uint8_t OP_LUI   = 0x37;  // 加载高位立即数
    constexpr uint8_t OP_AUIPC = 0x17;  // 添加 PC 高位
}

namespace riscv_funct3 {
    // 寄存器-寄存器操作
    constexpr uint8_t ADD  = 0x0;  // 加
    constexpr uint8_t SUB  = 0x0;  // 减 (funct7=0x20)
    constexpr uint8_t SLL  = 0x1;  // 逻辑左移
    constexpr uint8_t SLT  = 0x2;  // 小于置位
    constexpr uint8_t SLTU = 0x3;  // 无符号小于置位
    constexpr uint8_t XOR  = 0x4;  // 异或
    constexpr uint8_t SRL  = 0x5;  // 逻辑右移
    constexpr uint8_t SRA  = 0x5;  // 算术右移 (funct7=0x20)
    constexpr uint8_t OR   = 0x6;  // 或
    constexpr uint8_t AND  = 0x7;  // 与
    
    // 存储指令
    constexpr uint8_t SB = 0x0;  // 存储字节
    constexpr uint8_t SH = 0x1;  // 存储半字
    constexpr uint8_t SW = 0x2;  // 存储字
}

namespace riscv_funct7 {
    constexpr uint8_t ADD = 0x00;
    constexpr uint8_t SUB = 0x20;
    constexpr uint8_t SRA = 0x20;
}

// ============================================================================
// UART 输出捕获器
// ============================================================================

/**
 * UART 输出捕获器
 * 模拟 UART 外设，捕获写入的数据
 */
class UartCapture {
public:
    std::string output_buffer;
    
    UartCapture() : output_buffer() {}
    
    /**
     * 写入数据到 UART
     * @param data 要写入的 32 位数据
     * @param strbe 字节使能信号
     */
    void write(uint32_t data, uint8_t strbe) {
        // 根据字节使能提取有效字节
        // strbe[0] = 1 表示最低字节有效 (little-endian)
        if (strbe & 0x1) {
            char c = static_cast<char>(data & 0xFF);
            if (c != '\0') {
                output_buffer += c;
            }
        }
        if (strbe & 0x2) {
            char c = static_cast<char>((data >> 8) & 0xFF);
            if (c != '\0') {
                output_buffer += c;
            }
        }
        if (strbe & 0x4) {
            char c = static_cast<char>((data >> 16) & 0xFF);
            if (c != '\0') {
                output_buffer += c;
            }
        }
        if (strbe & 0x8) {
            char c = static_cast<char>((data >> 24) & 0xFF);
            if (c != '\0') {
                output_buffer += c;
            }
        }
    }
    
    /**
     * 写入单个字符
     * @param c 字符
     */
    void write_char(char c) {
        if (c != '\0' && c != '\n') {
            output_buffer += c;
        } else if (c == '\n') {
            output_buffer += '\n';
        }
    }
    
    /**
     * 获取捕获的输出
     */
    std::string get_output() const {
        return output_buffer;
    }
    
    /**
     * 清空缓冲区
     */
    void clear() {
        output_buffer.clear();
    }
};

// ============================================================================
// 简单 SoC 模型
// ============================================================================

class SimpleRv32iSoc {
public:
    ch::ch_device<InstrTCM<10, 32>> itcm;
    Simulator sim;
    UartCapture uart_capture;
    
    uint32_t regs[32];
    uint32_t pc;
    
    SimpleRv32iSoc()
        : itcm()
        , sim(itcm.context())
        , uart_capture()
        , pc(0) {
        for (int i = 0; i < 32; i++) {
            regs[i] = 0;
        }
    }
    
    void load_instruction(uint32_t addr, uint32_t instruction) {
        sim.set_input_value(itcm.instance().io().write_addr, addr);
        sim.set_input_value(itcm.instance().io().write_data, instruction);
        sim.set_input_value(itcm.instance().io().write_en, 1);
        sim.tick();
        sim.set_input_value(itcm.instance().io().write_en, 0);
    }
    
    bool is_uart_address(uint32_t addr) const {
        return (addr >= 0x40001000) && (addr <= 0x40001FFF);
    }
    
    void execute_one() {
        sim.set_input_value(itcm.instance().io().addr, pc >> 2);
        sim.tick();
        auto instr_val = sim.get_port_value(itcm.instance().io().data);
        uint32_t instruction = static_cast<uint32_t>(static_cast<uint64_t>(instr_val));
        
        if (instruction == 0) {
            // NOP 或停止
            pc += 4;
            return;
        }
        
        uint8_t opcode = instruction & 0x7F;
        uint8_t rd = (instruction >> 7) & 0x1F;
        uint8_t funct3 = (instruction >> 12) & 0x7;
        uint8_t rs1 = (instruction >> 15) & 0x1F;
        uint8_t rs2 = (instruction >> 20) & 0x1F;
        
        if (opcode == riscv_opcode::OP_LUI) {
            uint32_t imm = instruction & 0xFFFFF000;
            regs[rd] = imm;
            pc += 4;
        }
        else if (opcode == riscv_opcode::OP_OPIMM) {
            int16_t imm = static_cast<int16_t>((instruction >> 20) & 0xFFF);
            if (funct3 == 0) {
                regs[rd] = regs[rs1] + static_cast<uint32_t>(imm);
            }
            pc += 4;
        }
        else if (opcode == riscv_opcode::OP_OP) {
            uint8_t funct7 = (instruction >> 25) & 0x7F;
            if (funct3 == 0 && funct7 == 0x00) {
                regs[rd] = regs[rs1] + regs[rs2];
            }
            pc += 4;
        }
        else if (opcode == riscv_opcode::OP_STORE) {
            int16_t imm = static_cast<int16_t>(
                ((instruction >> 25) & 0x7F) << 5 |
                ((instruction >> 7) & 0x1F)
            );
            uint32_t addr = regs[rs1] + static_cast<uint32_t>(imm);
            
            if (is_uart_address(addr)) {
                uart_capture.write_char(static_cast<char>(regs[rs2] & 0xFF));
            }
            pc += 4;
        }
        else {
            pc += 4;
        }
    }
    
    /**
     * 运行 N 条指令
     */
    void run_instructions(int n) {
        for (int i = 0; i < n; i++) {
            execute_one();
        }
    }
};

// ============================================================================
// 生成 Hello World 程序
// ============================================================================

/**
 * 生成 Hello World 程序
 * 程序功能: 通过 UART MMIO 地址输出 "Hello World!"
 * 
 * RISC-V 汇编:
 *     lui x1, 0x40001      # x1 = 0x40001000 (UART 基地址)
 *     addi x1, x1, 0       # x1 = x1 + 0 (确保地址正确)
 *     li x2, 'H'           # x2 = 'H' (0x48)
 *     sw x2, 0(x1)         # 存储字节到 UART
 *     li x2, 'e'           # x2 = 'e' (0x65)
 *     sw x2, 0(x1)         # 存储字节到 UART
 *     ... (继续输出 "ello World!\n")
 * 
 * 指令编码说明:
 * - lui: 加载立即数到高位 (rd = imm[31:12] << 12)
 * - addi: 立即数加法
 * - li: 伪指令，等价于 addi rd, x0, imm
 * - sw: 存储字到内存
 */
std::vector<uint32_t> generate_hello_world_program() {
    std::vector<uint32_t> program;
    
    // UART 地址: 0x40001000
    // 高 20 位: 0x40001
    constexpr uint32_t UART_BASE_HIGH = 0x40001;
    
    // 字符定义
    const char* hello = "Hello World!\n";
    
    // lui x1, 0x40001 (加载 UART 基地址高位)
    // lui 指令编码: imm[31:12] | rd | 0110111
    uint32_t lui_instr = (UART_BASE_HIGH << 12) | (1 << 7) | 0x37;
    program.push_back(lui_instr);
    
    // addi x1, x1, 0 (确保 x1 的低 12 位为 0)
    // addi 指令编码: imm[11:0] | rs1 | funct3 | rd | opcode
    // addi x1, x1, 0: imm=0, rs1=1, funct3=0, rd=1, opcode=0x13
    uint32_t addi_instr = encode_i_type(0, 1, 0, 1, riscv_opcode::OP_OPIMM);
    program.push_back(addi_instr);
    
    // 输出每个字符
    for (size_t i = 0; hello[i] != '\0'; i++) {
        char c = hello[i];
        
        // li x2, c (加载字符到 x2) = addi x2, x0, c
        uint32_t li_instr = encode_i_type(static_cast<uint16_t>(c), 0, 0, 2, riscv_opcode::OP_OPIMM);
        program.push_back(li_instr);
        
        // sw x2, 0(x1) (存储到 UART 地址)
        // sw 指令编码: imm[11:5] | rs2 | rs1 | funct3 | imm[4:0] | opcode
        // sw x2, 0(x1): imm=0, rs2=2, rs1=1, funct3=2, opcode=0x23
        uint32_t sw_instr = encode_s_type(0, 2, 1, riscv_funct3::SW, riscv_opcode::OP_STORE);
        program.push_back(sw_instr);
    }
    
    // 数据完整后添加 NOP
    program.push_back(0x00000013);  // addi x0, x0, 0 (NOP)
    
    return program;
}

// ============================================================================
// 测试用例
// ============================================================================

TEST_CASE("SoC Hello World: UART output captured", "[soc][uart][hello-world]") {
    context ctx("soc_hello_world");
    ctx_swap swap(&ctx);
    
    // 创建简单 SoC
    SimpleRv32iSoc soc;
    
    // 生成 Hello World 程序
    auto program = generate_hello_world_program();
    
    // 加载程序到 I-TCM
    for (size_t i = 0; i < program.size(); i++) {
        soc.load_instruction(static_cast<uint32_t>(i), program[i]);
    }
    
    // 运行程序 (每条指令 1 个周期)
    // 程序长度大约是 2 + 13*2 + 1 = 29 条指令
    soc.run_instructions(static_cast<int>(program.size()) + 10);
    
    // 获取 UART 输出
    std::string uart_output = soc.uart_capture.get_output();
    
    // 验证输出
    std::string expected = "Hello World!\n";
    REQUIRE(uart_output == expected);
}

TEST_CASE("SoC Hello World: UART address range check", "[soc][uart][address]") {
    SimpleRv32iSoc soc;
    
    SECTION("UART address 0x40001000 is recognized") {
        REQUIRE(soc.is_uart_address(0x40001000) == true);
    }
    
    SECTION("UART address 0x40001FFF is recognized") {
        REQUIRE(soc.is_uart_address(0x40001FFF) == true);
    }
    
    SECTION("I-TCM address 0x20000000 is NOT UART") {
        REQUIRE(soc.is_uart_address(0x20000000) == false);
    }
    
    SECTION("Invalid address 0x10000000 is NOT UART") {
        REQUIRE(soc.is_uart_address(0x10000000) == false);
    }
}

TEST_CASE("SoC Hello World: I-TCM instruction loading", "[soc][itcm][instruction-load]") {
    context ctx("soc_itcm_load");
    ctx_swap swap(&ctx);
    
    // 创建 I-TCM
    ch::ch_device<InstrTCM<10, 32>> itcm;
    Simulator sim(itcm.context());
    
    SECTION("Load and read back instruction") {
        // 加载一条指令: ADDI x1, x0, 42 = 0x02A00093
        uint32_t test_instr = 0x02A00093;
        
        sim.set_input_value(itcm.instance().io().write_addr, 0);
        sim.set_input_value(itcm.instance().io().write_data, test_instr);
        sim.set_input_value(itcm.instance().io().write_en, 1);
        sim.tick();
        sim.set_input_value(itcm.instance().io().write_en, 0);
        
        // 读取指令
        sim.set_input_value(itcm.instance().io().addr, 0);
        sim.tick();
        
        auto loaded = sim.get_port_value(itcm.instance().io().data);
        REQUIRE(static_cast<uint32_t>(static_cast<uint64_t>(loaded)) == test_instr);
    }
    
    SECTION("Load multiple instructions") {
        std::vector<uint32_t> instructions = {
            0x00000013,  // NOP
            0x02A00093,  // ADDI x1, x0, 42
            0x00A00113,  // ADDI x2, x0, 10
            0x002081B3   // ADD x3, x1, x2
        };
        
        // 加载所有指令
        for (size_t i = 0; i < instructions.size(); i++) {
            sim.set_input_value(itcm.instance().io().write_addr, static_cast<uint32_t>(i));
            sim.set_input_value(itcm.instance().io().write_data, instructions[i]);
            sim.set_input_value(itcm.instance().io().write_en, 1);
            sim.tick();
        }
        sim.set_input_value(itcm.instance().io().write_en, 0);
        
        // 验证每条指令
        for (size_t i = 0; i < instructions.size(); i++) {
            sim.set_input_value(itcm.instance().io().addr, static_cast<uint32_t>(i));
            sim.tick();
            
            auto loaded = sim.get_port_value(itcm.instance().io().data);
            REQUIRE(static_cast<uint32_t>(static_cast<uint64_t>(loaded)) == instructions[i]);
        }
    }
}

TEST_CASE("SoC Hello World: Simple program execution", "[soc][execution][simple]") {
    context ctx("soc_simple_exec");
    ctx_swap swap(&ctx);
    
    SimpleRv32iSoc soc;
    
    // 简单程序: 将 'A' 写入 UART
    // lui x1, 0x40001  (UART 基地址)
    // addi x1, x1, 0
    // li x2, 'A'       (addi x2, x0, 'A')
    // sw x2, 0(x1)
    
    std::vector<uint32_t> simple_prog = {
        (0x40001 << 12) | (1 << 7) | 0x37,     // lui x1, 0x40001
        encode_i_type(0, 1, 0, 1, riscv_opcode::OP_OPIMM),  // addi x1, x1, 0
        encode_i_type('A', 0, 0, 2, riscv_opcode::OP_OPIMM), // li x2, 'A'
        encode_s_type(0, 2, 1, riscv_funct3::SW, riscv_opcode::OP_STORE)  // sw x2, 0(x1)
    };
    
    // 加载程序
    for (size_t i = 0; i < simple_prog.size(); i++) {
        soc.load_instruction(static_cast<uint32_t>(i), simple_prog[i]);
    }
    
    // 运行程序
    soc.run_instructions(static_cast<int>(simple_prog.size()) + 5);
    
    // 验证输出
    REQUIRE(soc.uart_capture.get_output() == "A");
}
