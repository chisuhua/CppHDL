/**
 * @file rv32i_pipeline.h
 * @brief RISC-V RV32I 5 级流水线顶层集成
 * 
 * 流水线结构:
 * ┌─────┬─────┬─────┬─────┐
 * │ IF  │ ID  │ EX  │ MEM │ WB │
 * ├─────┼─────┼─────┼─────┼────┤
 * │PC+4 │Decode│ALU  │Mem  │Write│
 * │IF/ID│ID/EX│EX/MEM│MEM/WB│back│
 * └─────┴─────┴─────┴─────┴────┘
 * 
 * 功能:
 * - 5 级流水线：取指 (IF) → 译码 (ID) → 执行 (EX) → 访存 (MEM) → 写回 (WB)
 * - 数据前推：EX→EX, MEM→EX, WB→EX
 * - Load-Use 冒险检测与停顿
 * - 分支预测错误冲刷
 */

#pragma once

#include "ch.hpp"
#include "component.h"
#include "stages/if_stage.h"
#include "stages/id_stage.h"
#include "stages/ex_stage.h"
#include "stages/mem_stage.h"
#include "stages/wb_stage.h"
#include "hazard_unit.h"
#include "rv32i_pipeline_regs.h"

namespace riscv {

using namespace ch::core;

class Rv32iPipeline : public ch::Component {
public:
    __io(
        ch_in<ch_bool>      rst;
        ch_in<ch_bool>      clk;

        ch_out<ch_uint<32>> instr_addr;
        ch_in<ch_uint<32>>  instr_data;
        ch_in<ch_bool>      instr_ready;

        ch_out<ch_uint<32>> data_addr;
        ch_out<ch_uint<32>> data_write_data;
        ch_out<ch_uint<4>>  data_strbe;
        ch_out<ch_bool>     data_write_en;
        ch_in<ch_uint<32>>  data_read_data;
        ch_in<ch_bool>      data_ready;

        ch_out<ch_uint<48>> perf_cycles;
        ch_out<ch_uint<48>> perf_instructions;
        ch_out<ch_uint<32>> perf_branch_count;
        ch_out<ch_uint<32>> perf_branch_mispredict;
        ch_out<ch_uint<32>> perf_load_use_stalls;
        ch_out<ch_uint<32>> perf_control_stalls;

        // ---- 诊断输出（dbg_*）----
        // ⚠️ 这些**不是**功能接口，是为了让「指令流 / 有效链 / 分支判定」
        //    可被外部（含 verilator）观测而存在的诊断面。
        //    存在的理由：`perf_instructions` 等计数器**不足以**区分
        //    「没取到指令」/「取到但没解码」/「解码了但没退休」三种状态。
        //    诊断面让逐拍追踪能定位到具体哪一级断了。
        ch_out<ch_uint<32>> dbg_id_instr;      // ID 级正在译码的指令字
        ch_out<ch_uint<32>> dbg_id_pc;         // ID 级 PC
        ch_out<ch_bool>     dbg_id_valid;      // ID 级有效
        // ⚠️⚠️ **命名有误导**（2026-10-25，第 16 轮 critic SHOULD_FIX #1）：
        //    这三个信号接的是 `id_stage.io().is_*` = **ID/EX 寄存后**的值
        //    ⇒ 它们描述的是「**EX 级**那条指令」的类型，**不是**「ID 级正在
        //    译码的那条」。
        //    （同组的 `dbg_id_instr` / `dbg_id_pc` / `dbg_id_valid` 才是真 ID 级
        //    —— 它们接 `id_stage` 的**输入**端口。）
        //    ⇒ **不改名**（端口集合已被护栏 C6 快照契约化），但注释 MUST 如实。
        ch_out<ch_bool>     dbg_id_is_branch;  // ⚠️ **EX 级**那条指令：是不是分支
        ch_out<ch_bool>     dbg_id_is_load;    // ⚠️ **EX 级**那条指令：是不是 load
        ch_out<ch_bool>     dbg_id_is_store;   // ⚠️ **EX 级**那条指令：是不是 store
        ch_out<ch_bool>     dbg_branch_taken;  // EX 级：分支是否成立
        ch_out<ch_bool>     dbg_exmem_valid;   // EX/MEM 有效寄存器
        ch_out<ch_bool>     dbg_exmem_is_store;// EX/MEM 的 store 标记
        ch_out<ch_bool>     dbg_mem_valid_out; // MEM 级输出给 WB 的有效位
        // ⚠️ 「**这一拍**检测到 load-use 冒险」的脉冲（2026-10-25，A1-a）：
        //    `perf_load_use_stalls` 计的是「**停顿拍数**」（保持型停顿 ⇒
        //    一个冒险持续 N 拍就 +N）；而「**冒险次数**」需要单独的单拍脉冲
        //    ⇒ capability `rv32i-commit-path` 要求两者**可区分**。
        ch_out<ch_bool>     dbg_load_use_detected;
        // 临时诊断（2026-10-25）：WB 写回通路
        ch_out<ch_bool>     dbg_wb_write_en;
        ch_out<ch_uint<32>> dbg_wb_write_data;
        ch_out<ch_uint<5>>  dbg_wb_rd_addr;
        ch_out<ch_uint<32>> dbg_exmem_alu_result;
        ch_out<ch_uint<32>> dbg_exmem_imm;
        ch_out<ch_uint<32>> dbg_exmem_rs2_data;
    )
    
