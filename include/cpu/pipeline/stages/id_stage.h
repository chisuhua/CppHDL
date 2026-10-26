/**
 * @file id_stage.h
 * @brief RISC-V 5 级流水线 - 译码级 (Instruction Decode)
 * 
 * 功能:
 * - 指令译码 (R/I/S/B/U/J 型)
 * - 寄存器文件读 (2 个读端口)
 * - 符号/零扩展
 * - 控制信号生成
 * 
 * 作者：DevMate
 * 最后修改：2026-04-09
 */

#pragma once

#include "ch.hpp"
#include "component.h"
#include "rv32i_decoder.h"

namespace riscv {

using namespace ch::core;

/**
 * @brief ID 级 (译码级)
 * 
 * 从 IF 级接收指令，译码后传递给 EX 级
 */
class IdStage : public ch::Component {
public:
    __io(
        // 输入 (来自 IF 级)
        ch_in<ch_uint<32>>  pc;             // IF/ID PC (用于分支目标计算)
        ch_in<ch_uint<32>>  instruction;    // 32 位指令
        ch_in<ch_bool>      valid;          // 指令有效
        // ⚠️ **flush / stall 输入**（2026-10-25，change rv32i-pipeline-commit-path-2026-10）：
        //    ID/EX 寄存器 MUST 能在「控制冒险」与「数据冒险」下**冻结/清空**：
        //    - `flush`：EX 级判定 taken 分支 ⇒ **错误路径**的指令（在 ID 级）
        //      MUST NOT 前进到 EX（否则它会**执行并写回寄存器堆**
        //      —— 实测：被冲刷的 `addi x6,x0,0x7f` 写回了 x6=0x7f）。
        //    - `stall`：load-use 冒险 ⇒ ID 级的消费者 MUST **留在 ID**
        //      （等待 load 的数据写回）⇒ ID/EX 冻结。
        //    此前两者**都没有** ⇒ 冲刷不生效、停顿「停了个寂寞」。
        ch_in<ch_bool>      flush;          // 控制冒险冲刷（错误路径）
        ch_in<ch_bool>      stall;          // 数据冒险停顿
        
        // 输入 (来自流水线级 reg_file 的读端口)
        ch_in<ch_uint<32>>  reg_rs1_data;   // RS1 读数据 (来自 reg_file)
        ch_in<ch_uint<32>>  reg_rs2_data;   // RS2 读数据 (来自 reg_file)
        
        // 输出 (到 EX 级)
        ch_out<ch_uint<5>>  rs1_addr;       // RS1 地址
        ch_out<ch_uint<5>>  rs2_addr;       // RS2 地址
        ch_out<ch_uint<32>> rs1_data;       // RS1 数据
        ch_out<ch_uint<32>> rs2_data;       // RS2 数据
        ch_out<ch_uint<32>> imm;            // 立即数 (符号扩展)
        ch_out<ch_uint<7>>  opcode;         // 操作码 (FIX: was 4bit, RISC-V opcodes are 7 bits)
        ch_out<ch_uint<3>>  funct3;         // funct3 字段
        ch_out<ch_bool>     is_branch;      // 分支指令
        ch_out<ch_bool>     is_load;        // 加载指令
        ch_out<ch_bool>     is_store;       // 存储指令
        ch_out<ch_bool>     is_jump;        // 跳转指令
        ch_out<ch_bool>     is_alu;         // ALU 指令
        ch_out<ch_uint<5>>  rd_addr;        // 目的寄存器地址
        ch_out<ch_bool>     valid_out;      // 寄存后的 valid 信号 (到 EX)
        // ⚠️ **ID/EX 寄存后的 pc**（2026-10-25 补）：
        //    EX 级需要「**它正在处理的那条指令**的 pc」来算分支目标
        //    （`branch_target = pc + imm`）。
        //    此前 `id_stage` **没有 pc 输出** ⇒ `ex_stage.io().pc` 只能接
        //    **IF/ID 的 pc 输入**（那是**当前 ID 级**那条指令的 pc，且随 ID
        //    逐拍前进）⇒ EX 拿到的 pc 比它处理的指令**晚一个字**
        //    （实测：BEQ 目标偏 1 个字 ⇒ 跳到 word 31 而非 word 30）。
        ch_out<ch_uint<32>> pc_out;         // 寄存后的 pc (到 EX)
        // ⚠️ **当前 ID 级那条指令**的 rs1/rs2 地址（**组合**，未过 ID/EX 寄存器）
        //    （2026-10-25 补）：寄存器堆的读地址 MUST 用这两个 ——
        //    `io().rs1_addr` / `io().rs2_addr` 是**寄存后**的值（属于**上一条**
        //    指令，ID/EX 寄存器在上一拍才锁存）⇒ 用它们读寄存器堆会读到
        //    **上一条指令**的寄存器内容（实测：`add x3,x1,x2` 读到 0，
        //    因为 x1/x2 是在两条指令之前写的）。
        ch_out<ch_uint<5>>  rs1_addr_id;     // 当前 ID 级的 rs1 地址
        ch_out<ch_uint<5>>  rs2_addr_id;     // 当前 ID 级的 rs2 地址
        // ID/EX 寄存后的 funct7（funct7[5] 区分 SUB / SRA）
        ch_out<ch_uint<7>>  funct7_out;
    )
    
