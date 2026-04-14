/**
 * @file test_riscv_tests.cpp
 * @brief RISC-V Tests ISA 集成测试
 * 
 * 测试目标:
 * 1. 加载 riscv-tests 编译的 .bin 文件
 * 2. 运行 RV32I 指令集测试
 * 3. 监控 tohost (0x80001000) 判断测试结果
 * 
 * riscv-tests 规格:
 * - 入口点: 0x80000000 (_start)
 * - tohost: 0x80001000 (1=pass, 0=fail)
 * - 二进制文件: /workspace/project/riscv-tests/build/rv32ui/*.bin
 * 
 * 作者: DevMate
 * 日期: 2026-04-14
 */

#include "../../../tests/catch_amalgamated.hpp"
#include "ch.hpp"
#include "simulator.h"
#include "../src/rv32i_tcm.h"

#include <cstdint>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>

using namespace ch;
using namespace ch::core;
using namespace riscv;

// ============================================================================
// RISC-V 常量定义
// ============================================================================

constexpr uint32_t RV32I_ENTRY_POINT = 0x80000000;
constexpr uint32_t RV32I_TOHOST_ADDR = 0x80001000;
constexpr uint32_t RV32I_TOHOST_WORD_INDEX = RV32I_TOHOST_ADDR / 4;
constexpr uint32_t RV32I_MEM_SIZE = 65536;  // 256KB 内存
constexpr uint32_t MAX_TICKS = 10000;       // 最大周期数 (防止无限循环)

// ============================================================================
// RISC-V 操作码和功能码
// ============================================================================

namespace rv32i_opcode {
    constexpr uint8_t LOAD    = 0x03;
    constexpr uint8_t OP_IMM  = 0x13;
    constexpr uint8_t AUIPC   = 0x17;
    constexpr uint8_t STORE   = 0x23;
    constexpr uint8_t OP      = 0x33;
    constexpr uint8_t BRANCH  = 0x63;
    constexpr uint8_t JALR    = 0x67;
    constexpr uint8_t JAL     = 0x6F;
    constexpr uint8_t LUI     = 0x37;
    constexpr uint8_t SYSTEM  = 0x73;
}

namespace rv32i_funct3 {
    // 加载/存储
    constexpr uint8_t LB  = 0x0;
    constexpr uint8_t LH  = 0x1;
    constexpr uint8_t LW  = 0x2;
    constexpr uint8_t LBU = 0x4;
    constexpr uint8_t LHU = 0x5;
    constexpr uint8_t SB  = 0x0;
    constexpr uint8_t SH  = 0x1;
    constexpr uint8_t SW  = 0x2;
    
    // ALU R 类型
    constexpr uint8_t ADD_SUB = 0x0;
    constexpr uint8_t SLL     = 0x1;
    constexpr uint8_t SLT     = 0x2;
    constexpr uint8_t SLTU    = 0x3;
    constexpr uint8_t XOR     = 0x4;
    constexpr uint8_t SRL_SRA = 0x5;
    constexpr uint8_t OR      = 0x6;
    constexpr uint8_t AND     = 0x7;
    
    // 分支
    constexpr uint8_t BEQ  = 0x0;
    constexpr uint8_t BNE  = 0x1;
    constexpr uint8_t BLT  = 0x4;
    constexpr uint8_t BGE  = 0x5;
    constexpr uint8_t BLTU = 0x6;
    constexpr uint8_t BGEU = 0x7;
}

namespace rv32i_funct7 {
    constexpr uint8_t ADD  = 0x00;
    constexpr uint8_t SUB  = 0x20;
    constexpr uint8_t SRA  = 0x20;
    constexpr uint8_t SRL  = 0x00;
}

// ============================================================================
// 简单内存模型
// ============================================================================

/**
 * RISC-V 测试用简单内存
 * 256KB 内存区域, 映射到 0x80000000
 */
class Rv32iSimpleMem {
public:
    static constexpr size_t WORD_COUNT = RV32I_MEM_SIZE / 4;
    
    Rv32iSimpleMem() {
        memset(memory, 0, sizeof(memory));
    }
    