    Rv32iPipeline(ch::Component* parent = nullptr, const std::string& name = "rv32i_pipeline")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    // Phase 5: describe() split into 3 helper methods (datapath, control_signals,
    // forwarding_paths). All pipeline stage instances and ch_reg<> state live as
    // member fields so the helpers can be plain void methods.

    // Stage instances + reg file + pipeline regs (all shared state)
    ch::ch_module<IfStage>  if_stage{"if_stage"};
    ch::ch_module<IdStage>  id_stage{"id_stage"};
    ch::ch_module<ExStage>  ex_stage{"ex_stage"};
    ch::ch_module<MemStage> mem_stage{"mem_stage"};
    ch::ch_module<WbStage>  wb_stage{"wb_stage"};
    ch::ch_module<HazardUnit> hazard{"hazard_unit"};

    ch_mem<ch_uint<32>, 32> reg_file{"reg_file"};

    ch_reg<ch_bool>     exmem_is_store{false, "exmem_is_store"};
    ch_reg<ch_uint<32>> exmem_alu_result{0_d, "exmem_alu_result"};
    // ⚠️ **返回地址通路**（2026-10-26 新增）：JAL/JALR 的写回值是 `pc + 4`，
    //    而 WB 级没有 `pc` 通路 ⇒ MUST 在 **EX 级**算好并逐级锁存过来。
    ch_reg<ch_uint<32>> exmem_link_value{0_d, "exmem_link_value"};
    ch_reg<ch_uint<32>> exmem_rs2_data{0_d, "exmem_rs2_data"};
    ch_reg<ch_uint<5>>  exmem_rd_addr{0_d, "exmem_rd_addr"};
    ch_reg<ch_uint<3>>  exmem_funct3{0_d, "exmem_funct3"};
    ch_reg<ch_bool>     exmem_is_load{false, "exmem_is_load"};
    ch_reg<ch_uint<32>> exmem_imm{0_d, "exmem_imm"};
    ch_reg<ch_bool>     exmem_valid{false, "exmem_valid"};
    // ⚠️ EX/MEM 的「指令类型」寄存器（2026-10-25 补）：
    //    MEM/WB 寄存器需要知道「WB 级那条指令是 ALU / jump / load」，
    //    而 MEM 级那条指令的类型只能从 EX/MEM 寄存器取 ——
    //    直接用 `id_stage.io().is_alu`（**EX 级**的视图）会**串一级**：
    //    MEM/WB 会在 T+1 装上「T 拍在 EX 级那条指令」的类型。
    ch_reg<ch_bool>     exmem_is_alu{false, "exmem_is_alu"};
    ch_reg<ch_bool>     exmem_is_jump{false, "exmem_is_jump"};

    ch_reg<ch_bool>     memwb_is_alu{false, "memwb_is_alu"};
    // ⚠️ MEM/WB 补齐（2026-10-25）：WB 需要的**写回数据**与 **is_load**
    //    必须与 WB 级那条指令同拍 ⇒ 必须过 MEM/WB 寄存器。
    //    此前 WB 直接组合直连 MEM（`wb_stage.io().alu_result <<= mem_stage...`）
    //    ⇒ 写回的是**上一条**指令的 ALU 结果 / 内存数据。
    ch_reg<ch_bool>     memwb_is_load{false, "memwb_is_load"};
    ch_reg<ch_uint<32>> memwb_alu_result{0_d, "memwb_alu_result"};
    ch_reg<ch_uint<32>> memwb_link_value{0_d, "memwb_link_value"};
    ch_reg<ch_uint<32>> memwb_mem_data{0_d, "memwb_mem_data"};
    ch_reg<ch_bool>     memwb_is_jump{false, "memwb_is_jump"};
    ch_reg<ch_uint<5>>  memwb_rd_addr{0_d, "memwb_rd_addr"};
    ch_reg<ch_bool>     memwb_valid{false, "memwb_valid"};

    ch_reg<ch_uint<48>> cycle_count{0_d, "cycle_count"};
    ch_reg<ch_uint<48>> commit_count{0_d, "commit_count"};
    ch_reg<ch_uint<32>> branch_count{0_d, "branch_count"};
    ch_reg<ch_uint<32>> branch_mispredict{0_d, "branch_mispredict"};
    ch_reg<ch_uint<32>> load_use_stalls{0_d, "load_use_stalls"};
    ch_reg<ch_uint<32>> control_stalls{0_d, "control_stalls"};

