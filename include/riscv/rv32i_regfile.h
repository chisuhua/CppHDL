/**
 * @file rv32i_regfile.h
 * @brief RV32I 寄存器文件 (32x32 bit)
 * 
 * 特性:
 * - 32 个 32 位通用寄存器
 * - 2 个读端口 (组合逻辑)
 * - 1 个写端口 (时序逻辑)
 * - x0 硬连线为 0
 */

#pragma once

#include "ch.hpp"
#include "component.h"

using namespace ch::core;

namespace riscv {

class Rv32iRegFile : public ch::Component {
public:
    __io(
        // 读端口 A
        ch_in<ch_uint<5>> rs1_addr;
        ch_out<ch_uint<32>> rs1_data;
        
        // 读端口 B
        ch_in<ch_uint<5>> rs2_addr;
        ch_out<ch_uint<32>> rs2_data;
        
        // 写端口
        ch_in<ch_uint<5>> rd_addr;
        ch_in<ch_uint<32>> rd_data;
        ch_in<ch_bool> rd_we;
        
        // 时钟与复位
        ch_in<ch_bool> clk;
        ch_in<ch_bool> rst;
    )
    
    Rv32iRegFile(ch::Component* parent = nullptr, const std::string& name = "rv32i_regfile")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // ========================================================================
        // 寄存器定义 (x0-x31)
        // ========================================================================
        ch_reg<ch_uint<32>> x0(0_d);   // 硬连线为 0
        ch_reg<ch_uint<32>> x1(0_d);
        ch_reg<ch_uint<32>> x2(0_d);
        ch_reg<ch_uint<32>> x3(0_d);
        ch_reg<ch_uint<32>> x4(0_d);
        ch_reg<ch_uint<32>> x5(0_d);
        ch_reg<ch_uint<32>> x6(0_d);
        ch_reg<ch_uint<32>> x7(0_d);
        ch_reg<ch_uint<32>> x8(0_d);
        ch_reg<ch_uint<32>> x9(0_d);
        ch_reg<ch_uint<32>> x10(0_d);
        ch_reg<ch_uint<32>> x11(0_d);
        ch_reg<ch_uint<32>> x12(0_d);
        ch_reg<ch_uint<32>> x13(0_d);
        ch_reg<ch_uint<32>> x14(0_d);
        ch_reg<ch_uint<32>> x15(0_d);
        ch_reg<ch_uint<32>> x16(0_d);
        ch_reg<ch_uint<32>> x17(0_d);
        ch_reg<ch_uint<32>> x18(0_d);
        ch_reg<ch_uint<32>> x19(0_d);
        ch_reg<ch_uint<32>> x20(0_d);
        ch_reg<ch_uint<32>> x21(0_d);
        ch_reg<ch_uint<32>> x22(0_d);
        ch_reg<ch_uint<32>> x23(0_d);
        ch_reg<ch_uint<32>> x24(0_d);
        ch_reg<ch_uint<32>> x25(0_d);
        ch_reg<ch_uint<32>> x26(0_d);
        ch_reg<ch_uint<32>> x27(0_d);
        ch_reg<ch_uint<32>> x28(0_d);
        ch_reg<ch_uint<32>> x29(0_d);
        ch_reg<ch_uint<32>> x30(0_d);
        ch_reg<ch_uint<32>> x31(0_d);
        
        // ========================================================================
        // 读端口 A (组合逻辑)
        // ========================================================================
        ch_uint<32> data_a(0_d);
        
