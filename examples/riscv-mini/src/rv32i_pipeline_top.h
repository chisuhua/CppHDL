// rv32i_pipeline_top.h
//
// RV32I SoC 顶层（pipeline + I-TCM + D-TCM）。
//
// ⚠️ 端口契约（ADR-046 D10 / D10-rev，2026-10-25 C3
//    `rv32i-soc-dual-parity-2026-10`）：
//    - 观测 MUST 走顶层 `ch_out`（verilator 只能读顶层端口）；
//    - 固件 MUST 能经 `prog_*` 写进 I-TCM（此前本顶层根本没接 TCM 写端口
//      ⇒ imem 恒 0 ⇒ 只能跑 NOP）；
//    - 端口集合由 `scripts/verify_no_duplicate_rv32i_headers.sh` 的 C6 快照钉住。
//
// ⚠️ TCM 规模 `ADDR_WIDTH = 12`（4096 字 = 16 KB）：
//    `1 << 20` 字的 `ch_mem` 在 verilator 下是 4 MB 展开数组，对 parity 固件
//    毫无收益。代价 = **地址高位被截断**（`pc` / 数据地址只看低 12 位）
//    ⇒ 固件与数据 MUST 落在 < 0x1000。
//    IF 阶段 `pc_reg` 复位为 0x80000000（`stages/if_stage.h`），其低 12 位为 0
//    ⇒ 从字 0 开始取指，按 +4 递增。
//
// ⚠️ 固件 MUST 无数据冒险：lib 版 `HazardUnit` 不接数据前推
//    （`pipeline/hazard_unit.h` 的 `io().rs1_data = io().rs1_data_raw`，
//    接上会在真实装配里形成组合环 —— ADR-046 D3 #14；D5 对此仍悬置）。
#pragma once

#include "ch.hpp"
#include "component.h"
#include "cpu/pipeline/rv32i_pipeline.h"
#include "cpu/pipeline/rv32i_tcm.h"

using namespace ch::core;
using namespace riscv;

class Rv32iPipelineTop : public ch::Component {
public:
    static constexpr unsigned kTcmAddrWidth = 12;

    __io(
        // 时钟 / 复位
        ch_in<ch_bool>          ext_clk;
        ch_in<ch_bool>          ext_rst;

        // 固件加载（写 I-TCM；加载期保持 ext_rst = 1，计数器不会被污染）
        ch_in<ch_bool>          prog_we;
        // 固件写地址（**字地址**，32 位）—— 接到 12 位 TCM 时高位被截断
        // （与 pc / 数据地址同一约束：地址必须 < 0x1000）
        ch_in<ch_uint<32>>      prog_addr;
        ch_in<ch_uint<32>>      prog_data;

        // 观测输出（verilator 可读；ADR-035 R3：位宽 MUST <= 64）
        ch_out<ch_uint<32>>     obs_instr_addr;
        ch_out<ch_uint<32>>     obs_data_addr;
        ch_out<ch_uint<32>>     obs_data_write_data;
        ch_out<ch_bool>         obs_data_write_en;
        ch_out<ch_uint<48>>     obs_perf_cycles;
        ch_out<ch_uint<48>>     obs_perf_instructions;
        ch_out<ch_uint<32>>     obs_perf_branch_count;
        ch_out<ch_uint<32>>     obs_perf_branch_mispredict;
        ch_out<ch_uint<32>>     obs_perf_load_use_stalls;
        ch_out<ch_uint<32>>     obs_perf_control_stalls;

        // 诊断输出（指令流 / 分支判定 / 有效链的逐级可观测性）
        ch_out<ch_uint<32>>     dbg_id_instr;
        ch_out<ch_uint<32>>     dbg_id_pc;
        ch_out<ch_bool>         dbg_id_valid;
        // ⚠️ 这三个是「**EX 级**那条指令」的类型（接 `id_stage` 的 **ID/EX
        //    寄存后**的输出），**不是**「ID 级正在译码的那条」——
        //    （同组的 `dbg_id_instr` / `dbg_id_pc` / `dbg_id_valid` 才是真 ID 级）
        ch_out<ch_bool>         dbg_id_is_branch;
        ch_out<ch_bool>         dbg_id_is_load;
        ch_out<ch_bool>         dbg_id_is_store;
        ch_out<ch_bool>         dbg_branch_taken;
        ch_out<ch_bool>         dbg_wb_write_en;
        ch_out<ch_uint<32>>     dbg_wb_write_data;
        ch_out<ch_uint<5>>      dbg_wb_rd_addr;
        ch_out<ch_uint<32>>     dbg_exmem_alu_result;
        ch_out<ch_uint<32>>     dbg_exmem_imm;
        ch_out<ch_uint<32>>     dbg_exmem_rs2_data;
        ch_out<ch_uint<32>>     obs_data_rdata;   // D-TCM 读数据（aread，0-cycle）
        // 有效链的逐级观测（EX/MEM → MEM/WB → WB）
        ch_out<ch_bool>         dbg_exmem_valid;
        ch_out<ch_bool>         dbg_mem_valid_out;
        // load-use 冒险检测脉冲（与「停顿拍数」区分）
        ch_out<ch_bool>         dbg_load_use_detected;
    )

    Rv32iPipelineTop(ch::Component* parent = nullptr,
                     const std::string& name = "rv32i_pipeline_top")
        : ch::Component(parent, name) {}

    void create_ports() override {
        new (io_storage_) io_type;
    }

