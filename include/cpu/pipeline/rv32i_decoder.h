/**
 * @file rv32i_decoder.h
 * @brief RV32I 指令译码器
 * 
 * 支持指令格式:
 * - R-type: 寄存器 - 寄存器操作
 * - I-type: 立即数操作/加载
 * - S-type: 存储
 * - B-type: 分支
 * - U-type: 上位立即数
 * - J-type: 跳转
 */

#pragma once

#include "ch.hpp"
#include "component.h"

namespace riscv {

using namespace ch::core;

// 指令字段结构
struct RvInstrFields {
    ch_uint<7> opcode;
    ch_uint<5> rd;
    ch_uint<3> funct3;
    ch_uint<5> rs1;
    ch_uint<5> rs2;
    ch_uint<7> funct7;
    ch_uint<32> imm;  // 符号扩展后的立即数
};

// 指令类型枚举
constexpr unsigned INSTR_R = 0;
constexpr unsigned INSTR_I = 1;
constexpr unsigned INSTR_S = 2;
constexpr unsigned INSTR_B = 3;
constexpr unsigned INSTR_U = 4;
constexpr unsigned INSTR_J = 5;

// Opcode 定义
constexpr unsigned OP_LOAD   = 0b0000011;
constexpr unsigned OP_STORE  = 0b0100011;
constexpr unsigned OP_BRANCH = 0b1100011;
constexpr unsigned OP_JALR   = 0b1100111;
constexpr unsigned OP_JAL    = 0b1101111;
constexpr unsigned OP_LUI    = 0b0110111;
constexpr unsigned OP_AUIPC  = 0b0010111;
constexpr unsigned OP_OPIMM  = 0b0010011;
constexpr unsigned OP_OP     = 0b0110011;
constexpr unsigned OP_SYSTEM = 0b1110011;
// ⚠️ **2026-11-15 · B4 · G4**：`FENCE` / `FENCE.I`（opcode `0x0F`）此前**完全不在**
//    译码表里 ⇒ ⇒ 与「未知 opcode」**混为一谈**。本版把它**显式**列为已知指令
//    （行为 = **NOP**：单核无 cache ⇒ 无排序语义；`FENCE.I` 同理，无 I-cache）
//    ⇒ ⇒ 依据 = `rv32i-isa-gap-closure-2026-11` 的 G4。
constexpr unsigned OP_FENCE = 0b0001111;

class Rv32iDecoder : public ch::Component {
public:
    __io(
        ch_in<ch_uint<32>> instruction;
        ch_out<ch_uint<7>>  opcode;
        ch_out<ch_uint<5>>  rd;
        ch_out<ch_uint<3>>  funct3;
        ch_out<ch_uint<5>>  rs1;
        ch_out<ch_uint<5>>  rs2;
        ch_out<ch_uint<7>>  funct7;
        ch_out<ch_uint<32>> imm;       // 符号扩展立即数
        ch_out<ch_uint<3>>  instr_type; // 指令类型
        // ⚠️ **2026-11-16 · B4 · G1 扩展**：**保留组合**判定（ISA 的非法编码）
        //    0 = 合法 · 1 = **已知未实现**（`OP_SYSTEM`）· 2 = **未知 opcode**
        //    · 3 = 已知 opcode 但 **funct3 / funct7 是保留组合**
        ch_out<ch_uint<2>>  illegal_reason{"illegal_reason"};   // ⚠️ **具名**（端口命名契约）
    )
    
    Rv32iDecoder(ch::Component* parent = nullptr, const std::string& name = "rv32i_decoder")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // ========================================================================
        // 提取指令字段 (组合逻辑)
        // ========================================================================
        
        ch_uint<7>  op(0_d);
        ch_uint<5>  dest(0_d);
        ch_uint<3>  f3(0_d);
        ch_uint<5>  src1(0_d);
        ch_uint<5>  src2(0_d);
        ch_uint<7>  f7(0_d);
        ch_uint<32> imm_val(0_d);
        ch_uint<3>  type(INSTR_R);
        