    /**
     * 加载二进制文件到内存
     * @param file_path 二进制文件路径
     * @param offset 加载偏移 (相对于 0x80000000)
     * @return 成功返回 true
     */
    bool load_binary(const std::string& file_path, uint32_t offset = 0) {
        std::ifstream file(file_path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            return false;
        }
        
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        std::vector<uint8_t> buffer(size);
        if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
            return false;
        }
        
        // 将字节转换为 32 位字 (little-endian)
        uint32_t word_offset = offset / 4;
        for (size_t i = 0; i + 3 < size; i += 4) {
            uint32_t word = static_cast<uint32_t>(buffer[i]) |
                           (static_cast<uint32_t>(buffer[i + 1]) << 8) |
                           (static_cast<uint32_t>(buffer[i + 2]) << 16) |
                           (static_cast<uint32_t>(buffer[i + 3]) << 24);
            if (word_offset + i / 4 < WORD_COUNT) {
                memory[word_offset + i / 4] = word;
            }
        }
        
        return true;
    }
    
    /**
     * 读取 32 位字
     */
    uint32_t read_word(uint32_t addr) const {
        uint32_t index = (addr - RV32I_ENTRY_POINT) / 4;
        if (index >= WORD_COUNT) {
            return 0;
        }
        return memory[index];
    }
    
    /**
     * 写入 32 位字
     */
    void write_word(uint32_t addr, uint32_t value) {
        uint32_t index = (addr - RV32I_ENTRY_POINT) / 4;
        if (index >= WORD_COUNT) {
            return;
        }
        memory[index] = value;
    }
    
    /**
     * 读取字节 (用于 lb/lbu)
     */
    uint8_t read_byte(uint32_t addr) const {
        uint32_t word = read_word(addr & ~0x3);
        uint32_t offset = addr & 0x3;
        return static_cast<uint8_t>((word >> (offset * 8)) & 0xFF);
    }
    
    /**
     * 写入字节 (用于 sb)
     */
    void write_byte(uint32_t addr, uint8_t value) {
        uint32_t word = read_word(addr & ~0x3);
        uint32_t offset = addr & 0x3;
        word = (word & ~(0xFF << (offset * 8))) | (static_cast<uint32_t>(value) << (offset * 8));
        write_word(addr & ~0x3, word);
    }
    
    /**
     * 读取半字 (用于 lh/lhu)
     */
    uint16_t read_half(uint32_t addr) const {
        uint32_t word = read_word(addr & ~0x3);
        uint32_t offset = addr & 0x3;
        if (offset <= 2) {
            return static_cast<uint16_t>((word >> (offset * 8)) & 0xFFFF);
        }
        return 0;
    }
    
    /**
     * 写入半字 (用于 sh)
     */
    void write_half(uint32_t addr, uint16_t value) {
        uint32_t word = read_word(addr & ~0x3);
        uint32_t offset = addr & 0x3;
        word = (word & ~(0xFFFF << (offset * 8))) | (static_cast<uint32_t>(value) << (offset * 8));
        write_word(addr & ~0x3, word);
    }

private:
    uint32_t memory[WORD_COUNT];
};

// ============================================================================
// 最小化 RV32I 解释器
// ============================================================================

/**
 * RV32I 指令执行结果
 */
enum class ExecResult {
    CONTINUE,
    EBREAK,
    ECALL,
    TOHOST_SET
};

/**
 * RV32I 简单解释器
 * 支持 RV32I 基指令集, 用于运行 riscv-tests
 */
class Rv32iInterpreter {
public:
    Rv32iSimpleMem memory;
    uint32_t pc;
    uint32_t regs[32];
    uint32_t tohost_value;
    bool tohost_written;
    
    Rv32iInterpreter() 
        : pc(RV32I_ENTRY_POINT)
        , tohost_value(0)
        , tohost_written(false) 
    {
        for (int i = 0; i < 32; i++) {
            regs[i] = 0;
        }
    }
    
    /**
     * 加载二进制文件
     */
    bool load_binary(const std::string& path) {
        return memory.load_binary(path, 0);
    }
    
