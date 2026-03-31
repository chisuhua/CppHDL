/**
 * RV32I Register File
 * 
 * 32×32-bit 寄存器文件
 * - x0 硬连线为 0
 * - 双读端口，单写端口
 */

#pragma once

#include "ch.hpp"
#include "component.h"

using namespace ch::core;

namespace riscv {

class RegFile : public ch::Component {
public:
    __io(
        // 读端口 1
        ch_in<ch_uint<5>> rs1_addr;
        ch_out<ch_uint<32>> rs1_data;
        
        // 读端口 2
        ch_in<ch_uint<5>> rs2_addr;
        ch_out<ch_uint<32>> rs2_data;
        
        // 写端口
        ch_in<ch_uint<5>> rd_addr;
        ch_in<ch_uint<32>> rd_data;
        ch_in<ch_bool> rd_we;
    )
    
    RegFile(ch::Component* parent = nullptr, const std::string& name = "regfile")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 32 个 32 位寄存器
        ch_reg<ch_uint<32>> r0(0_d), r1(0_d), r2(0_d), r3(0_d);
        ch_reg<ch_uint<32>> r4(0_d), r5(0_d), r6(0_d), r7(0_d);
        ch_reg<ch_uint<32>> r8(0_d), r9(0_d), r10(0_d), r11(0_d);
        ch_reg<ch_uint<32>> r12(0_d), r13(0_d), r14(0_d), r15(0_d);
        ch_reg<ch_uint<32>> r16(0_d), r17(0_d), r18(0_d), r19(0_d);
        ch_reg<ch_uint<32>> r20(0_d), r21(0_d), r22(0_d), r23(0_d);
        ch_reg<ch_uint<32>> r24(0_d), r25(0_d), r26(0_d), r27(0_d);
        ch_reg<ch_uint<32>> r28(0_d), r29(0_d), r30(0_d), r31(0_d);
        
        ch_reg<ch_uint<32>>* regs[32] = {
            &r0, &r1, &r2, &r3, &r4, &r5, &r6, &r7,
            &r8, &r9, &r10, &r11, &r12, &r13, &r14, &r15,
            &r16, &r17, &r18, &r19, &r20, &r21, &r22, &r23,
            &r24, &r25, &r26, &r27, &r28, &r29, &r30, &r31
        };
        
        // x0 硬连线为 0，其他寄存器可读
        auto rs1_val = ch_uint<32>(0_d);
        rs1_val = select(io().rs1_addr == ch_uint<5>(1_d), r1, rs1_val);
        rs1_val = select(io().rs1_addr == ch_uint<5>(2_d), r2, rs1_val);
        rs1_val = select(io().rs1_addr == ch_uint<5>(3_d), r3, rs1_val);
        rs1_val = select(io().rs1_addr == ch_uint<5>(4_d), r4, rs1_val);
        rs1_val = select(io().rs1_addr == ch_uint<5>(5_d), r5, rs1_val);
        rs1_val = select(io().rs1_addr == ch_uint<5>(6_d), r6, rs1_val);
        rs1_val = select(io().rs1_addr == ch_uint<5>(7_d), r7, rs1_val);
        rs1_val = select(io().rs1_addr == ch_uint<5>(8_d), r8, rs1_val);
        rs1_val = select(io().rs1_addr == ch_uint<5>(9_d), r9, rs1_val);
        rs1_val = select(io().rs1_addr == ch_uint<5>(10_d), r10, rs1_val);
        rs1_val = select(io().rs1_addr == ch_uint<5>(11_d), r11, rs1_val);
        rs1_val = select(io().rs1_addr == ch_uint<5>(12_d), r12, rs1_val);
        rs1_val = select(io().rs1_addr == ch_uint<5>(13_d), r13, rs1_val);
        rs1_val = select(io().rs1_addr == ch_uint<5>(14_d), r14, rs1_val);
        rs1_val = select(io().rs1_addr == ch_uint<5>(15_d), r15, rs1_val);
        rs1_val = select(io().rs1_addr == ch_uint<5>(16_d), r16, rs1_val);
        rs1_val = select(io().rs1_addr == ch_uint<5>(17_d), r17, rs1_val);
        rs1_val = select(io().rs1_addr == ch_uint<5>(18_d), r18, rs1_val);
        rs1_val = select(io().rs1_addr == ch_uint<5>(19_d), r19, rs1_val);
        rs1_val = select(io().rs1_addr == ch_uint<5>(20_d), r20, rs1_val);
        rs1_val = select(io().rs1_addr == ch_uint<5>(21_d), r21, rs1_val);
        rs1_val = select(io().rs1_addr == ch_uint<5>(22_d), r22, rs1_val);
        rs1_val = select(io().rs1_addr == ch_uint<5>(23_d), r23, rs1_val);
        rs1_val = select(io().rs1_addr == ch_uint<5>(24_d), r24, rs1_val);
        rs1_val = select(io().rs1_addr == ch_uint<5>(25_d), r25, rs1_val);
        rs1_val = select(io().rs1_addr == ch_uint<5>(26_d), r26, rs1_val);
        rs1_val = select(io().rs1_addr == ch_uint<5>(27_d), r27, rs1_val);
        rs1_val = select(io().rs1_addr == ch_uint<5>(28_d), r28, rs1_val);
        rs1_val = select(io().rs1_addr == ch_uint<5>(29_d), r29, rs1_val);
        rs1_val = select(io().rs1_addr == ch_uint<5>(30_d), r30, rs1_val);
        rs1_val = select(io().rs1_addr == ch_uint<5>(31_d), r31, rs1_val);
        