        // 提取固定字段
        op = bits<6, 0>(io().instruction);
        dest = bits<11, 7>(io().instruction);
        f3 = bits<14, 12>(io().instruction);
        src1 = bits<19, 15>(io().instruction);
        src2 = bits<24, 20>(io().instruction);
        f7 = bits<31, 25>(io().instruction);
        
        // ========================================================================
        // 立即数生成 (根据指令类型)
        // ========================================================================
        
        // I-type: imm[11:0] = instr[31:20], sign-extended to 32 bits
        ch_uint<32> imm_i = sext<32>(bits<31, 20>(io().instruction));
        
        // S-type: imm[11:0] = instr[31:25] << 5 | instr[11:7], sign-extended
        ch_uint<12> imm_s_base = (bits<31, 25>(io().instruction) << ch_uint<7>(5_d)) | bits<11, 7>(io().instruction);
        ch_uint<32> imm_s = sext<32>(imm_s_base);
        
        // B-type: imm[12|10:5|4:1|11] = instr[31|30:25|11:8|7], sign-extended
        ch_uint<13> imm_b_base = 
            (bits<31, 31>(io().instruction) << ch_uint<12>(12_d)) |  // imm[12]
            (bits<30, 25>(io().instruction) << ch_uint<6>(5_d)) |    // imm[10:5]
            (bits<11, 8>(io().instruction) << ch_uint<4>(1_d)) |     // imm[4:1]
            // imm[11]：instr[7] 承载 **imm[11]**（ADR-063 D3）⇒ MUST 左移 11
            // ⚠️ 此前漏移位 ⇒ 落在 bit 0 ⇒ |偏移| >= 2048 的条件分支目标错 2047。
            (bits<7, 7>(io().instruction) << ch_uint<4>(11_d));
        ch_uint<32> imm_b = sext<32>(imm_b_base);
        
        // U-type: imm[31:12] = instr[31:12]
        ch_uint<32> imm_u = io().instruction & ch_uint<32>(0xFFFFF000);
        
        // J-type: imm[20|10:1|11|19:12] = instr[31|30:21|20|19:12], sign-extended
        ch_uint<21> imm_j_base = 
            (bits<31, 31>(io().instruction) << ch_uint<20>(20_d)) |  // imm[20]
            (bits<30, 21>(io().instruction) << ch_uint<10>(1_d)) |   // imm[10:1]
            (bits<20, 20>(io().instruction) << ch_uint<11>(11_d)) |  // imm[11]
            (bits<19, 12>(io().instruction) << ch_uint<12>(12_d));   // imm[19:12]
        ch_uint<32> imm_j = sext<32>(imm_j_base);
        
        // 根据 opcode 选择立即数和指令类型
        imm_val = select(op == ch_uint<7>(OP_OPIMM),  imm_i, imm_val);
        imm_val = select(op == ch_uint<7>(OP_LOAD),   imm_i, imm_val);
        imm_val = select(op == ch_uint<7>(OP_JALR),   imm_i, imm_val);
        imm_val = select(op == ch_uint<7>(OP_STORE),  imm_s, imm_val);
        imm_val = select(op == ch_uint<7>(OP_BRANCH), imm_b, imm_val);
        imm_val = select(op == ch_uint<7>(OP_LUI),    imm_u, imm_val);
        imm_val = select(op == ch_uint<7>(OP_AUIPC),  imm_u, imm_val);
        imm_val = select(op == ch_uint<7>(OP_JAL),    imm_j, imm_val);
        