    /**
     * 执行一条指令
     */
    ExecResult step() {
        if (pc < RV32I_ENTRY_POINT || pc >= RV32I_ENTRY_POINT + RV32I_MEM_SIZE) {
            return ExecResult::EBREAK;
        }
        
        uint32_t instr = memory.read_word(pc);
        if (instr == 0) {
            pc += 4;
            return ExecResult::CONTINUE;
        }
        
        uint8_t opcode = instr & 0x7F;
        uint8_t rd = (instr >> 7) & 0x1F;
        uint8_t funct3 = (instr >> 12) & 0x7;
        uint8_t rs1 = (instr >> 15) & 0x1F;
        uint8_t rs2 = (instr >> 20) & 0x1F;
        uint32_t funct7 = (instr >> 25) & 0x7F;
        
        switch (opcode) {
            case rv32i_opcode::LUI:
                regs[rd] = instr & 0xFFFFF000;
                pc += 4;
                break;
                
            case rv32i_opcode::AUIPC:
                regs[rd] = pc + (instr & 0xFFFFF000);
                pc += 4;
                break;
                
            case rv32i_opcode::JALR: {
                int32_t imm = static_cast<int32_t>((instr >> 20) & 0xFFF);
                imm = (imm << 20) >> 20;
                uint32_t target = (regs[rs1] + imm) & ~1U;
                regs[rd] = pc + 4;
                pc = target;
                break;
            }
                
            case rv32i_opcode::JAL:
                regs[rd] = pc + 4;
                {
                    int32_t imm = 0;
                    imm |= ((instr >> 31) & 0x1) << 20;
                    imm |= ((instr >> 21) & 0x3FF) << 1;
                    imm |= ((instr >> 20) & 0x1) << 11;
                    imm |= ((instr >> 12) & 0xFF) << 12;
                    imm = (imm << 11) >> 11;
                    pc += imm;
                }
                break;
                
            case rv32i_opcode::BRANCH: {
                int32_t offset = 0;
                offset |= ((instr >> 31) & 0x1) << 12;
                offset |= ((instr >> 7) & 0x1) << 11;
                offset |= ((instr >> 25) & 0x3F) << 5;
                offset |= ((instr >> 8) & 0xF) << 1;
                offset = (offset << 19) >> 19;
                
                bool taken = false;
                switch (funct3) {
                    case rv32i_funct3::BEQ:  taken = (regs[rs1] == regs[rs2]); break;
                    case rv32i_funct3::BNE:  taken = (regs[rs1] != regs[rs2]); break;
                    case rv32i_funct3::BLT:  taken = (static_cast<int32_t>(regs[rs1]) < static_cast<int32_t>(regs[rs2])); break;
                    case rv32i_funct3::BGE:  taken = (static_cast<int32_t>(regs[rs1]) >= static_cast<int32_t>(regs[rs2])); break;
                    case rv32i_funct3::BLTU: taken = (regs[rs1] < regs[rs2]); break;
                    case rv32i_funct3::BGEU: taken = (regs[rs1] >= regs[rs2]); break;
                    default: break;
                }
                if (taken) {
                    pc += offset;
                } else {
                    pc += 4;
                }
                break;
            }
                
            case rv32i_opcode::LOAD: {
                int32_t imm = static_cast<int32_t>((instr >> 20) & 0xFFF);
                imm = (imm << 20) >> 20;
                uint32_t addr = regs[rs1] + imm;
                
                switch (funct3) {
                    case rv32i_funct3::LB:  regs[rd] = static_cast<int32_t>(static_cast<int8_t>(memory.read_byte(addr))); break;
                    case rv32i_funct3::LH:  regs[rd] = static_cast<int32_t>(static_cast<int16_t>(memory.read_half(addr))); break;
                    case rv32i_funct3::LW:  regs[rd] = memory.read_word(addr); break;
                    case rv32i_funct3::LBU: regs[rd] = memory.read_byte(addr); break;
                    case rv32i_funct3::LHU: regs[rd] = memory.read_half(addr); break;
                    default: break;
                }
                pc += 4;
                break;
            }
                
            case rv32i_opcode::STORE: {
                int32_t imm = 0;
                imm |= ((instr >> 25) & 0x7F) << 5;
                imm |= ((instr >> 7) & 0x1F);
                imm = (imm << 20) >> 20;
                uint32_t addr = regs[rs1] + imm;
                
                switch (funct3) {
                    case rv32i_funct3::SB: memory.write_byte(addr, regs[rs2] & 0xFF); break;
                    case rv32i_funct3::SH: memory.write_half(addr, regs[rs2] & 0xFFFF); break;
                    case rv32i_funct3::SW: memory.write_word(addr, regs[rs2]); break;
                    default: break;
                }
                pc += 4;
                break;
            }
                
            case rv32i_opcode::OP_IMM: {
                int32_t imm = static_cast<int32_t>((instr >> 20) & 0xFFF);
                imm = (imm << 20) >> 20;
                
                switch (funct3) {
                    case rv32i_funct3::ADD_SUB: regs[rd] = regs[rs1] + imm; break;
                    case rv32i_funct3::SLL:     regs[rd] = regs[rs1] << (imm & 0x1F); break;
                    case rv32i_funct3::SLT:     regs[rd] = (static_cast<int32_t>(regs[rs1]) < imm) ? 1 : 0; break;
                    case rv32i_funct3::SLTU:    regs[rd] = (regs[rs1] < static_cast<uint32_t>(imm)) ? 1 : 0; break;
                    case rv32i_funct3::XOR:     regs[rd] = regs[rs1] ^ imm; break;
                    case rv32i_funct3::SRL_SRA:
                        if (funct7 == rv32i_funct7::SRA) {
                            regs[rd] = static_cast<uint32_t>(static_cast<int32_t>(regs[rs1]) >> (imm & 0x1F));
                        } else {
                            regs[rd] = regs[rs1] >> (imm & 0x1F);
                        }
                        break;
                    case rv32i_funct3::OR:      regs[rd] = regs[rs1] | imm; break;
                    case rv32i_funct3::AND:     regs[rd] = regs[rs1] & imm; break;
                    default: break;
                }
                pc += 4;
                break;
            }
                
            case rv32i_opcode::OP: {
                switch (funct3) {
                    case rv32i_funct3::ADD_SUB:
                        if (funct7 == rv32i_funct7::SUB) {
                            regs[rd] = regs[rs1] - regs[rs2];
                        } else {
                            regs[rd] = regs[rs1] + regs[rs2];
                        }
                        break;
                    case rv32i_funct3::SLL:     regs[rd] = regs[rs1] << (regs[rs2] & 0x1F); break;
                    case rv32i_funct3::SLT:     regs[rd] = (static_cast<int32_t>(regs[rs1]) < static_cast<int32_t>(regs[rs2])) ? 1 : 0; break;
                    case rv32i_funct3::SLTU:    regs[rd] = (regs[rs1] < regs[rs2]) ? 1 : 0; break;
                    case rv32i_funct3::XOR:     regs[rd] = regs[rs1] ^ regs[rs2]; break;
                    case rv32i_funct3::SRL_SRA:
                        if (funct7 == rv32i_funct7::SRA) {
                            regs[rd] = static_cast<uint32_t>(static_cast<int32_t>(regs[rs1]) >> (regs[rs2] & 0x1F));
                        } else {
                            regs[rd] = regs[rs1] >> (regs[rs2] & 0x1F);
                        }
                        break;
                    case rv32i_funct3::OR:      regs[rd] = regs[rs1] | regs[rs2]; break;
                    case rv32i_funct3::AND:     regs[rd] = regs[rs1] & regs[rs2]; break;
                    default: break;
                }
                pc += 4;
                break;
            }
                
            case rv32i_opcode::SYSTEM: {
                if (funct3 == 0 && rs2 == 0) {
                    return ExecResult::EBREAK;
                }
                pc += 4;
                break;
            }
                
            default:
                pc += 4;
                break;
        }
        
        regs[0] = 0;
        
        uint32_t current_tohost = memory.read_word(RV32I_TOHOST_ADDR);
        if (current_tohost != 0 && current_tohost != tohost_value) {
            tohost_value = current_tohost;
            tohost_written = true;
            return ExecResult::TOHOST_SET;
        }
        
        return ExecResult::CONTINUE;
    }
    