        // 根据 rs1_addr 选择寄存器
        auto sel_a0 = (io().rs1_addr == ch_uint<5>(0_d));
        auto sel_a1 = (io().rs1_addr == ch_uint<5>(1_d));
        auto sel_a2 = (io().rs1_addr == ch_uint<5>(2_d));
        auto sel_a3 = (io().rs1_addr == ch_uint<5>(3_d));
        auto sel_a4 = (io().rs1_addr == ch_uint<5>(4_d));
        auto sel_a5 = (io().rs1_addr == ch_uint<5>(5_d));
        auto sel_a6 = (io().rs1_addr == ch_uint<5>(6_d));
        auto sel_a7 = (io().rs1_addr == ch_uint<5>(7_d));
        auto sel_a8 = (io().rs1_addr == ch_uint<5>(8_d));
        auto sel_a9 = (io().rs1_addr == ch_uint<5>(9_d));
        auto sel_a10 = (io().rs1_addr == ch_uint<5>(10_d));
        auto sel_a11 = (io().rs1_addr == ch_uint<5>(11_d));
        auto sel_a12 = (io().rs1_addr == ch_uint<5>(12_d));
        auto sel_a13 = (io().rs1_addr == ch_uint<5>(13_d));
        auto sel_a14 = (io().rs1_addr == ch_uint<5>(14_d));
        auto sel_a15 = (io().rs1_addr == ch_uint<5>(15_d));
        auto sel_a16 = (io().rs1_addr == ch_uint<5>(16_d));
        auto sel_a17 = (io().rs1_addr == ch_uint<5>(17_d));
        auto sel_a18 = (io().rs1_addr == ch_uint<5>(18_d));
        auto sel_a19 = (io().rs1_addr == ch_uint<5>(19_d));
        auto sel_a20 = (io().rs1_addr == ch_uint<5>(20_d));
        auto sel_a21 = (io().rs1_addr == ch_uint<5>(21_d));
        auto sel_a22 = (io().rs1_addr == ch_uint<5>(22_d));
        auto sel_a23 = (io().rs1_addr == ch_uint<5>(23_d));
        auto sel_a24 = (io().rs1_addr == ch_uint<5>(24_d));
        auto sel_a25 = (io().rs1_addr == ch_uint<5>(25_d));
        auto sel_a26 = (io().rs1_addr == ch_uint<5>(26_d));
        auto sel_a27 = (io().rs1_addr == ch_uint<5>(27_d));
        auto sel_a28 = (io().rs1_addr == ch_uint<5>(28_d));
        auto sel_a29 = (io().rs1_addr == ch_uint<5>(29_d));
        auto sel_a30 = (io().rs1_addr == ch_uint<5>(30_d));
        auto sel_a31 = (io().rs1_addr == ch_uint<5>(31_d));
        
        data_a = select(sel_a0, x0, data_a);
        data_a = select(sel_a1, x1, data_a);
        data_a = select(sel_a2, x2, data_a);
        data_a = select(sel_a3, x3, data_a);
        data_a = select(sel_a4, x4, data_a);
        data_a = select(sel_a5, x5, data_a);
        data_a = select(sel_a6, x6, data_a);
        data_a = select(sel_a7, x7, data_a);
        data_a = select(sel_a8, x8, data_a);
        data_a = select(sel_a9, x9, data_a);
        data_a = select(sel_a10, x10, data_a);
        data_a = select(sel_a11, x11, data_a);
        data_a = select(sel_a12, x12, data_a);
        data_a = select(sel_a13, x13, data_a);
        data_a = select(sel_a14, x14, data_a);
        data_a = select(sel_a15, x15, data_a);
        data_a = select(sel_a16, x16, data_a);
        data_a = select(sel_a17, x17, data_a);
        data_a = select(sel_a18, x18, data_a);
        data_a = select(sel_a19, x19, data_a);
        data_a = select(sel_a20, x20, data_a);
        data_a = select(sel_a21, x21, data_a);
        data_a = select(sel_a22, x22, data_a);
        data_a = select(sel_a23, x23, data_a);
        data_a = select(sel_a24, x24, data_a);
        data_a = select(sel_a25, x25, data_a);
        data_a = select(sel_a26, x26, data_a);
        data_a = select(sel_a27, x27, data_a);
        data_a = select(sel_a28, x28, data_a);
        data_a = select(sel_a29, x29, data_a);
        data_a = select(sel_a30, x30, data_a);
        data_a = select(sel_a31, x31, data_a);
        
        // x0 硬连线为 0
        data_a = select(sel_a0, ch_uint<32>(0_d), data_a);
        
        io().rs1_data = data_a;
        
        // ========================================================================
        // 读端口 B (组合逻辑)
        // ========================================================================
        ch_uint<32> data_b(0_d);
        