        type = select(op == ch_uint<7>(OP_OP),    ch_uint<3>(INSTR_R), type);
        type = select(op == ch_uint<7>(OP_OPIMM), ch_uint<3>(INSTR_I), type);
        type = select(op == ch_uint<7>(OP_LOAD),  ch_uint<3>(INSTR_I), type);
        type = select(op == ch_uint<7>(OP_JALR),  ch_uint<3>(INSTR_I), type);
        type = select(op == ch_uint<7>(OP_STORE), ch_uint<3>(INSTR_S), type);
        type = select(op == ch_uint<7>(OP_BRANCH), ch_uint<3>(INSTR_B), type);
        type = select(op == ch_uint<7>(OP_LUI),   ch_uint<3>(INSTR_U), type);
        type = select(op == ch_uint<7>(OP_AUIPC), ch_uint<3>(INSTR_U), type);
        type = select(op == ch_uint<7>(OP_JAL),   ch_uint<3>(INSTR_J), type);
        
        // ========================================================================
        // **保留组合**判定（ISA 的非法编码 ⇒ 非法指令）
        //
        // ⚠️⚠️ **关键事实（手推 · 实测踩过）**：**U 型（LUI / AUIPC）与 J 型（JAL）
        //    **没有 funct3 字段** —— bits 14:12 是**立即数位**
        //    （U 型 = `imm[14:12]` · J 型 = `imm[19:17]`）⇒ ⇒ 它们的**任何**取值都合法
        //    ⇒ ⇒ ⚠️ 把「funct3 MUST 为 000」套到 U/J 型上 ⇒ ⇒ **误判合法指令为非法**
        //    ⇒ ⇒ （实测：`lui x10, 0x1` = `0x00001537` 的 funct3 位 = 001 ⇒ **合法**）
        //
        // ⚠️ **R 型（OP）的 funct7**：只允许 `0x00`（ADD/SLL/SLT/SLTU/XOR/SRL/OR/AND）
        //    与 `0x20`（SUB / SRA）⇒ ⇒ 且 `0x20` **仅当** funct3 ∈ {000, 101}
        //    ⇒ ⇒ ⚠️ **funct7[5] 对 I 型是 `imm[10]`**（立即数位）⇒ ⇒ OP-IMM **不查 funct7**
        // ========================================================================
        auto f3_load   = (f3 == ch_uint<3>(0)) | (f3 == ch_uint<3>(1))
                       | (f3 == ch_uint<3>(2)) | (f3 == ch_uint<3>(4))
                       | (f3 == ch_uint<3>(5));
        auto f3_store  = (f3 == ch_uint<3>(0)) | (f3 == ch_uint<3>(1))
                       | (f3 == ch_uint<3>(2));
        auto f3_branch = (f3 == ch_uint<3>(0)) | (f3 == ch_uint<3>(1))
                       | (f3 == ch_uint<3>(4)) | (f3 == ch_uint<3>(5))
                       | (f3 == ch_uint<3>(6)) | (f3 == ch_uint<3>(7));
        auto f3_zero   = (f3 == ch_uint<3>(0));
        // R 型 funct7：`0x00` 全合法；`0x20` 仅当 funct3 ∈ {000, 101}
        auto f7_ok     = (f7 == ch_uint<7>(0))
                       | ((f7 == ch_uint<7>(0x20))
                          & ((f3 == ch_uint<3>(0)) | (f3 == ch_uint<3>(5))));
        auto reserved  = ((op == ch_uint<7>(OP_OP))      & !f7_ok)
                       | ((op == ch_uint<7>(OP_LOAD))    & !f3_load)
                       | ((op == ch_uint<7>(OP_STORE))   & !f3_store)
                       | ((op == ch_uint<7>(OP_BRANCH))  & !f3_branch)
                       | ((op == ch_uint<7>(OP_JALR))    & !f3_zero);
        // ⚠️ **OP-IMM / JAL / LUI / AUIPC 不在此表** ⇒ ⇒ 前者 `imm[10]` 是立即数位
        //    ⇒ 后两者**无 funct3 字段** ⇒ ⇒ 任何取值都合法
        
