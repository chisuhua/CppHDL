/**
 * @file forwarding_dut_fixture.h
 * @brief 前推通路**组合 DUT**：冒险检测（`riscv::HazardUnit`）+ 数据 mux（`chlib::ForwardingMux`）
 *
 * ⚠️ 2026-10-24（change `rv32i-canonical-convergence-2026-10`，ADR-046 D3 #14 / D5）：
 *    收敛前，`examples/riscv-mini/src/hazard_unit.h` 与 `include/cpu/pipeline/hazard_unit.h`
 *    是**同名不同语义**的两份实现：
 *      - lib 版：只做冒险检测（`rs1_data` 直通），因为把数据 mux 放进 `HazardUnit`
 *        并接进真实流水线装配会形成**组合环**
 *        （实测：`FATAL: Cycle detected in IR graph during topological sort`，
 *          `git show bf78367` 证明那是 2026-04-15 的消环事故记录）；
 *      - example 版：检测 + 数据 mux合一，仅在「作为独立 DUT、不接 EX stage 闭环」
 *        时可用。
 *    ⇒ ADR-046 D3 #14（删 example 副本、消费者重指 lib）与 D5（不在本 change
 *      裁决前推语义）**互斥** ⇒ 本 fixture 给出唯一同时满足两者的形态：
 *      **把数据 mux 交给既有的 `chlib::ForwardingMux`**（`include/chlib/forwarding.h`），
 *      `HazardUnit` 保持**唯一一份**且**不带** mux。
 *
 *    档位语义逐档等价（0→REG / 1→EX / 2→MEM）：
 *      | forward_a/b | chlib::ForwardingMux | 原 example 版 hazard_unit mux |
 *      |---|---|---|
 *      | 0 | `rs1_data_reg`   | `rs1_data_raw`   |
 *      | 1 | `ex_mem_alu_result` | `ex_alu_result` |
 *      | 2 | `mem_wb_result`  | `mem_alu_result` |
 *
 *    ⇒ 被测逻辑（冒险检测真值表 + mux 真值表）**不变**；只是从「单组件内联」
 *      变为「两组件组合」。断言的**期望值不变**。
 */
#pragma once

#include "ch.hpp"
#include "component.h"
#include "chlib/forwarding.h"
#include "cpu/pipeline/hazard_unit.h"

namespace riscv_test {

class ForwardingDut : public ch::Component {
public:
    __io(
        // 流水线寄存器地址信息
        ch_in<ch_uint<5>>  id_rs1_addr;
        ch_in<ch_uint<5>>  id_rs2_addr;
        ch_in<ch_uint<5>>  ex_rd_addr;
        ch_in<ch_uint<5>>  mem_rd_addr;
        ch_in<ch_uint<5>>  wb_rd_addr;

        // 写使能信号
        ch_in<ch_bool>     ex_reg_write;
        ch_in<ch_bool>     mem_reg_write;
        ch_in<ch_bool>     wb_reg_write;

        // 指令类型
        ch_in<ch_bool>     mem_is_load;
        ch_in<ch_bool>     ex_branch;
        ch_in<ch_bool>     ex_branch_taken;

        // 前推数据输入
        ch_in<ch_uint<32>> ex_alu_result;
        ch_in<ch_uint<32>> mem_alu_result;
        ch_in<ch_uint<32>> wb_write_data;

        // 原始寄存器数据
        ch_in<ch_uint<32>> rs1_data_raw;
        ch_in<ch_uint<32>> rs2_data_raw;

        // 前推控制输出
        ch_out<ch_uint<2>> forward_a;
        ch_out<ch_uint<2>> forward_b;

        // 前推数据输出（**经 mux 后的数据** —— 与原 example 版 hazard_unit 的
        // `rs1_data` 语义一致）
        ch_out<ch_uint<32>> rs1_data;
        ch_out<ch_uint<32>> rs2_data;

        // 冒险控制输出
        ch_out<ch_bool>    stall;
        ch_out<ch_bool>    flush;
        ch_out<ch_bool>    pc_src;
    )

    ForwardingDut(ch::Component* parent = nullptr, const std::string& name = "forwarding_dut")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch::ch_module<riscv::HazardUnit>        hazard{"hazard"};
        ch::ch_module<chlib::ForwardingMux<32>> mux{"forwarding_mux"};

        // --- 冒险检测单元的输入 ---
        hazard.io().id_rs1_addr     <<= io().id_rs1_addr;
        hazard.io().id_rs2_addr     <<= io().id_rs2_addr;
        hazard.io().ex_rd_addr      <<= io().ex_rd_addr;
        hazard.io().mem_rd_addr     <<= io().mem_rd_addr;
        hazard.io().wb_rd_addr      <<= io().wb_rd_addr;
        hazard.io().ex_reg_write    <<= io().ex_reg_write;
        hazard.io().mem_reg_write   <<= io().mem_reg_write;
        hazard.io().wb_reg_write    <<= io().wb_reg_write;
        hazard.io().mem_is_load     <<= io().mem_is_load;
        hazard.io().ex_branch       <<= io().ex_branch;
        hazard.io().ex_branch_taken <<= io().ex_branch_taken;
        hazard.io().rs1_data_raw    <<= io().rs1_data_raw;
        hazard.io().rs2_data_raw    <<= io().rs2_data_raw;
        hazard.io().ex_alu_result   <<= io().ex_alu_result;
        hazard.io().mem_alu_result  <<= io().mem_alu_result;
        hazard.io().wb_write_data   <<= io().wb_write_data;

        // --- 前推数据 mux：源选择来自 hazard，数据来自 TB / 流水线 ---
        mux.io().forward_a         <<= hazard.io().forward_a;
        mux.io().forward_b         <<= hazard.io().forward_b;
        mux.io().rs1_data_reg      <<= io().rs1_data_raw;
        mux.io().rs2_data_reg      <<= io().rs2_data_raw;
        mux.io().ex_mem_alu_result <<= io().ex_alu_result;
        mux.io().mem_wb_result     <<= io().mem_alu_result;

        // --- 输出 ---
        io().forward_a <<= hazard.io().forward_a;
        io().forward_b <<= hazard.io().forward_b;
        io().rs1_data  <<= mux.io().alu_input_a;
        io().rs2_data  <<= mux.io().alu_input_b;
        io().stall     <<= hazard.io().stall;
        io().flush     <<= hazard.io().flush;
        io().pc_src    <<= hazard.io().pc_src;
    }
};

} // namespace riscv_test