    IdStage(ch::Component* parent = nullptr, const std::string& name = "id_stage")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // ========================================================================
        // 指令字段提取 (组合逻辑)
        // ========================================================================
        // 从指令中提取各个字段
        ch_uint<7>  opcode_raw(0_d, "opcode_raw");
        ch_uint<5>  rd(0_d, "rd");
        ch_uint<3>  funct3(0_d, "funct3");
        ch_uint<5>  rs1(0_d, "rs1");
        ch_uint<5>  rs2(0_d, "rs2");
        ch_uint<7>  funct7(0_d, "funct7");
        
        // 提取固定位字段
        opcode_raw = bits<6, 0>(io().instruction);
        rd         = bits<11, 7>(io().instruction);
        funct3     = bits<14, 12>(io().instruction);
        rs1        = bits<19, 15>(io().instruction);
        rs2        = bits<24, 20>(io().instruction);
        funct7     = bits<31, 25>(io().instruction);
        
        // ========================================================================
        // 指令类型检测 (组合逻辑)
        // ========================================================================
        // 检测指令类型 (opcode 比较)
        auto is_op    = (opcode_raw == ch_uint<7>(OP_OP));
        auto is_opimm = (opcode_raw == ch_uint<7>(OP_OPIMM));
        auto is_load  = (opcode_raw == ch_uint<7>(OP_LOAD));
        auto is_store = (opcode_raw == ch_uint<7>(OP_STORE));
        auto is_branch= (opcode_raw == ch_uint<7>(OP_BRANCH));
        auto is_jalr  = (opcode_raw == ch_uint<7>(OP_JALR));
        auto is_jal   = (opcode_raw == ch_uint<7>(OP_JAL));
        auto is_lui   = (opcode_raw == ch_uint<7>(OP_LUI));
        auto is_auipc = (opcode_raw == ch_uint<7>(OP_AUIPC));
        
        // ========================================================================
        // 立即数生成 (组合逻辑)
        // ========================================================================
        // I-type 立即数：instr[31:20] 符号扩展到 32 位
        ch_uint<32> imm_i = sext<32>(bits<31, 20>(io().instruction));
        
        // S-type 立即数：instr[31:25] << 5 | instr[11:7], 符号扩展
        ch_uint<12> imm_s_base =
            (bits<31, 25>(io().instruction) << ch_uint<3>(5_d)) |
            bits<11, 7>(io().instruction);
        ch_uint<32> imm_s = sext<32>(imm_s_base);

        // B-type 立即数：instr[31|30:25|11:8|7], 符号扩展
        ch_uint<13> imm_b_base =
            (bits<31, 31>(io().instruction) << ch_uint<5>(12_d)) |  // 5 bits can hold 0-31
            (bits<30, 25>(io().instruction) << ch_uint<3>(5_d)) |   // 3 bits can hold 0-7
            (bits<11, 8>(io().instruction) << ch_uint<1>(1_d)) |    // 1 bit can hold 0-1
            bits<7, 7>(io().instruction);
        ch_uint<32> imm_b = sext<32>(imm_b_base);

        // U-type 立即数：imm[31:12] = instr[31:12]（**低 12 位 MUST 清零**）
        //
        // ⚠️ **2026-10-26 修正（既有缺陷）**：原实现是
        //      `instr_masked = instruction & ch_uint<32>(1048575_d)`（= 0x000FFFFF，
        //      **20 位**掩码 ⇒ 把 **rd / opcode 位也算进了立即数**）
        //      `imm_u = instr_masked << 12`
        //    ⇒ 实测 `auipc x18, 0`（机器码 `0x00000917`）得到 `pc + 0x917000`
        //      （0x917 = `rd<<7 | opcode` 左移 12）——**U 型立即数一直是错的**。
        //    此前无固件 / 无测试覆盖 LUI / AUIPC（唯二消费者是仓外的 riscv-tests
        //    二进制，CI 全 SKIP）⇒ 缺陷从未暴露。
        //    ⇒ 掩码 MUST 是 `instr[31:12]`（0xFFFFF000）且**不再左移**。
        //    ⚠️ 十进制字面量（本仓惯例：`0x..._d` 会被 `lit_dec` 拒绝，见
        //    `mem_stage.h:120/140` 的同型写法）。
        ch_uint<32> instr_masked = io().instruction & ch_uint<32>(4294963200_d);
        ch_uint<32> imm_u = instr_masked;