        auto sel_b0 = (io().rs2_addr == ch_uint<5>(0_d));
        auto sel_b1 = (io().rs2_addr == ch_uint<5>(1_d));
        auto sel_b2 = (io().rs2_addr == ch_uint<5>(2_d));
        auto sel_b3 = (io().rs2_addr == ch_uint<5>(3_d));
        auto sel_b4 = (io().rs2_addr == ch_uint<5>(4_d));
        auto sel_b5 = (io().rs2_addr == ch_uint<5>(5_d));
        auto sel_b6 = (io().rs2_addr == ch_uint<5>(6_d));
        auto sel_b7 = (io().rs2_addr == ch_uint<5>(7_d));
        auto sel_b8 = (io().rs2_addr == ch_uint<5>(8_d));
        auto sel_b9 = (io().rs2_addr == ch_uint<5>(9_d));
        auto sel_b10 = (io().rs2_addr == ch_uint<5>(10_d));
        auto sel_b11 = (io().rs2_addr == ch_uint<5>(11_d));
        auto sel_b12 = (io().rs2_addr == ch_uint<5>(12_d));
        auto sel_b13 = (io().rs2_addr == ch_uint<5>(13_d));
        auto sel_b14 = (io().rs2_addr == ch_uint<5>(14_d));
        auto sel_b15 = (io().rs2_addr == ch_uint<5>(15_d));
        auto sel_b16 = (io().rs2_addr == ch_uint<5>(16_d));
        auto sel_b17 = (io().rs2_addr == ch_uint<5>(17_d));
        auto sel_b18 = (io().rs2_addr == ch_uint<5>(18_d));
        auto sel_b19 = (io().rs2_addr == ch_uint<5>(19_d));
        auto sel_b20 = (io().rs2_addr == ch_uint<5>(20_d));
        auto sel_b21 = (io().rs2_addr == ch_uint<5>(21_d));
        auto sel_b22 = (io().rs2_addr == ch_uint<5>(22_d));
        auto sel_b23 = (io().rs2_addr == ch_uint<5>(23_d));
        auto sel_b24 = (io().rs2_addr == ch_uint<5>(24_d));
        auto sel_b25 = (io().rs2_addr == ch_uint<5>(25_d));
        auto sel_b26 = (io().rs2_addr == ch_uint<5>(26_d));
        auto sel_b27 = (io().rs2_addr == ch_uint<5>(27_d));
        auto sel_b28 = (io().rs2_addr == ch_uint<5>(28_d));
        auto sel_b29 = (io().rs2_addr == ch_uint<5>(29_d));
        auto sel_b30 = (io().rs2_addr == ch_uint<5>(30_d));
        auto sel_b31 = (io().rs2_addr == ch_uint<5>(31_d));
        
        data_b = select(sel_b0, x0, data_b);
        data_b = select(sel_b1, x1, data_b);
        data_b = select(sel_b2, x2, data_b);
        data_b = select(sel_b3, x3, data_b);
        data_b = select(sel_b4, x4, data_b);
        data_b = select(sel_b5, x5, data_b);
        data_b = select(sel_b6, x6, data_b);
        data_b = select(sel_b7, x7, data_b);
        data_b = select(sel_b8, x8, data_b);
        data_b = select(sel_b9, x9, data_b);
        data_b = select(sel_b10, x10, data_b);
        data_b = select(sel_b11, x11, data_b);
        data_b = select(sel_b12, x12, data_b);
        data_b = select(sel_b13, x13, data_b);
        data_b = select(sel_b14, x14, data_b);
        data_b = select(sel_b15, x15, data_b);
        data_b = select(sel_b16, x16, data_b);
        data_b = select(sel_b17, x17, data_b);
        data_b = select(sel_b18, x18, data_b);
        data_b = select(sel_b19, x19, data_b);
        data_b = select(sel_b20, x20, data_b);
        data_b = select(sel_b21, x21, data_b);
        data_b = select(sel_b22, x22, data_b);
        data_b = select(sel_b23, x23, data_b);
        data_b = select(sel_b24, x24, data_b);
        data_b = select(sel_b25, x25, data_b);
        data_b = select(sel_b26, x26, data_b);
        data_b = select(sel_b27, x27, data_b);
        data_b = select(sel_b28, x28, data_b);
        data_b = select(sel_b29, x29, data_b);
        data_b = select(sel_b30, x30, data_b);
        data_b = select(sel_b31, x31, data_b);
        
        // x0 硬连线为 0
        data_b = select(sel_b0, ch_uint<32>(0_d), data_b);
        
        io().rs2_data = data_b;
        
        // ========================================================================
        // 写端口 (时序逻辑)
        // ========================================================================
        
        // 写使能信号 (rd_addr != 0 且 rd_we == 1)
        auto we = select(io().rd_addr != ch_uint<5>(0_d), io().rd_we, ch_bool(false));
        
