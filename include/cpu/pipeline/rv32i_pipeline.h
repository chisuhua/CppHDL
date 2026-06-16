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
        ch_out<ch_uint<32>> perf_control_stalls
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
    ch_reg<ch_uint<32>> exmem_rs2_data{0_d, "exmem_rs2_data"};
    ch_reg<ch_uint<5>>  exmem_rd_addr{0_d, "exmem_rd_addr"};
    ch_reg<ch_uint<3>>  exmem_funct3{0_d, "exmem_funct3"};
    ch_reg<ch_bool>     exmem_is_load{false, "exmem_is_load"};
    ch_reg<ch_uint<32>> exmem_imm{0_d, "exmem_imm"};
    ch_reg<ch_bool>     exmem_valid{false, "exmem_valid"};

    ch_reg<ch_bool>     memwb_is_alu{false, "memwb_is_alu"};
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
        // Reg file read ports (combinational)
        auto rs1_data_result = reg_file.sread(id_stage.io().rs1_addr, ch_bool(true));
        auto rs2_data_result = reg_file.sread(id_stage.io().rs2_addr, ch_bool(true));
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

        // ID -> EX
        ex_stage.io().rs1_data <<= id_stage.io().rs1_data;
        ex_stage.io().rs2_data <<= id_stage.io().rs2_data;
        ex_stage.io().imm <<= id_stage.io().imm;
        ex_stage.io().funct3 <<= id_stage.io().funct3;
        ex_stage.io().is_branch <<= id_stage.io().is_branch;
        ex_stage.io().is_store <<= id_stage.io().is_store;
        ex_stage.io().pc <<= id_stage.io().pc;
        ex_stage.io().opcode <<= id_stage.io().opcode;

        // EX -> EX/MEM pipeline regs
        exmem_is_store <<= ex_stage.io().is_store_alu;
        exmem_alu_result <<= ex_stage.io().alu_result;
        exmem_rs2_data <<= ex_stage.io().rs2_data;
        exmem_rd_addr <<= id_stage.io().rd_addr;
        exmem_funct3 <<= id_stage.io().funct3;
        exmem_is_load <<= id_stage.io().is_load;
        exmem_imm <<= id_stage.io().imm;
        exmem_valid <<= id_stage.io().valid;

        // EX/MEM -> MEM
        mem_stage.io().alu_result <<= exmem_alu_result;
        mem_stage.io().rs2_data <<= exmem_rs2_data;
        mem_stage.io().rd_addr <<= exmem_rd_addr;
        mem_stage.io().is_store <<= exmem_is_store;
        mem_stage.io().is_load <<= exmem_is_load;
        mem_stage.io().funct3 <<= exmem_funct3;
        mem_stage.io().mem_valid <<= exmem_valid;

        // MEM -> WB
        wb_stage.io().alu_result <<= mem_stage.io().alu_result_out;
        wb_stage.io().rd_addr <<= mem_stage.io().rd_addr_out;
        wb_stage.io().mem_data <<= mem_stage.io().mem_data;
        wb_stage.io().is_load <<= mem_stage.io().is_load;
        wb_stage.io().mem_valid <<= mem_stage.io().mem_valid_out;

        // MEM/WB pipeline regs
        memwb_is_alu->next = select(mem_stage.io().mem_valid, id_stage.io().is_alu, memwb_is_alu);
        memwb_is_jump->next = select(mem_stage.io().mem_valid, id_stage.io().is_jump, memwb_is_jump);
        memwb_rd_addr->next = select(mem_stage.io().mem_valid, mem_stage.io().rd_addr_out, memwb_rd_addr);
        memwb_valid->next = mem_stage.io().mem_valid_out;

        // MEM/WB -> WB stage
        wb_stage.io().is_alu <<= memwb_is_alu;
        wb_stage.io().is_jump <<= memwb_is_jump;
        wb_stage.io().rd_addr <<= memwb_rd_addr;
        wb_stage.io().mem_valid <<= memwb_valid;

        // Data memory interface
        io().data_addr <<= exmem_alu_result;
        io().data_write_data <<= exmem_rs2_data;
        io().data_strbe <<= ch_uint<4>(15_d);
        io().data_write_en <<= exmem_is_store;
    }

    // Control signals: hazard unit + reset/stall/flush to all stages
    void build_control_signals() {
        // IF stage stall/flush from hazard
        if_stage.io().stall <<= hazard.io().stall;
        if_stage.io().flush <<= hazard.io().flush;
        if_stage.io().rst <<= io().rst;

        // Hazard unit inputs
        hazard.io().id_rs1_addr <<= id_stage.io().rs1_addr;
        hazard.io().id_rs2_addr <<= id_stage.io().rs2_addr;
        hazard.io().ex_rd_addr <<= id_stage.io().rd_addr;
        hazard.io().mem_rd_addr <<= id_stage.io().rd_addr;
        hazard.io().wb_rd_addr <<= wb_stage.io().rd_addr_out;
        hazard.io().ex_reg_write <<= id_stage.io().is_alu;
        hazard.io().mem_reg_write <<= id_stage.io().is_load;
        hazard.io().wb_reg_write <<= wb_stage.io().write_en;
        hazard.io().mem_is_load <<= id_stage.io().is_load;
        hazard.io().ex_branch <<= id_stage.io().is_branch;
        hazard.io().ex_branch_taken <<= ex_stage.io().branch_taken;
        hazard.io().ex_alu_result <<= ex_stage.io().alu_result;
        hazard.io().mem_alu_result <<= mem_stage.io().alu_result;
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

        auto branch_mispred_detected = ex_stage.io().branch_taken & if_stage.io().valid & hazard.io().flush;
        branch_mispredict->next = select(reset_active, ch_uint<32>(0_d), branch_mispredict + branch_mispred_detected);

        load_use_stalls->next = select(reset_active, ch_uint<32>(0_d), load_use_stalls + hazard.io().stall);
        control_stalls->next = select(reset_active, ch_uint<32>(0_d), control_stalls + branch_mispred_detected);

        io().perf_cycles <<= cycle_count;
        io().perf_instructions <<= commit_count;
        io().perf_branch_count <<= branch_count;
        io().perf_branch_mispredict <<= branch_mispredict;
        io().perf_load_use_stalls <<= load_use_stalls;
        io().perf_control_stalls <<= control_stalls;
    }

    void describe() override {
        build_datapath();
        build_control_signals();
        build_forwarding_paths();
    }
};

} // namespace riscv