    bool run_until_tohost() {
        tohost_value = 0;
        tohost_written = false;
        
        for (uint32_t tick = 0; tick < MAX_TICKS; tick++) {
            ExecResult result = step();
            
            if (result == ExecResult::TOHOST_SET) {
                return tohost_value == 1;
            }
            
            if (result == ExecResult::EBREAK) {
                uint32_t th = memory.read_word(RV32I_TOHOST_ADDR);
                if (th != 0) {
                    tohost_value = th;
                    return th == 1;
                }
                return false;
            }
        }
        
        return false;
    }
};

// ============================================================================
// 辅助函数
// ============================================================================

/**
 * 运行单个 riscv-test
 * @param test_name 测试名称
 * @param bin_path 二进制文件路径
 * @return true 如果测试通过
 */
bool run_riscv_test(const std::string& test_name, const std::string& bin_path) {
    Rv32iInterpreter interpreter;
    
    if (!interpreter.load_binary(bin_path)) {
        std::cerr << "Failed to load binary: " << bin_path << std::endl;
        return false;
    }
    
    bool passed = interpreter.run_until_tohost();
    
    std::cout << "  " << test_name << ": " 
              << (passed ? "PASS" : "FAIL") 
              << " (tohost=0x" << std::hex << interpreter.tohost_value << std::dec;
    if (!passed && interpreter.tohost_value == 0) {
        std::cout << ", timeout or no tohost write";
    }
    std::cout << ")" << std::endl;
    
    return passed;
}