    // Datapath: data flow wiring between the 5 pipeline stages + reg file + memory
    void build_datapath() {
        // Reg file read ports
        //
        // ⚠️ MUST 用 **aread（0-cycle 组合读）**，不能用 sread（1-cycle）：
        //    ID 级的 `idex_rs1_data` / `idex_rs2_data` 寄存器**已经**提供了
        //    「读地址 → 读数据」的那 1 拍延迟（地址在 ID 拍组合给出，数据在
        //    ID/EX 寄存器里与指令同拍进 EX）。
        //    若这里再用 sread（再晚 1 拍）⇒ EX 拿到的操作数属于**上一条**指令
        //    （实测：store 的写数据恒为 0 —— 读到的是上一条 NOP 的 rs2=x0）。
        //    ⚠️ 与 imem 的约定**不冲突**：ADR-041 强制的是 **imem** 用 sread
        //    （取指地址要 1 拍），寄存器堆不是 imem。
        //    ⚠️ 读地址 MUST 用 **当前 ID 级**的地址（`rs1_addr_id` / `rs2_addr_id`），
        //    不得用寄存后的 `rs1_addr` / `rs2_addr`（那是**上一条**指令的地址）
        auto rs1_data_result = reg_file.aread(id_stage.io().rs1_addr_id,
                                               "reg_file_rs1");
        auto rs2_data_result = reg_file.aread(id_stage.io().rs2_addr_id,
                                               "reg_file_rs2");
        id_stage.io().reg_rs1_data <<= rs1_data_result;
        id_stage.io().reg_rs2_data <<= rs2_data_result;

        // WB writes back to reg file
        auto wb_write_en = wb_stage.io().write_en & (wb_stage.io().rd_addr_out != ch_uint<5>(0_d));
        reg_file.write(wb_stage.io().rd_addr_out, wb_stage.io().write_data, wb_write_en, "reg_file_write");

        // IF stage connections
        if_stage.io().branch_target <<= ex_stage.io().branch_target;
        if_stage.io().branch_valid <<= ex_stage.io().branch_taken;
        io().instr_addr <<= if_stage.io().instr_addr;
        if_stage.io().instr_data <<= io().instr_data;
        if_stage.io().instr_ready <<= io().instr_ready;

        // IF -> ID
        id_stage.io().pc <<= if_stage.io().pc;
        id_stage.io().instruction <<= if_stage.io().instruction;
        id_stage.io().valid <<= if_stage.io().valid;
        // ⚠️ 控制/数据冒险的反馈必须进 ID 级：
        //    - `flush`（taken 分支）⇒ ID/EX 清空（错误路径不进 EX）
        //    - `stall`（load-use）⇒ ID/EX 冻结（消费者留在 ID）
        id_stage.io().flush <<= hazard.io().flush;
        id_stage.io().stall <<= hazard.io().stall;

        // ID -> EX
        ex_stage.io().rs1_data <<= id_stage.io().rs1_data;
        ex_stage.io().rs2_data <<= id_stage.io().rs2_data;
        ex_stage.io().imm <<= id_stage.io().imm;
        ex_stage.io().funct3 <<= id_stage.io().funct3;
        // ⚠️ **分支条件类型（funct3）MUST 接进 EX**（2026-10-25 实测修复）：
        //    `ex_stage.io().branch_type` 此前**从未被驱动** ⇒ 恒为 0 ⇒
        //    **所有分支都按 BEQ 判定**（实测：`bne`（x0 != x0 恒假）被当成
        //    BEQ ⇒ 被判为「成立」⇒ 程序跳进了错误路径）。
        ex_stage.io().branch_type <<= id_stage.io().funct3;
        ex_stage.io().is_branch <<= id_stage.io().is_branch;
        ex_stage.io().is_store <<= id_stage.io().is_store;
        // ⚠️ EX 级必须拿**它自己那条指令**的 pc（ID/EX 寄存后的），
        //    而不是 IF/ID 的 pc 输入（那是当前 ID 级那条指令的 pc）
        ex_stage.io().pc <<= id_stage.io().pc_out;
        ex_stage.io().funct7 <<= id_stage.io().funct7_out;

        // ⚠️ **ALU 操作码 MUST 由 funct3 / funct7 推导并接线**（2026-10-25 实测修复）：
        //    `ex_stage.io().alu_op` 此前**从未被驱动**（全仓无连接）⇒ 恒为 0
        //    ⇒ 所有 ALU 操作都算 ADD（实测：`sub x4,x3,x2` 得 16 = 13+3，
        //    而非 13-3 = 10）。
        //    映射（RV32I）：funct3 000=ADD/SUB · 001=SLL · 010=SLT · 011=SLTU ·
        //    100=XOR · 101=SRL/SRA · 110=OR · 111=AND；funct7[5] 区分 SUB / SRA。
        {
            auto alu_f3 = id_stage.io().funct3;
            // ⚠️⚠️ **`funct7[5]`（= `instr[30]`）只对 R 型 与 SRAI 有意义**
            //    （2026-10-26 实测修复）：
            //      对 **I 型的 `ADDI` / `ANDI` / …** 而言，`instr[30]` 是 **`imm[10]`**
            //      的一部分 ⇒ 直接用它判 SUB/SRA 会把 **`imm >= 0x400`** 或
            //      负数误判成 SUB：
            //        `addi x21, x0, -8`（`0xFF800A93`）⇒ `0 - (-8)` = **8**
            //        `addi x21, x0, 0x7FF`（`0x7FF00A93`）⇒ `0 - 0x7FF` = **0xFFFFF801**
            //      ⇒ 本固件原来的 imm 全部 < 0x400（`0x100` / `0x122`）⇒ 侥幸未暴露。
            //    ⇒ MUST 用「**R 型** 或 **`OP_OPIMM` 且 funct3=101（SRAI）**」门控。
            auto alu_is_r_type = (id_stage.io().opcode == ch_uint<7>(OP_OP));
            auto alu_is_srai = (id_stage.io().opcode == ch_uint<7>(OP_OPIMM)) &
                               (alu_f3 == ch_uint<3>(5_d));
            auto alu_is_sub_sra = bits<5, 5>(id_stage.io().funct7_out) &
                                  (alu_is_r_type | alu_is_srai);
            // ⚠️ **访存指令的 funct3 编码的是「访存宽度」而不是 ALU 操作**
            //    （S/L 型：000=byte 001=half 010=word）⇒ LOAD/STORE 的
            //    ALU 结果必须是 **ADD**（rs1 + imm = 访存地址）。
            //    （实测：按 funct3 直接映射 ⇒ `sw` 被当成 SLT ⇒ 地址算成
            //    「0 < 10」= 1。）
            auto alu_is_mem = (id_stage.io().opcode == ch_uint<7>(OP_LOAD)) |
                              (id_stage.io().opcode == ch_uint<7>(OP_STORE));
            ch_uint<4> alu_op_calc = ch_uint<4>(ALU_OP_ADD);
            alu_op_calc = select(alu_f3 == ch_uint<3>(7_d),
                                 ch_uint<4>(ALU_OP_AND), alu_op_calc);
            alu_op_calc = select(alu_f3 == ch_uint<3>(6_d),
                                 ch_uint<4>(ALU_OP_OR), alu_op_calc);
            alu_op_calc = select(alu_f3 == ch_uint<3>(4_d),
                                 ch_uint<4>(ALU_OP_XOR), alu_op_calc);
            alu_op_calc = select(alu_f3 == ch_uint<3>(3_d),
                                 ch_uint<4>(ALU_OP_SLTU), alu_op_calc);
            alu_op_calc = select(alu_f3 == ch_uint<3>(2_d),
                                 ch_uint<4>(ALU_OP_SLT), alu_op_calc);
            alu_op_calc = select(alu_f3 == ch_uint<3>(1_d),
                                 ch_uint<4>(ALU_OP_SLL), alu_op_calc);
            alu_op_calc = select(alu_f3 == ch_uint<3>(5_d),
                                 select(alu_is_sub_sra, ch_uint<4>(ALU_OP_SRA),
                                        ch_uint<4>(ALU_OP_SRL)),
                                 alu_op_calc);
            alu_op_calc = select(alu_f3 == ch_uint<3>(0_d),
                                 select(alu_is_sub_sra, ch_uint<4>(ALU_OP_SUB),
                                        ch_uint<4>(ALU_OP_ADD)),
                                 alu_op_calc);
            alu_op_calc = select(alu_is_mem, ch_uint<4>(ALU_OP_ADD), alu_op_calc);
            ex_stage.io().alu_op <<= alu_op_calc;
        }
        ex_stage.io().opcode <<= id_stage.io().opcode;

        // EX -> EX/MEM pipeline regs
        //
        // ⚠️⚠️ **EX/MEM 的装载使能 MUST 与 ID/EX 的装载使能一致**
        //    （`id_stage` 内部：`idex_update_en = io().valid`（**不含** `!stall`），
        //      而气泡（stall / flush）由 `idex_valid` 的**清零分支**承担 ⇒
        //      ID/EX 在气泡拍装的是「valid=0 的气泡」。）。
        //    否则会出现「**ID/EX 被气泡冻结**（数据停在旧指令）而
        //    **EX/MEM 照常装载**（有效位前进）」⇒ **有效位与数据错开一拍**
        //    ⇒ 被冲刷/停顿的**陈旧指令**最终被写回寄存器堆
        //    （实测：被 taken 分支冲刷的 `addi x6,x0,0x7f` 写回了 x6=0x7f）。
        //    ⇒ 这里对每个 EX/MEM 寄存器都用 `select(exmem_en, 新值, 保持)`。
        // ⚠️⚠️ **EX/MEM 的装载使能 MUST NOT 因停顿而冻结**（2026-10-25 改）：
        //    停顿被触发的当拍，「处于 EX 级的那条指令」**正是**那条 load
        //    ⇒ 若 EX/MEM 此时冻结，**load 永远进不了 MEM 级** ⇒ 它的数据
        //    永远到不了 WB ⇒ **停顿永不解除 ⇒ 死锁**。
        //    ⇒ 停顿的效果是「ID/EX 装气泡」（消费者留在 ID 级），
        //       而 EX/MEM **照常装载 EX 级的结果**（load 正常前进）。
        //    ⚠️ `id_stage.io().valid_out` 在 stall/flush 的**边沿**被清零
        //       ⇒ 下一拍 `exmem_en` 自动为 0 ⇒ EX/MEM 装气泡 ✔
        // ⚠️⚠️ **MUST NOT 因 `flush` 而清零**（2026-10-25，第 15 轮 critic
        //    SHOULD_FIX #8）：flush 由 **EX 级**的 `branch_taken` 产生 ⇒
        //    flush 那一拍**正在 EX 级的那条指令就是分支本身** ⇒ 若此时
        //    EX/MEM 装气泡 ⇒ **taken 分支从不退休**（实测：退休数里没有
        //    `beq`）⇒ 与 capability R1「计数器 MUST 对 ALU / load / store /
        //    branch **一视同仁**」有张力。
        //    ⇒ 教科书语义：flush 只杀「**比分支年轻**」的指令 ⇒
        //       分支本身 MUST 正常退休。
        //    ⇒ 错误路径那条（在 ID 级）由 `id_stage` 的 flush 通路拦住
        //       （ID/EX 装气泡）⇒ 不进 EX ⇒ 不会被写回 ✔
        auto exmem_en = id_stage.io().valid_out;
        // ⚠️ **气泡时 EX/MEM 的「控制信号」MUST 清零**（不能保持旧值）：
        //    `mem_stage` 的访存使能是 `do_mem_access = is_load | is_store`
        //    —— **它不看 `mem_valid`** ⇒ 若气泡拍残留了上一条 store 的
        //    `is_store`，MEM 级会**真的写一次内存**（地址是旧值）⇒
        //    **静默的内存误写**。
        //    ⇒ `is_store` / `is_load` 在气泡时清零；其余数据字段保持
        //       （valid=0 ⇒ 不会被写回，故无害）。
        exmem_is_store <<= select(exmem_en, ex_stage.io().is_store_alu,
                                  ch_bool(false));
        exmem_alu_result <<= select(exmem_en, ex_stage.io().alu_result,
                                    exmem_alu_result);
        // 返回地址（JAL/JALR 的写回值）—— 与 ALU 结果**同拍**锁存
        exmem_link_value <<= select(exmem_en, ex_stage.io().link_value,
                                    exmem_link_value);
        exmem_rs2_data <<= select(exmem_en, ex_stage.io().rs2_data,
                                  exmem_rs2_data);
        exmem_rd_addr <<= select(exmem_en, id_stage.io().rd_addr,
                                 exmem_rd_addr);
        exmem_funct3 <<= select(exmem_en, id_stage.io().funct3, exmem_funct3);
        exmem_is_load <<= select(exmem_en, id_stage.io().is_load,
                                 ch_bool(false));
        exmem_imm <<= select(exmem_en, id_stage.io().imm, exmem_imm);
        // ⚠️ 气泡时 EX/MEM 的 valid MUST **清零**（而不是保持）：
        //    数据保持 + valid 保持 ⇒ 同一条指令在 MEM 级**停留两拍**
        //    ⇒ MEM/WB 抓两次 ⇒ WB **写回两次**（实测：x1 连续两拍各写回一次）。
        //    ⇒ 气泡以「valid=0」的形式**逐级传播**（EX/MEM → MEM/WB → WB）。
        // ⚠️⚠️ **EX/MEM 的 valid MUST 取 `valid_out`（ID/EX 寄存后的）**
        //    （2026-10-25 补）：此前取的是 `id_stage.io().valid`（**IF/ID 的
        //    valid** = **ID 级**那条指令的有效性）⇒ 与 `exmem_en`（用
        //    `valid_out` = **EX 级**那条指令的有效性）**错位一级**
        //    ⇒ EX/MEM 的「有效位」与「数据」可能来自**不同的指令**
        //    ⇒ 同一条指令在 MEM 级**停留两拍** ⇒ MEM/WB 抓两次
        //    ⇒ WB **写回两次**（实测：x1 在连续两拍各写回一次 0x0a）。
        exmem_valid <<= select(exmem_en, id_stage.io().valid_out, ch_bool(false));
        // EX 级的指令类型（`id_stage.io().*` 是 **ID/EX 寄存后**的值
        //  ⇒ 正是 EX 级看到的指令类型）⇒ 进 EX/MEM 后成为 MEM 级的类型
        exmem_is_alu <<= select(exmem_en, id_stage.io().is_alu, ch_bool(false));
        exmem_is_jump <<= select(exmem_en, id_stage.io().is_jump, ch_bool(false));

        // EX/MEM -> MEM
        mem_stage.io().alu_result <<= exmem_alu_result;
        mem_stage.io().rs2_data <<= exmem_rs2_data;
        mem_stage.io().rd_addr <<= exmem_rd_addr;
        mem_stage.io().is_store <<= exmem_is_store;
        mem_stage.io().is_load <<= exmem_is_load;
        mem_stage.io().funct3 <<= exmem_funct3;
        mem_stage.io().mem_valid <<= exmem_valid;
        // ⚠️ MEM 级的**读数据**必须接进来（此前 MEM 级没有这个端口 ⇒ load 恒读回 0）
        mem_stage.io().mem_read_data <<= io().data_read_data;

        // MEM/WB pipeline regs
        //
        // ⚠️ 全部取自 **MEM 级**（`exmem_*` / `mem_stage.io().*`），
        //    **不得**取自 `id_stage.io().*`（那是 EX 级的视图 ⇒ 串一级）。
        // ⚠️ 使能 MUST 用 `exmem_valid`（EX/MEM 寄存器）而**不是**
        //    `mem_stage.io().mem_valid`（MEM 级的**输入端口**）：
        //    ⚠️ 框架禁止的是「**子组件的输入端口 ← 同一个子组件的输入端口**」
        //       （`child_in <<= child_in` 的 driver 必须是**当前模块**的端口）
        //       ⇒ 父组件**读**子组件的输入端口会触发运行时 CHERROR
        //       （⚠️ 但「父输出 ← 子输入」是**允许**的 —— `build_debug_outputs()`
        //       里的 `dbg_id_instr` / `dbg_id_pc` / `dbg_id_valid` 就是这种形态）。
        //    两者是同一个值（`mem_stage.io().mem_valid <<= exmem_valid`）。
        memwb_is_alu->next     = select(exmem_valid, exmem_is_alu, memwb_is_alu);
        memwb_is_jump->next    = select(exmem_valid, exmem_is_jump, memwb_is_jump);
        memwb_is_load->next    = select(exmem_valid, exmem_is_load, memwb_is_load);
        memwb_rd_addr->next    = select(exmem_valid, mem_stage.io().rd_addr_out, memwb_rd_addr);
        memwb_alu_result->next = select(exmem_valid, mem_stage.io().alu_result_out, memwb_alu_result);
        // ⚠️ 返回地址**不过 MEM 级**（MEM 不碰它）⇒ 直接从 EX/MEM 寄存器抓
        memwb_link_value->next = select(exmem_valid, exmem_link_value, memwb_link_value);
        memwb_mem_data->next   = select(exmem_valid, mem_stage.io().mem_data, memwb_mem_data);
        // MEM/WB 的 valid = MEM 级的 valid（气泡以 valid=0 逐级传播 ⇒
        // 数据保持、valid 清零 ⇒ WB 不会把陈旧指令再写回一次）
        memwb_valid->next      = mem_stage.io().mem_valid_out;

        // MEM/WB -> WB stage（WB 只能看到 MEM/WB 寄存器 ⇒ 与 WB 级指令同拍）
        wb_stage.io().alu_result <<= memwb_alu_result;
        wb_stage.io().link_value <<= memwb_link_value;
        wb_stage.io().rd_addr <<= memwb_rd_addr;
        wb_stage.io().mem_data <<= memwb_mem_data;
        wb_stage.io().is_load <<= memwb_is_load;
        wb_stage.io().is_alu <<= memwb_is_alu;
        wb_stage.io().is_jump <<= memwb_is_jump;
        wb_stage.io().mem_valid <<= memwb_valid;

        // Data memory interface
        io().data_addr <<= exmem_alu_result;
        // ⚠️ **SB/SH 的写数据 MUST「lane 对齐」**（2026-10-26 新增）：
        //    RISC-V 规定 store 的源数据**低位对齐**，由**总线**按 `addr[1:0]`
        //    选车道 ⇒ `sb` 到偏移 1 时，源字节 MUST 落在 **lane 1**
        //    （即 `rs2 << 8`）⇒ 否则「掩码对了、数据在错 lane」——
        //    那是一个比「字节写没生效」**更隐蔽**的错误。
        //
        //    ⚠️ **只对 SB/SH 位移**（funct3 = 000 / 001）：
        //    `sw` MUST 保持原样（**含非对齐 `sw` 的既有行为**：整字写到
        //    所在字地址）⇒ 这里的改动**不改变**任何既有 `sw` 行为。
        //    ⚠️ 移位量 MUST 用**运行期节点** ⇒ 用 `shl<N>`（显式结果宽度）；
        //    `operator<<` 会按 RHS 的**类型宽度**推算结果宽度 ⇒ 运行期移位量
        //    会把结果宽度算错（见 `include/axi4/byte_lane.h` 的同型警告）。
        auto store_byte_offset = bits<1, 0>(exmem_alu_result);
        // SB/SH = funct3 000/001；SW = 010 ⇒ `!= 010` 即「部分写」
        auto store_is_partial = exmem_is_store &
                                (exmem_funct3 != ch_uint<3>(2_d));
        auto lane_shift = select(!store_is_partial, ch_uint<5>(0_d),
                          select(store_byte_offset == ch_uint<2>(0_d), ch_uint<5>(0_d),
                          select(store_byte_offset == ch_uint<2>(1_d), ch_uint<5>(8_d),
                          select(store_byte_offset == ch_uint<2>(2_d), ch_uint<5>(16_d),
                                                 ch_uint<5>(24_d)))));
        io().data_write_data <<= shl<32>(exmem_rs2_data, lane_shift);
        // ⚠️ 字节使能 MUST 来自 **MEM 级**（2026-10-25，第 14 轮 critic
        //    SHOULD_FIX #5/#6）：此前**恒接 0xF** ⇒ SB/SH 的字节使能
        //    **从未生效**（mem_stage 里算好的 `store_byte_enable` 是死代码）。
        //    ⇒ 改接 MEM 级的输出（SW 仍是 0xF ⇒ 整字写行为不变）。
        io().data_strbe <<= mem_stage.io().store_byte_enable_out;
        io().data_write_en <<= exmem_is_store;
    }