        auto rs2_val = ch_uint<32>(0_d);
        rs2_val = select(io().rs2_addr == ch_uint<5>(1_d), r1, rs2_val);
        rs2_val = select(io().rs2_addr == ch_uint<5>(2_d), r2, rs2_val);
        rs2_val = select(io().rs2_addr == ch_uint<5>(3_d), r3, rs2_val);
        rs2_val = select(io().rs2_addr == ch_uint<5>(4_d), r4, rs2_val);
        rs2_val = select(io().rs2_addr == ch_uint<5>(5_d), r5, rs2_val);
        rs2_val = select(io().rs2_addr == ch_uint<5>(6_d), r6, rs2_val);
        rs2_val = select(io().rs2_addr == ch_uint<5>(7_d), r7, rs2_val);
        rs2_val = select(io().rs2_addr == ch_uint<5>(8_d), r8, rs2_val);
        rs2_val = select(io().rs2_addr == ch_uint<5>(9_d), r9, rs2_val);
        rs2_val = select(io().rs2_addr == ch_uint<5>(10_d), r10, rs2_val);
        rs2_val = select(io().rs2_addr == ch_uint<5>(11_d), r11, rs2_val);
        rs2_val = select(io().rs2_addr == ch_uint<5>(12_d), r12, rs2_val);
        rs2_val = select(io().rs2_addr == ch_uint<5>(13_d), r13, rs2_val);
        rs2_val = select(io().rs2_addr == ch_uint<5>(14_d), r14, rs2_val);
        rs2_val = select(io().rs2_addr == ch_uint<5>(15_d), r15, rs2_val);
        rs2_val = select(io().rs2_addr == ch_uint<5>(16_d), r16, rs2_val);
        rs2_val = select(io().rs2_addr == ch_uint<5>(17_d), r17, rs2_val);
        rs2_val = select(io().rs2_addr == ch_uint<5>(18_d), r18, rs2_val);
        rs2_val = select(io().rs2_addr == ch_uint<5>(19_d), r19, rs2_val);
        rs2_val = select(io().rs2_addr == ch_uint<5>(20_d), r20, rs2_val);
        rs2_val = select(io().rs2_addr == ch_uint<5>(21_d), r21, rs2_val);
        rs2_val = select(io().rs2_addr == ch_uint<5>(22_d), r22, rs2_val);
        rs2_val = select(io().rs2_addr == ch_uint<5>(23_d), r23, rs2_val);
        rs2_val = select(io().rs2_addr == ch_uint<5>(24_d), r24, rs2_val);
        rs2_val = select(io().rs2_addr == ch_uint<5>(25_d), r25, rs2_val);
        rs2_val = select(io().rs2_addr == ch_uint<5>(26_d), r26, rs2_val);
        rs2_val = select(io().rs2_addr == ch_uint<5>(27_d), r27, rs2_val);
        rs2_val = select(io().rs2_addr == ch_uint<5>(28_d), r28, rs2_val);
        rs2_val = select(io().rs2_addr == ch_uint<5>(29_d), r29, rs2_val);
        rs2_val = select(io().rs2_addr == ch_uint<5>(30_d), r30, rs2_val);
        rs2_val = select(io().rs2_addr == ch_uint<5>(31_d), r31, rs2_val);
        
