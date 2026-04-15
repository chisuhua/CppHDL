/**
 * @file mem_stage.h
 * @brief RISC-V 5 级流水线 - MEM 级 (访存级)
 * 
 * 功能:
 * - 数据存储器接口 (加载/存储)
 * - 字节/半字/字处理
 * - 符号扩展/零扩展
 * - 数据前向到 WB 级
 * 
 * 作者：DevMate
 * 最后修改：2026-04-09
 */

#pragma once

#include "ch.hpp"
#include "component.h"
#include "chlib/arithmetic.h"
#include "chlib/logic.h"

using namespace ch::core;

namespace riscv {

/**
 * MEM 级 (访存级) 组件
 * 
 * 包含:
 * - 数据内存接口 (加载和存储)
 * - 支持 LB/LH/LW/LBU/LHU 加载指令
 * - 支持 SB/SH/SW 存储指令
 * - 符号扩展和零扩展处理
 * - 数据多选器 (ALU 结果或内存数据)
 */
class MemStage : public ch::Component {
public:
    __io(
        // ====================================================================
        // 输入端口：来自 EX/MEM 流水线寄存器
        // ====================================================================
        ch_in<ch_uint<32>> alu_result;   // ALU 结果 (内存地址 for load/store)
        ch_in<ch_uint<32>> rs2_data;     // RS2 数据 (存储数据)
        ch_in<ch_uint<5>>  rd_addr;     // 目的寄存器地址 (传递到 WB)
        ch_in<ch_bool>     is_load;      // 是否为加载指令
        ch_in<ch_bool>     is_store;     // 是否为存储指令
        ch_in<ch_uint<3>>  funct3;       // 功能码 (决定数据宽度)
        ch_in<ch_bool>     mem_valid;    // 内存访问有效信号 (来自控制单元)
        
        // ====================================================================
        // 输出端口：写入 MEM/WB 流水线寄存器
        // ====================================================================
        ch_out<ch_uint<32>> alu_result_out; // ALU 结果传递到 WB (流水线寄存器)
        ch_out<ch_uint<5>>  rd_addr_out;  // rd_addr 流水线寄存器
        ch_out<ch_uint<32>> mem_data;    // 从内存读出的数据 (WB 级使用)
        ch_out<ch_bool>     mem_valid_out; // 内存数据有效 (用于 WB 级多选)
    )

    MemStage(ch::Component* parent = nullptr, const std::string& name = "mem_stage")
        : ch::Component(parent, name) {}

    void create_ports() override {
        new (io_storage_) io_type;
    }