    // Control signals: hazard unit + reset/stall/flush to all stages
    void build_control_signals() {
        // IF stage stall/flush from hazard
        if_stage.io().stall <<= hazard.io().stall;
        if_stage.io().flush <<= hazard.io().flush;
        if_stage.io().rst <<= io().rst;

        // Hazard unit inputs
        // ⚠️ MUST 用**当前 ID 级**的地址（`rs1_addr_id` / `rs2_addr_id`），
        //    不得用寄存后的 `id_stage.io().rs1_addr`（那是 **EX 级**的视图）：
        //    load-use 检测要比较的是「**正在 ID 级**的消费者」的 rs1/rs2 与
        //    「MEM 级的 load」的 rd ⇒ 用 EX 级的地址会**晚一级**触发
        //    （此时消费者的操作数**已经锁存**）⇒ 停顿「停了个寂寞」
        //    （实测：`lw` 紧邻 `add` 时，停顿计数 +1 但读到的仍是旧值）。
        hazard.io().id_rs1_addr <<= id_stage.io().rs1_addr_id;
        hazard.io().id_rs2_addr <<= id_stage.io().rs2_addr_id;
        // ⚠️ **冒险单元的「MEM 级」输入 MUST 取自 MEM 级**（EX/MEM 寄存器），
        //    不得取自 `id_stage.io().*`（那是 EX 级的视图）：
        //    - `mem_rd_addr` 原接 `id_stage.rd_addr` ⇒ load-use 检测拿「EX 级
        //      指令的 rd」当「MEM 级 load 的 rd」⇒ 检测退化成「与自身比较」；
        //    - `mem_is_load` 原接 `id_stage.is_load` ⇒ 同上。
        //    （`ex_rd_addr` / `ex_reg_write` 接 `id_stage.*` 是**对的** ——
        //      `id_stage.io().*` 是 ID/EX 寄存后的值 = EX 级的视图。）
        hazard.io().ex_rd_addr <<= id_stage.io().rd_addr;
        hazard.io().mem_rd_addr <<= exmem_rd_addr;
        hazard.io().wb_rd_addr <<= wb_stage.io().rd_addr_out;
        hazard.io().ex_reg_write <<= id_stage.io().is_alu | id_stage.io().is_load |
                                    id_stage.io().is_jump;
        hazard.io().mem_reg_write <<= exmem_is_alu | exmem_is_load | exmem_is_jump;
        hazard.io().wb_reg_write <<= wb_stage.io().write_en;
        // ⚠️ **load-use 检测 MUST 走 EX 级**（`ex_is_load`）：
        //    5 级流水线的标准检测点是「EX 级的 load vs ID 级的消费者」。
        //    `mem_is_load`（MEM 级那一路）**故意不连接**（保持 0）：
        //    它晚一级 —— MEM 级 load 到达时消费者早已进 EX、操作数已锁存
        //    ⇒ 停顿「停了个寂寞」；而且在流水线里它会**多停一拍**且是
        //    **不必要**的（消费者进 EX 时那条 load 已在 WB 写回）。
        //    `mem_is_load` 只给「独立使用 HazardUnit」的场景用。
        hazard.io().ex_is_load <<= id_stage.io().is_load;
        // ⚠️ `ex_branch` = **条件分支 + 跳转**（2026-10-26 扩展）：
        //    `JAL`/`JALR` 是**无条件**跳 ⇒ 也必须 flush 错误路径
        //    （否则 PC 跳了、错误路径的指令却继续流下去 ⇒ 效果泄漏）。
        //    ⚠️ 两者**互斥**（一条指令不可能同时是 OP_BRANCH 与 OP_JAL/OP_JALR）
        //    ⇒ `if_stage` 的隐式不变量（flush 的唯一来源是
        //    `ex_branch & ex_branch_taken`，且 `branch_valid` 接同一个
        //    `ex_stage.branch_taken`）**结构不变**。
        hazard.io().ex_branch <<= id_stage.io().is_branch | id_stage.io().is_jump;
        hazard.io().ex_branch_taken <<= ex_stage.io().branch_taken;
        hazard.io().ex_alu_result <<= ex_stage.io().alu_result;
        // ⚠️ MUST 用 `exmem_alu_result`（EX/MEM 寄存器）而**不是**
        //    `mem_stage.io().alu_result`（MEM 级的**输入端口**）：
        //    框架禁止「**子输入 ← 同一个子的子输入**」（`child_in <<= child_in`）
        //    ⇒ 父组件**读**子组件的输入端口会触发运行时 CHERROR
        //    （⚠️「父输出 ← 子输入」是**允许**的）
        //    （`in <<= in` 的 driver 必须是当前模块的端口）⇒ 运行时 CHERROR。
        //    两者是同一个值（`mem_stage.io().alu_result <<= exmem_alu_result;`）。
        hazard.io().mem_alu_result <<= exmem_alu_result;
        hazard.io().wb_write_data <<= wb_stage.io().write_data;
    }