// ============================================================================
// riscv-tests 二进制文件路径
// ============================================================================

const char* RISCV_TESTS_PATH = "/workspace/project/riscv-tests/build/rv32ui/";

const std::vector<std::pair<const char*, const char*>> RV32UI_TESTS = {
    {"add", "add.bin"},
    {"addi", "addi.bin"},
    {"and", "and.bin"},
    {"andi", "andi.bin"},
    {"auipc", "auipc.bin"},
    {"beq", "beq.bin"},
    {"bge", "bge.bin"},
    {"bgeu", "bgeu.bin"},
    {"blt", "blt.bin"},
    {"bltu", "bltu.bin"},
    {"bne", "bne.bin"},
    {"fence_i", "fence_i.bin"},
    {"jal", "jal.bin"},
    {"jalr", "jalr.bin"},
    {"lb", "lb.bin"},
    {"lbu", "lbu.bin"},
    {"lh", "lh.bin"},
    {"lhu", "lhu.bin"},
    {"lui", "lui.bin"},
    {"lw", "lw.bin"},
    {"ma_data", "ma_data.bin"},
    {"or", "or.bin"},
    {"ori", "ori.bin"},
    {"sb", "sb.bin"},
    {"sh", "sh.bin"},
    {"simple", "simple.bin"},
    {"sll", "sll.bin"},
    {"slli", "slli.bin"},
    {"slt", "slt.bin"},
    {"slti", "slti.bin"},
    {"sltiu", "sltiu.bin"},
    {"sltu", "sltu.bin"},
    {"sra", "sra.bin"},
    {"srai", "srai.bin"},
    {"srl", "srl.bin"},
    {"srli", "srli.bin"},
    {"st_ld", "st_ld.bin"},
    {"sub", "sub.bin"},
    {"sw", "sw.bin"},
    {"xor", "xor.bin"},
    {"xori", "xori.bin"},
};

// ============================================================================
// 测试用例
// ============================================================================

TEST_CASE("rv32i_tests: simple sanity check", "[rv32i][riscv-tests]") {
    // 简单测试: 验证解释器基本功能
    Rv32iInterpreter interp;
    REQUIRE(interp.pc == RV32I_ENTRY_POINT);
    REQUIRE(interp.regs[0] == 0);
}

TEST_CASE("rv32i_tests: memory load and read", "[rv32i][riscv-tests]") {
    Rv32iInterpreter interp;
    
    // 写入一个值到内存
    interp.memory.write_word(RV32I_ENTRY_POINT, 0xDEADBEEF);
    
    // 读回来
    uint32_t val = interp.memory.read_word(RV32I_ENTRY_POINT);
    REQUIRE(val == 0xDEADBEEF);
}