        io().rs1_data = rs1_val;
        io().rs2_data = rs2_val;
        
        // 写寄存器 (x0 不可写)
        auto write_valid = io().rd_we && (io().rd_addr != ch_uint<5>(0_d));
        
        r1->next = select(write_valid && (io().rd_addr == ch_uint<5>(1_d)), io().rd_data, r1);
        r2->next = select(write_valid && (io().rd_addr == ch_uint<5>(2_d)), io().rd_data, r2);
        r3->next = select(write_valid && (io().rd_addr == ch_uint<5>(3_d)), io().rd_data, r3);
        r4->next = select(write_valid && (io().rd_addr == ch_uint<5>(4_d)), io().rd_data, r4);
        r5->next = select(write_valid && (io().rd_addr == ch_uint<5>(5_d)), io().rd_data, r5);
        r6->next = select(write_valid && (io().rd_addr == ch_uint<5>(6_d)), io().rd_data, r6);
        r7->next = select(write_valid && (io().rd_addr == ch_uint<5>(7_d)), io().rd_data, r7);
        r8->next = select(write_valid && (io().rd_addr == ch_uint<5>(8_d)), io().rd_data, r8);
        r9->next = select(write_valid && (io().rd_addr == ch_uint<5>(9_d)), io().rd_data, r9);
        r10->next = select(write_valid && (io().rd_addr == ch_uint<5>(10_d)), io().rd_data, r10);
        r11->next = select(write_valid && (io().rd_addr == ch_uint<5>(11_d)), io().rd_data, r11);
        r12->next = select(write_valid && (io().rd_addr == ch_uint<5>(12_d)), io().rd_data, r12);
        r13->next = select(write_valid && (io().rd_addr == ch_uint<5>(13_d)), io().rd_data, r13);
        r14->next = select(write_valid && (io().rd_addr == ch_uint<5>(14_d)), io().rd_data, r14);
        r15->next = select(write_valid && (io().rd_addr == ch_uint<5>(15_d)), io().rd_data, r15);
        r16->next = select(write_valid && (io().rd_addr == ch_uint<5>(16_d)), io().rd_data, r16);
        r17->next = select(write_valid && (io().rd_addr == ch_uint<5>(17_d)), io().rd_data, r17);
        r18->next = select(write_valid && (io().rd_addr == ch_uint<5>(18_d)), io().rd_data, r18);
        r19->next = select(write_valid && (io().rd_addr == ch_uint<5>(19_d)), io().rd_data, r19);
        r20->next = select(write_valid && (io().rd_addr == ch_uint<5>(20_d)), io().rd_data, r20);
        r21->next = select(write_valid && (io().rd_addr == ch_uint<5>(21_d)), io().rd_data, r21);
        r22->next = select(write_valid && (io().rd_addr == ch_uint<5>(22_d)), io().rd_data, r22);
        r23->next = select(write_valid && (io().rd_addr == ch_uint<5>(23_d)), io().rd_data, r23);
        r24->next = select(write_valid && (io().rd_addr == ch_uint<5>(24_d)), io().rd_data, r24);
        r25->next = select(write_valid && (io().rd_addr == ch_uint<5>(25_d)), io().rd_data, r25);
        r26->next = select(write_valid && (io().rd_addr == ch_uint<5>(26_d)), io().rd_data, r26);
        r27->next = select(write_valid && (io().rd_addr == ch_uint<5>(27_d)), io().rd_data, r27);
        r28->next = select(write_valid && (io().rd_addr == ch_uint<5>(28_d)), io().rd_data, r28);
        r29->next = select(write_valid && (io().rd_addr == ch_uint<5>(29_d)), io().rd_data, r29);
        r30->next = select(write_valid && (io().rd_addr == ch_uint<5>(30_d)), io().rd_data, r30);
        r31->next = select(write_valid && (io().rd_addr == ch_uint<5>(31_d)), io().rd_data, r31);
    }
};

} // namespace riscv