    // Forwarding paths: data forwarding + hazard detection + perf counters
    void build_forwarding_paths() {
        // Raw register values from ID (forwarded to hazard unit)
        hazard.io().rs1_data_raw <<= id_stage.io().rs1_data;
        hazard.io().rs2_data_raw <<= id_stage.io().rs2_data;

        // Forwarding MUX outputs override RS1/RS2 on EX inputs
        ex_stage.io().rs1_data <<= hazard.io().rs1_data;
        ex_stage.io().rs2_data <<= hazard.io().rs2_data;

        // Performance counters
        auto reset_active = io().rst;
        cycle_count->next = select(reset_active, ch_uint<48>(0_d), cycle_count + 1_d);

        auto commit_valid = mem_stage.io().mem_valid_out;
        commit_count->next = select(reset_active, ch_uint<48>(0_d), commit_count + commit_valid);

        auto branch_decoded = id_stage.io().is_branch;
        branch_count->next = select(reset_active, ch_uint<32>(0_d), branch_count + branch_decoded);

        // ⚠️ **控制重定向**（2026-10-26）：`ex_branch` 已扩为「条件分支 **+** 跳转」
        //    ⇒ `branch_taken & flush` 现在同时包含 `JAL`/`JALR` 的重定向。
        auto control_redirect =
            ex_stage.io().branch_taken & if_stage.io().valid & hazard.io().flush;
        // ⚠️ **误预测**只统计**条件分支**（`JAL`/`JALR` 是**无条件**跳 ⇒ 不是
        //    「预测错误」）⇒ MUST 用 `id_stage.io().is_branch` 门控
        //    （它是 **EX 级**那条指令的类型，与 `ex_stage.branch_taken` 同拍）。
        //    ⚠️ 不加这个门控 ⇒ 每执行一条 `JAL`/`JALR` 都会被记一次「误预测」
        //    ⇒ 性能计数器语义被污染（实测会多出 2 次）。
        auto branch_mispred_detected = id_stage.io().is_branch & control_redirect;
        branch_mispredict->next = select(reset_active, ch_uint<32>(0_d), branch_mispredict + branch_mispred_detected);

        load_use_stalls->next = select(reset_active, ch_uint<32>(0_d), load_use_stalls + hazard.io().stall);
        // ⚠️ `control_stalls` 统计**全部**控制重定向造成的停顿拍数
        //    （含 `JAL`/`JALR` —— 它们同样让流水线空 3 拍）⇒ 与「误预测」**分离**
        //    （此前两者同源，只因固件里只有条件分支）。
        control_stalls->next = select(reset_active, ch_uint<32>(0_d), control_stalls + control_redirect);

        io().perf_cycles <<= cycle_count;
        io().perf_instructions <<= commit_count;
        io().perf_branch_count <<= branch_count;
        io().perf_branch_mispredict <<= branch_mispredict;
        io().perf_load_use_stalls <<= load_use_stalls;
        io().perf_control_stalls <<= control_stalls;
    }