TEST_CASE("rv32i_tests: add instruction", "[rv32i][riscv-tests]") {
    // ADDI x1, x0, 10
    // ADDI x2, x0, 20
    // ADD x3, x1, x2  (x3 should be 30)
    
    Rv32iInterpreter interp;
    
    // 加载简单程序
    // lui x1, 10 (实际上用 addi)
    // addi x1, x0, 10 = 0x00A00093
    // addi x2, x0, 20 = 0x01400113
    // add x3, x1, x2 = 0x002181B3
    
    uint32_t prog[] = {
        0x00A00093,  // addi x1, x0, 10
        0x01400113,  // addi x2, x0, 20
        0x002181B3   // add x3, x1, x2
    };
    
    for (size_t i = 0; i < 3; i++) {
        interp.memory.write_word(RV32I_ENTRY_POINT + i * 4, prog[i]);
    }
    
    // 执行 3 条指令
    for (int i = 0; i < 3; i++) {
        interp.step();
    }
    
    REQUIRE(interp.regs[1] == 10);
    REQUIRE(interp.regs[2] == 20);
    REQUIRE(interp.regs[3] == 30);
}

TEST_CASE("rv32i_tests: load/store instructions", "[rv32i][riscv-tests]") {
    Rv32iInterpreter interp;
    
    // 存储一个值然后加载
    uint32_t test_addr = RV32I_ENTRY_POINT + 0x100;
    uint32_t test_value = 0x12345678;
    
    // sw x1, 0(x2)
    // lw x3, 0(x2)
    
    // lui x1, imm (加载地址高位)
    uint32_t lui_instr = (0x80000 << 12) | (1 << 7) | 0x37;  // lui x1, 0x80000
    
    // 直接设置寄存器然后执行
    interp.regs[1] = test_addr;
    interp.regs[2] = test_value;
    
    // SW: 存储
    uint32_t sw_instr = 0x00022023;  // sw x2, 0(x1)
    interp.memory.write_word(interp.pc, sw_instr);
    interp.step();
    
    // LW: 加载
    uint32_t lw_instr = 0x00020103;  // lw x2, 0(x1)
    interp.memory.write_word(interp.pc, lw_instr);
    interp.step();
    
    REQUIRE(interp.regs[2] == test_value);
}

TEST_CASE("rv32i_tests: branch instructions", "[rv32i][riscv-tests]") {
    Rv32iInterpreter interp;
    
    // 测试 beq
    // addi x1, x0, 10
    // addi x2, x0, 10
    // beq x1, x2, skip (偏移 8 字节)
    // addi x3, x0, 1
    // skip:
    // addi x3, x0, 2
    
    uint32_t prog[] = {
        0x00A00093,  // addi x1, x0, 10
        0x00A00113,  // addi x2, x0, 10
        0x00408263,  // beq x1, x2, +8 (PC+8)
        0x00100113,  // addi x3, x0, 1
        0x00200113   // addi x3, x0, 2
    };
    
    for (size_t i = 0; i < 5; i++) {
        interp.memory.write_word(interp.pc + i * 4, prog[i]);
    }
    
    // 执行 4 条 (跳过 beq 后面的 addi x3, x0, 1)
    for (int i = 0; i < 4; i++) {
        interp.step();
    }
    
    // x3 应该是 2 (beq 跳过了 x3=1 那条指令)
    REQUIRE(interp.regs[3] == 2);
}

TEST_CASE("rv32i_tests: jal instructions", "[rv32i][riscv-tests]") {
    Rv32iInterpreter interp;
    
    // JAL: 跳转并链接
    // jal x1, +8  (跳转到 8 字节后, 返回地址存 x1)
    // addi x2, x0, 1
    // addi x2, x0, 2
    // addi x2, x0, 3
    
    uint32_t prog[] = {
        0x008000EF,  // jal x1, +8
        0x00100113,  // addi x2, x0, 1
        0x00200113,  // addi x2, x0, 2
        0x00300113   // addi x2, x0, 3
    };
    
    for (size_t i = 0; i < 4; i++) {
        interp.memory.write_word(interp.pc + i * 4, prog[i]);
    }
    
    // 执行 jal 指令
    interp.step();
    
    // x1 应该保存返回地址 (pc + 4)
    REQUIRE(interp.regs[1] == RV32I_ENTRY_POINT + 4);
    // pc 应该跳转到 prog[2] (跳过 prog[1])
    REQUIRE(interp.pc == RV32I_ENTRY_POINT + 8);
}