        // J-type 立即数：instr[31|30:21|20|19:12], 符号扩展
        ch_uint<21> imm_j_base =
            (bits<31, 31>(io().instruction) << ch_uint<5>(20_d)) |  // 5 bits can hold 0-31
            (bits<30, 21>(io().instruction) << ch_uint<1>(1_d)) |   // 1 bit can hold 0-1
            (bits<20, 20>(io().instruction) << ch_uint<4>(11_d)) |  // 4 bits can hold 0-15
            (bits<19, 12>(io().instruction) << ch_uint<5>(12_d));   // 5 bits can hold 0-31
        ch_uint<32> imm_j = sext<32>(imm_j_base);
        
        // 根据指令类型选择立即数
        ch_uint<32> selected_imm(0_d, "selected_imm");
        selected_imm = select(is_opimm,   imm_i, selected_imm);
        selected_imm = select(is_load,    imm_i, selected_imm);
        selected_imm = select(is_jalr,    imm_i, selected_imm);
        selected_imm = select(is_store,   imm_s, selected_imm);
        selected_imm = select(is_branch,  imm_b, selected_imm);
        selected_imm = select(is_lui,     imm_u, selected_imm);
        selected_imm = select(is_auipc,   imm_u, selected_imm);
        selected_imm = select(is_jal,     imm_j, selected_imm);
        
        // ========================================================================
        // 寄存器地址选择 (组合逻辑)
        // ========================================================================
        // 读端口 A - rs1 地址
        auto rs1_addr_mux = select(
            is_op | is_opimm | is_load | is_store | is_branch | is_jalr,
            rs1,
            ch_uint<5>(0_d)
        );
        
        // 读端口 B - rs2 地址
        auto rs2_addr_mux = select(
            is_op | is_store | is_branch,
            rs2,
            ch_uint<5>(0_d)
        );
        
        // ========================================================================
        // 控制信号生成 (组合逻辑)
        // ========================================================================
        // 分支指令标志
        ch_bool branch_flag = is_branch;
        
        // 加载指令标志
        ch_bool load_flag = is_load;
        
        // 存储指令标志
        ch_bool store_flag = is_store;
        
        // 跳转指令标志
        ch_bool jump_flag = is_jal | is_jalr;
        
        // ALU 指令标志 (R-type, I-type, LUI, AUIPC)
        ch_bool alu_flag = is_op | is_opimm | is_lui | is_auipc;
        
        // ========================================================================
        // ID/EX 流水线寄存器
        // ========================================================================
        // 存储译码结果，传递给 EX 级
        ch_reg<ch_uint<5>>  idex_rs1_addr(0_d, "idex_rs1_addr");
        ch_reg<ch_uint<5>>  idex_rs2_addr(0_d, "idex_rs2_addr");
        ch_reg<ch_uint<32>> idex_pc(0_d, "idex_pc");
        ch_reg<ch_uint<7>> idex_funct7(0_d, "idex_funct7");
        ch_reg<ch_uint<32>> idex_rs1_data(0_d, "idex_rs1_data");
        ch_reg<ch_uint<32>> idex_rs2_data(0_d, "idex_rs2_data");
        ch_reg<ch_uint<32>> idex_imm(0_d, "idex_imm");
        ch_reg<ch_uint<7>>  idex_opcode(0_d, "idex_opcode");
        ch_reg<ch_uint<3>>  idex_funct3(0_d, "idex_funct3");
        ch_reg<ch_bool>     idex_is_branch(false, "idex_is_branch");
        ch_reg<ch_bool>     idex_is_load(false, "idex_is_load");
        ch_reg<ch_bool>     idex_is_store(false, "idex_is_store");
        ch_reg<ch_bool>     idex_is_jump(false, "idex_is_jump");
        ch_reg<ch_bool>     idex_is_alu(false, "idex_is_alu");
        ch_reg<ch_uint<5>>  idex_rd_addr(0_d, "idex_rd_addr");
        ch_reg<ch_bool>     idex_valid(false, "idex_valid");
        