        // 已知 opcode 集合（**FENCE 已含** ⇒ 它是已知 = NOP ⇒ NOT 非法）
        // ⚠️ **`OP_SYSTEM` MUST NOT 在此表**（2026-11-25 · P6-2）：它按 `funct3`
        //    细分（见下）⇒ ⇒ 若整体归「合法」⇒ `funct3 = 0`（ECALL/…) 会掉到
        //    reason 0 ⇒ ⇒「合法但无功能」⇒ ⇒ **比冻结更糟**（会当 NOP 退休）。
        auto known_opcode = (op == ch_uint<7>(OP_OP)) | (op == ch_uint<7>(OP_OPIMM))
                          | (op == ch_uint<7>(OP_LOAD)) | (op == ch_uint<7>(OP_STORE))
                          | (op == ch_uint<7>(OP_BRANCH)) | (op == ch_uint<7>(OP_JALR))
                          | (op == ch_uint<7>(OP_JAL)) | (op == ch_uint<7>(OP_LUI))
                          | (op == ch_uint<7>(OP_AUIPC)) | (op == ch_uint<7>(OP_FENCE));
        // ========================================================================
        // `OP_SYSTEM` 的 `funct3` 分类（**2026-11-25 · P6-2 · change
        // `rv32i-csr-instructions-2026-11`**）
        //
        // | funct3 | 指令 | 本项处置 | reason |
        // |---|---|---|---|
        // | 001/010/011 | CSRRW/S/C | **实现** | 0 |
        // | 101/110/111 | CSRRWI/SI/CI | **实现** | 0 |
        // | 000 | ECALL/EBREAK/MRET/WFI | 未实现（P6-4/P6-5） | **1** |
        // | 100 | **保留** | — | **3** |
        //
        // ⚠️ 优先级仍是 `1 > 3 > 2 > 0`（ADR-071 v1.2）⇒ ⇒ `OP_SYSTEM` 的
        //    判定仍插在**最前**。
        // ========================================================================
        auto is_op_system     = (op == ch_uint<7>(OP_SYSTEM));
        auto csr_legal_f3     = (f3 == ch_uint<3>(1)) | (f3 == ch_uint<3>(2))
                              | (f3 == ch_uint<3>(3)) | (f3 == ch_uint<3>(5))
                              | (f3 == ch_uint<3>(6)) | (f3 == ch_uint<3>(7));
        // `funct3 = 0`（ECALL/EBREAK/MRET/WFI）⇒ 仍「已知未实现」
        auto sys_known_unimpl = is_op_system & !csr_legal_f3 & (f3 != ch_uint<3>(4));
        // `funct3 = 4` ⇒ ISA 保留组合
        auto sys_reserved     = is_op_system & (f3 == ch_uint<3>(4));
        // 「合法」= 既有已知 opcode ∪ **合法 `funct3` 的 CSR 指令**
        auto legal_opcode     = known_opcode | (is_op_system & csr_legal_f3);
        
        // ========================================================================
        // 输出
        // ========================================================================
        io().opcode = op;
        io().rd = dest;
        io().funct3 = f3;
        io().rs1 = src1;
        io().rs2 = src2;
        io().funct7 = f7;
        io().imm = imm_val;
        io().instr_type = type;
        // ⚠️ **MUST 是最后一个 `io()` 赋值**（2026-11-16）：端口节点按**赋值顺序**编号
        //    ⇒ 插在中间会让既有 `decoder_unnamed_output_N` **整体位移**
        //    ⇒ ⇒ 独立 Verilog TB 的端口表**全部错位**（实测踩过 · 见 id_stage 的同型记录）。
        //
        // reason 码：0 合法 · 1 已知未实现（`OP_SYSTEM` 的 `funct3=0`）·
        // 2 未知 opcode · 3 保留组合（含 `OP_SYSTEM` 的 `funct3=4`）
        // ⚠️ 优先级 `1 > 3 > 2 > 0`（ADR-071 v1.2）
        io().illegal_reason =
            select(sys_known_unimpl, ch_uint<2>(1),
            select(sys_reserved, ch_uint<2>(3),
            select(reserved, ch_uint<2>(3),
            select(legal_opcode, ch_uint<2>(0), ch_uint<2>(2)))));
    }
};

} // namespace riscv