TEST_CASE("rv32i_tests: binary file loading", "[rv32i][riscv-tests][file]") {
    std::string simple_path = std::string(RISCV_TESTS_PATH) + "simple.bin";
    
    Rv32iInterpreter interp;
    bool loaded = interp.load_binary(simple_path);
    
    // 如果文件存在, 验证加载成功
    if (loaded) {
        // 至少应该加载了一些指令
        uint32_t first_word = interp.memory.read_word(RV32I_ENTRY_POINT);
        REQUIRE(first_word != 0);
    }
}

TEST_CASE("rv32i_tests: simple binary execution", "[rv32i][riscv-tests][file]") {
    std::string simple_path = std::string(RISCV_TESTS_PATH) + "simple.bin";
    
    Rv32iInterpreter interp;
    if (!interp.load_binary(simple_path)) {
        SKIP("simple.bin not found");
    }
    
    bool passed = interp.run_until_tohost();
    REQUIRE(passed == true);
}

// 运行单个测试的辅助 TEST_CASE 工厂
#define TEST_CASE_FOR_each(test_name, test_file) \
TEST_CASE("rv32i_tests: " #test_name, "[rv32i][riscv-tests][" #test_name "]") { \
    std::string path = std::string(RISCV_TESTS_PATH) + test_file; \
    bool passed = run_riscv_test(#test_name, path); \
    REQUIRE(passed == true); \
}

// ============================================================================
// RV32UI 测试用例 - 每个测试一个 TEST_CASE
// ============================================================================

TEST_CASE_FOR_each(add, "add.bin")
TEST_CASE_FOR_each(addi, "addi.bin")
TEST_CASE_FOR_each(and, "and.bin")
TEST_CASE_FOR_each(andi, "andi.bin")
TEST_CASE_FOR_each(auipc, "auipc.bin")
TEST_CASE_FOR_each(beq, "beq.bin")
TEST_CASE_FOR_each(bge, "bge.bin")
TEST_CASE_FOR_each(bgeu, "bgeu.bin")
TEST_CASE_FOR_each(blt, "blt.bin")
TEST_CASE_FOR_each(bltu, "bltu.bin")
TEST_CASE_FOR_each(bne, "bne.bin")
TEST_CASE_FOR_each(fence_i, "fence_i.bin")
TEST_CASE_FOR_each(jal, "jal.bin")
TEST_CASE_FOR_each(jalr, "jalr.bin")
TEST_CASE_FOR_each(lb, "lb.bin")
TEST_CASE_FOR_each(lbu, "lbu.bin")
TEST_CASE_FOR_each(lh, "lh.bin")
TEST_CASE_FOR_each(lhu, "lhu.bin")
TEST_CASE_FOR_each(lui, "lui.bin")
TEST_CASE_FOR_each(lw, "lw.bin")
TEST_CASE_FOR_each(ma_data, "ma_data.bin")
TEST_CASE_FOR_each(or, "or.bin")
TEST_CASE_FOR_each(ori, "ori.bin")
TEST_CASE_FOR_each(sb, "sb.bin")
TEST_CASE_FOR_each(sh, "sh.bin")
TEST_CASE_FOR_each(simple, "simple.bin")
TEST_CASE_FOR_each(sll, "sll.bin")
TEST_CASE_FOR_each(slli, "slli.bin")
TEST_CASE_FOR_each(slt, "slt.bin")
TEST_CASE_FOR_each(slti, "slti.bin")
TEST_CASE_FOR_each(sltiu, "sltiu.bin")
TEST_CASE_FOR_each(sltu, "sltu.bin")
TEST_CASE_FOR_each(sra, "sra.bin")
TEST_CASE_FOR_each(srai, "srai.bin")
TEST_CASE_FOR_each(srl, "srl.bin")
TEST_CASE_FOR_each(srli, "srli.bin")
TEST_CASE_FOR_each(st_ld, "st_ld.bin")
TEST_CASE_FOR_each(sub, "sub.bin")
TEST_CASE_FOR_each(sw, "sw.bin")
TEST_CASE_FOR_each(xor, "xor.bin")
TEST_CASE_FOR_each(xori, "xori.bin")