    void describe() override {
        ch::ch_module<Rv32iPipeline> pipeline{"pipeline"};
        ch::ch_module<InstrTCM<kTcmAddrWidth, 32>> itcm{"itcm"};
        ch::ch_module<DataTCM<kTcmAddrWidth, 32>> dtcm{"dtcm"};

        // ---- I-TCM：取指 + 固件写 ----
        //
        // ⚠️ **地址单位**：`InstrTCM` / `DataTCM` 内部的 `ch_mem` 是按
        //    **字索引**寻址的（`memory.sread(io().addr, …)`），而流水线的
        //    `instr_addr` / `data_addr` 是**字节地址**（pc = 0x80000000 + 4k）。
        //    ⇒ 必须右移 2 位取字索引，否则取指**快 4 倍**（实测：ID 级解码的
        //    字比 PC 快 4 个字 ⇒ 分支在错误的 PC 上被判定 ⇒ 程序跑飞）。
        //    既有 fixture `tests/test_verilator_complex_e2e.cpp` 用
        //    `bits<3, 2>(cpu.io().instr_addr)` 正是这个修正。
        //    ⚠️ 位宽 MUST 正确：`bits<MSB,LSB>` 是**闭区间** ⇒ 宽度 =
        //    MSB−LSB+1 ⇒ 字索引需要 **kTcmAddrWidth 位**（12 位 ⇒ 4096 个
        //    字索引）⇒ 切片 MUST 是 `bits<kTcmAddrWidth+1, 2]`（13 位 ⇒ 低 12 位）
        //    —— 此前写成 `+2`（14 位）⇒ **多一位**（off-by-one），只因 bit 13
        //    恒 0 且端口静默截断才没暴露（第 15 轮 critic SHOULD_FIX #2）。
        itcm.io().addr <<= bits<kTcmAddrWidth + 1, 2>(pipeline.io().instr_addr);
        itcm.io().write_en <<= io().prog_we;
        itcm.io().write_addr <<= io().prog_addr;
        itcm.io().write_data <<= io().prog_data;
        pipeline.io().instr_data <<= itcm.io().data;
        pipeline.io().instr_ready <<= itcm.io().ready;

        // ---- D-TCM：load / store ----
        // ⚠️ 同上：数据地址也是字节地址 ⇒ TCM 需要字索引
        dtcm.io().addr <<= bits<kTcmAddrWidth + 1, 2>(pipeline.io().data_addr);
        dtcm.io().wdata <<= pipeline.io().data_write_data;
        dtcm.io().write <<= pipeline.io().data_write_en;
        dtcm.io().valid <<= ch_bool(true);
        // ⚠️ **字节使能 MUST 接到 D-TCM**（2026-10-26 新增）：
        //    此前 `data_strbe`（MEM 级算好的车道掩码）**全仓无消费者**
        //    ⇒ `sb` / `sh` 在 SoC 里**完全不生效**（整字覆盖）。
        //    ⇒ 现在接线 ⇒ D-TCM 按 `strbe` 做读-改-写。
        //    ⚠️ 约定：`wdata` 已由流水线**lane 对齐**（`rv32i_pipeline.h`），
        //    `addr` 在此已右移 2 位 ⇒ 是**字地址**。
        dtcm.io().strbe <<= pipeline.io().data_strbe;
        pipeline.io().data_read_data <<= dtcm.io().rdata;
        pipeline.io().data_ready <<= dtcm.io().ready;

        // ---- 时钟 / 复位 ----
        pipeline.io().rst <<= io().ext_rst;
        pipeline.io().clk <<= io().ext_clk;

        // ---- 观测输出 ----
        io().obs_instr_addr <<= pipeline.io().instr_addr;
        io().obs_data_addr <<= pipeline.io().data_addr;
        io().obs_data_write_data <<= pipeline.io().data_write_data;
        io().obs_data_write_en <<= pipeline.io().data_write_en;
        io().obs_perf_cycles <<= pipeline.io().perf_cycles;
        io().obs_perf_instructions <<= pipeline.io().perf_instructions;
        io().obs_perf_branch_count <<= pipeline.io().perf_branch_count;
        io().obs_perf_branch_mispredict <<= pipeline.io().perf_branch_mispredict;
        io().obs_perf_load_use_stalls <<= pipeline.io().perf_load_use_stalls;
        io().obs_perf_control_stalls <<= pipeline.io().perf_control_stalls;

        // ---- 诊断输出 ----
        io().dbg_id_instr <<= pipeline.io().dbg_id_instr;
        io().dbg_id_pc <<= pipeline.io().dbg_id_pc;
        io().dbg_id_valid <<= pipeline.io().dbg_id_valid;
        io().dbg_id_is_branch <<= pipeline.io().dbg_id_is_branch;
        io().dbg_id_is_load <<= pipeline.io().dbg_id_is_load;
        io().dbg_id_is_store <<= pipeline.io().dbg_id_is_store;
        io().dbg_branch_taken <<= pipeline.io().dbg_branch_taken;
        io().dbg_wb_write_en <<= pipeline.io().dbg_wb_write_en;
        io().dbg_wb_write_data <<= pipeline.io().dbg_wb_write_data;
        io().dbg_wb_rd_addr <<= pipeline.io().dbg_wb_rd_addr;
        io().dbg_exmem_alu_result <<= pipeline.io().dbg_exmem_alu_result;
        io().dbg_exmem_imm <<= pipeline.io().dbg_exmem_imm;
        io().dbg_exmem_rs2_data <<= pipeline.io().dbg_exmem_rs2_data;
        io().obs_data_rdata <<= dtcm.io().rdata;
        io().dbg_exmem_valid <<= pipeline.io().dbg_exmem_valid;
        io().dbg_mem_valid_out <<= pipeline.io().dbg_mem_valid_out;
        io().dbg_load_use_detected <<= pipeline.io().dbg_load_use_detected;
    }
};