        // 写数据选择
        auto sel_w0 = (io().rd_addr == ch_uint<5>(0_d));
        auto sel_w1 = (io().rd_addr == ch_uint<5>(1_d));
        auto sel_w2 = (io().rd_addr == ch_uint<5>(2_d));
        auto sel_w3 = (io().rd_addr == ch_uint<5>(3_d));
        auto sel_w4 = (io().rd_addr == ch_uint<5>(4_d));
        auto sel_w5 = (io().rd_addr == ch_uint<5>(5_d));
        auto sel_w6 = (io().rd_addr == ch_uint<5>(6_d));
        auto sel_w7 = (io().rd_addr == ch_uint<5>(7_d));
        auto sel_w8 = (io().rd_addr == ch_uint<5>(8_d));
        auto sel_w9 = (io().rd_addr == ch_uint<5>(9_d));
        auto sel_w10 = (io().rd_addr == ch_uint<5>(10_d));
        auto sel_w11 = (io().rd_addr == ch_uint<5>(11_d));
        auto sel_w12 = (io().rd_addr == ch_uint<5>(12_d));
        auto sel_w13 = (io().rd_addr == ch_uint<5>(13_d));
        auto sel_w14 = (io().rd_addr == ch_uint<5>(14_d));
        auto sel_w15 = (io().rd_addr == ch_uint<5>(15_d));
        auto sel_w16 = (io().rd_addr == ch_uint<5>(16_d));
        auto sel_w17 = (io().rd_addr == ch_uint<5>(17_d));
        auto sel_w18 = (io().rd_addr == ch_uint<5>(18_d));
        auto sel_w19 = (io().rd_addr == ch_uint<5>(19_d));
        auto sel_w20 = (io().rd_addr == ch_uint<5>(20_d));
        auto sel_w21 = (io().rd_addr == ch_uint<5>(21_d));
        auto sel_w22 = (io().rd_addr == ch_uint<5>(22_d));
        auto sel_w23 = (io().rd_addr == ch_uint<5>(23_d));
        auto sel_w24 = (io().rd_addr == ch_uint<5>(24_d));
        auto sel_w25 = (io().rd_addr == ch_uint<5>(25_d));
        auto sel_w26 = (io().rd_addr == ch_uint<5>(26_d));
        auto sel_w27 = (io().rd_addr == ch_uint<5>(27_d));
        auto sel_w28 = (io().rd_addr == ch_uint<5>(28_d));
        auto sel_w29 = (io().rd_addr == ch_uint<5>(29_d));
        auto sel_w30 = (io().rd_addr == ch_uint<5>(30_d));
        auto sel_w31 = (io().rd_addr == ch_uint<5>(31_d));
        
        // 寄存器更新 (使用 ->next)
        x0->next = x0;   // x0 永远为 0
        x1->next = select(we && sel_w1, io().rd_data, x1);
        x2->next = select(we && sel_w2, io().rd_data, x2);
        x3->next = select(we && sel_w3, io().rd_data, x3);
        x4->next = select(we && sel_w4, io().rd_data, x4);
        x5->next = select(we && sel_w5, io().rd_data, x5);
        x6->next = select(we && sel_w6, io().rd_data, x6);
        x7->next = select(we && sel_w7, io().rd_data, x7);
        x8->next = select(we && sel_w8, io().rd_data, x8);
        x9->next = select(we && sel_w9, io().rd_data, x9);
        x10->next = select(we && sel_w10, io().rd_data, x10);
        x11->next = select(we && sel_w11, io().rd_data, x11);
        x12->next = select(we && sel_w12, io().rd_data, x12);
        x13->next = select(we && sel_w13, io().rd_data, x13);
        x14->next = select(we && sel_w14, io().rd_data, x14);
        x15->next = select(we && sel_w15, io().rd_data, x15);
        x16->next = select(we && sel_w16, io().rd_data, x16);
        x17->next = select(we && sel_w17, io().rd_data, x17);
        x18->next = select(we && sel_w18, io().rd_data, x18);
        x19->next = select(we && sel_w19, io().rd_data, x19);
        x20->next = select(we && sel_w20, io().rd_data, x20);
        x21->next = select(we && sel_w21, io().rd_data, x21);
        x22->next = select(we && sel_w22, io().rd_data, x22);
        x23->next = select(we && sel_w23, io().rd_data, x23);
        x24->next = select(we && sel_w24, io().rd_data, x24);
        x25->next = select(we && sel_w25, io().rd_data, x25);
        x26->next = select(we && sel_w26, io().rd_data, x26);
        x27->next = select(we && sel_w27, io().rd_data, x27);
        x28->next = select(we && sel_w28, io().rd_data, x28);
        x29->next = select(we && sel_w29, io().rd_data, x29);
        x30->next = select(we && sel_w30, io().rd_data, x30);
        x31->next = select(we && sel_w31, io().rd_data, x31);
    }
};

} // namespace riscv