        // ID/EX 寄存器更新使能
        // 当输入有效且有效信号稳定时更新
        // ⚠️ **ID/EX 在 stall 时 MUST 装「气泡」而不是「冻结」**（2026-10-25 改）：
        //    冻结（保持旧值）会让 EX 级持有**消费者**的数据，而 EX/MEM 装的是
        //    **load** 的数据 ⇒ 有效位与数据**错开一级** ⇒ 被阻塞的指令最终
        //    仍会被写回（实测：被 taken 分支冲刷的 `addi x6,x0,0x7f` 写回了 0x7f）。
        //    ⇒ stall 时 valid 置 0（气泡）；**数据字段仍会刷新**（无害：
        //    valid=0 ⇒ 该指令在任何一级都不产生副作用）。
        //    flush ⇒ 同样装气泡（错误路径不进 EX）。
        auto idex_update_en = io().valid;
        
        // 更新寄存器
        idex_rs1_addr->next = select(idex_update_en, rs1_addr_mux, idex_rs1_addr);
        idex_rs2_addr->next = select(idex_update_en, rs2_addr_mux, idex_rs2_addr);
        idex_pc->next       = select(idex_update_en, io().pc, idex_pc);
        idex_funct7->next   = select(idex_update_en, funct7, idex_funct7);
        idex_rs1_data->next = select(idex_update_en, io().reg_rs1_data, idex_rs1_data);
        idex_rs2_data->next = select(idex_update_en, io().reg_rs2_data, idex_rs2_data);
        idex_imm->next      = select(idex_update_en, selected_imm, idex_imm);
        idex_opcode->next   = select(idex_update_en, opcode_raw, idex_opcode);
        idex_funct3->next   = select(idex_update_en, funct3, idex_funct3);
        // ⚠️ **气泡时 MUST 清零控制信号**（2026-10-25 补）：
        //    气泡（stall / flush）若**残留**上一条指令的属性 ⇒
        //      ① `is_load` 残留 ⇒ `hazard.ex_is_load` 误判 load-use
        //         ⇒ 无谓的停顿（甚至自锁）；
        //      ② `is_branch` 残留 ⇒ `hazard.ex_branch` 误判控制冒险
        //         ⇒ 误触发 flush（自锁式冲刷）；
        //      ③ `is_store` / `is_jump` / `is_alu` 残留 ⇒ 同类误判；
        //      ④ `rd_addr` 残留 ⇒ 与 ID 级消费者的地址**误匹配**。
        //    ⇒ 气泡时把它们清零（其余数据字段保持无害：valid=0）。
        auto idex_bubble = io().flush | io().stall;
        idex_is_branch->next= select(idex_bubble, ch_bool(false),
                                     select(idex_update_en, branch_flag, idex_is_branch));
        idex_is_load->next  = select(idex_bubble, ch_bool(false),
                                     select(idex_update_en, load_flag, idex_is_load));
        idex_is_store->next = select(idex_bubble, ch_bool(false),
                                     select(idex_update_en, store_flag, idex_is_store));
        idex_is_jump->next  = select(idex_bubble, ch_bool(false),
                                     select(idex_update_en, jump_flag, idex_is_jump));
        idex_is_alu->next   = select(idex_bubble, ch_bool(false),
                                     select(idex_update_en, alu_flag, idex_is_alu));
        idex_rd_addr->next  = select(idex_bubble, ch_uint<5>(0_d),
                                     select(idex_update_en, rd, idex_rd_addr));
        // ⚠️ **气泡（flush 或 stall）时 valid MUST 被清零**（2026-10-25 改）：
        //    - flush：错误路径的指令 MUST NOT 前进到 EX 并写回
        //      （实测：被 taken 分支冲刷的 `addi x6,x0,0x7f` 写回了 0x7f）。
        //    - stall：消费者 MUST **留在 ID 级**（不进 EX）⇒ 装气泡。
        //      （此前是「冻结」⇒ 有效位与数据错开一级 ⇒ 仍被写回。）
        idex_valid->next    = select(io().flush | io().stall, ch_bool(false),
                                     select(idex_update_en, io().valid, idex_valid));
        
        // ========================================================================
        // 输出到 EX 级
        // ========================================================================
        io().rs1_addr  = idex_rs1_addr;
        io().rs2_addr  = idex_rs2_addr;
        io().rs1_data  = idex_rs1_data;
        io().rs2_data  = idex_rs2_data;
        io().imm       = idex_imm;
        io().opcode    = idex_opcode;
        io().funct3    = idex_funct3;
        io().is_branch = idex_is_branch;
        io().is_load   = idex_is_load;
        io().is_store  = idex_is_store;
        io().is_jump   = idex_is_jump;
        io().is_alu    = idex_is_alu;
        io().rd_addr   = idex_rd_addr;
        io().valid_out = idex_valid;
        io().pc_out    = idex_pc;
        io().rs1_addr_id = rs1_addr_mux;
        io().rs2_addr_id = rs2_addr_mux;
        io().funct7_out  = idex_funct7;
    }
};

} // namespace riscv