    void describe() override {
        // ====================================================================
        // 1. 内存访问控制逻辑
        // ====================================================================
        // 只有当 is_load 或 is_store 为真时才进行内存访问
        // mem_valid 信号控制是否真正访问内存

        auto do_mem_access = io().is_load | io().is_store;
        auto mem_access_valid = do_mem_access & io().mem_valid;

        // ====================================================================
        // 2. 加载指令处理 (Load)
        // ====================================================================
        // 根据 funct3 确定加载的数据宽度和扩展方式:
        // 
        // funct3 | 指令 | 宽度 | 扩展方式
        // -------|------|------|--------
        // 000    | LB   | 8 位 | 符号扩展
        // 001    | LH   | 16 位| 符号扩展
        // 010    | LW   | 32 位| 无扩展
        // 100    | LBU  | 8 位 | 零扩展
        // 101    | LHU  | 16 位| 零扩展

        // ---- 2.1 从内存读取数据 (简化模型，假设单周期内存) ----
        // 实际实现中，这里会连接真实的内存模块
        // memory.sread(alu_result, clk_en)
        // 这里我们使用一个占位值，实际数据由外部内存模块提供
        ch_uint<32> mem_read_data = ch_uint<32>(0_d);

        // ---- 2.2 字节加载 (LB/LBU) ----
        // 提取地址的低位 2bit 来确定字节位置
        auto byte_offset = bits<1, 0>(io().alu_result);
        
        // 根据字节偏移选择对应的字节
        auto byte0 = bits<7, 0>(mem_read_data);   // 地址 [1:0] == 00
        auto byte1 = bits<15, 8>(mem_read_data);  // 地址 [1:0] == 01
        auto byte2 = bits<23, 16>(mem_read_data); // 地址 [1:0] == 10
        auto byte3 = bits<31, 24>(mem_read_data); // 地址 [1:0] == 11

        // 字节选择器
        ch_uint<8> selected_byte = ch_uint<8>(0_d);
        selected_byte = select(byte_offset == ch_uint<2>(0_d), byte0, selected_byte);
        selected_byte = select(byte_offset == ch_uint<2>(1_d), byte1, selected_byte);
        selected_byte = select(byte_offset == ch_uint<2>(2_d), byte2, selected_byte);
        selected_byte = select(byte_offset == ch_uint<2>(3_d), byte3, selected_byte);

        // LB: 符号扩展到 32 位
        // 方法：检查最高位，如果为 1 则填充 1
        auto byte_sign = bits<7, 7>(selected_byte);
        ch_uint<32> lb_result = select(byte_sign, 
            ch_uint<32>(4278190080_d) | zext<32>(selected_byte),  // 0xFFFFFF00
            zext<32>(selected_byte));

        // LBU: 零扩展到 32 位
        ch_uint<32> lbu_result = zext<32>(selected_byte);

        // ---- 2.3 半字加载 (LH/LHU) ----
        // 提取地址的低位 1bit 来确定半字位置 (半字对齐)
        auto half_offset = bit_select(io().alu_result, 1_d);
        
        // 根据半字偏移选择对应的半字
        auto half0 = bits<15, 0>(mem_read_data);  // 地址 [1] == 0
        auto half1 = bits<31, 16>(mem_read_data); // 地址 [1] == 1

        // 半字选择器
        ch_uint<16> selected_half = select(half_offset == ch_bool(false), half0, half1);

        // LH: 符号扩展到 32 位
        auto half_sign = bits<15, 15>(selected_half);
        ch_uint<32> lh_result = select(half_sign,
            ch_uint<32>(4294901760_d) | zext<32>(selected_half),  // 0xFFFF0000
            zext<32>(selected_half));

        // LHU: 零扩展到 32 位
        ch_uint<32> lhu_result = zext<32>(selected_half);

        // ---- 2.4 字加载 (LW) ----
        // 直接读取 32 位数据，无需扩展
        ch_uint<32> lw_result = mem_read_data;

        // ---- 2.5 加载结果多选器 ----
        // 根据 funct3 选择正确的加载结果
        ch_uint<32> load_result = ch_uint<32>(0_d);

        // LB (000) - 符号扩展字节
        load_result = select(io().funct3 == ch_uint<3>(0_d), lb_result, load_result);
        // LH (001) - 符号扩展半字
        load_result = select(io().funct3 == ch_uint<3>(1_d), lh_result, load_result);
        // LW (010) - 字
        load_result = select(io().funct3 == ch_uint<3>(2_d), lw_result, load_result);
        // LBU (100) - 零扩展字节
        load_result = select(io().funct3 == ch_uint<3>(4_d), lbu_result, load_result);
        // LHU (101) - 零扩展半字
        load_result = select(io().funct3 == ch_uint<3>(5_d), lhu_result, load_result);

        // 最终加载输出：只有 is_load 为真时才使用加载结果
        auto final_load_data = select(io().is_load, load_result, ch_uint<32>(0_d));

        // ====================================================================
        // 3. 存储指令处理 (Store)
        // ====================================================================
        // 存储指令不需要输出数据到 WB 级，但需要生成存储器写使能和写数据
        // 这里我们只输出一个存储有效的标志

        // 存储字节使能信号 (用于内存接口的 strbe)
        // 根据 funct3 生成对应的字节使能
        ch_uint<4> store_byte_enable = ch_uint<4>(0_d);
        
        // SB (000): 存储单个字节
        auto sb_be = select(byte_offset == ch_uint<2>(0_d), ch_uint<4>(1_d),   // 0001
                       select(byte_offset == ch_uint<2>(1_d), ch_uint<4>(2_d),   // 0010
                       select(byte_offset == ch_uint<2>(2_d), ch_uint<4>(4_d),   // 0100
                              ch_uint<4>(8_d))));                                // 1000
        
        // SH (001): 存储半字 (2 字节)
        auto sh_be = select(half_offset == ch_bool(false), ch_uint<4>(3_d),     // 0011
                            ch_uint<4>(12_d));                                  // 1100
        
        // SW (010): 存储字 (4 字节)
        auto sw_be = ch_uint<4>(15_d);  // 1111

        // 存储字节使能选择器
        store_byte_enable = select(io().funct3 == ch_uint<3>(0_d), sb_be, store_byte_enable);
        store_byte_enable = select(io().funct3 == ch_uint<3>(1_d), sh_be, store_byte_enable);
        store_byte_enable = select(io().funct3 == ch_uint<3>(2_d), sw_be, store_byte_enable);

        // 存储数据也需要根据 funct3 选择正确的部分
        // 但对于输出到 WB 级，存储指令不需要数据
        // 这里我们只输出一个占位值
        ch_uint<32> store_data_out = ch_uint<32>(0_d);

        // ====================================================================
        // 4. MEM 级输出多选器
        // ====================================================================
        // MEM 级输出：
        // - 对于加载指令：输出从内存读取的数据
        // - 对于存储指令：输出 0 (存储指令不写回寄存器)
        // - 对于其他指令：传递 ALU 结果到 WB 级 (在 WB 级处理)
        
        // 内存数据有效信号：只有加载指令才有效
        auto mem_data_valid = io().is_load & io().mem_valid;

        // ====================================================================
        // 5. 输出端口连接
        // ====================================================================

        // 内存读出的数据 (用于 WB 级)
        io().mem_data = select(io().is_load, final_load_data, ch_uint<32>(0_d));
        
        // 内存数据有效信号 (用于 WB 级判断是否使用 mem_data)
        io().mem_valid_out = mem_data_valid;
    }
};

} // namespace riscv