    // 诊断面：把「指令流 / 有效链 / 分支判定」逐级引出
    void build_debug_outputs() {
        io().dbg_id_instr <<= id_stage.io().instruction;
        io().dbg_id_pc <<= id_stage.io().pc;
        io().dbg_id_valid <<= id_stage.io().valid;
        io().dbg_id_is_branch <<= id_stage.io().is_branch;
        io().dbg_id_is_load <<= id_stage.io().is_load;
        io().dbg_id_is_store <<= id_stage.io().is_store;
        io().dbg_branch_taken <<= ex_stage.io().branch_taken;
        io().dbg_exmem_valid <<= exmem_valid;
        io().dbg_exmem_is_store <<= exmem_is_store;
        io().dbg_mem_valid_out <<= mem_stage.io().mem_valid_out;
        io().dbg_load_use_detected <<= hazard.io().load_use_detected;
        io().dbg_wb_write_en <<= wb_stage.io().write_en;
        io().dbg_wb_write_data <<= wb_stage.io().write_data;
        io().dbg_wb_rd_addr <<= wb_stage.io().rd_addr_out;
        io().dbg_exmem_alu_result <<= exmem_alu_result;
        io().dbg_exmem_imm <<= exmem_imm;
        io().dbg_exmem_rs2_data <<= exmem_rs2_data;
    }

    void describe() override {
        build_datapath();
        build_control_signals();
        build_forwarding_paths();
        build_debug_outputs();
    }
};

} // namespace riscv
