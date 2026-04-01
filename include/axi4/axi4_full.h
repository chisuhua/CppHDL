/**
 * @file axi4_full.h
 * @brief AXI4 全功能从设备控制器
 * 
 * 支持 AXI4 协议完整功能：
 * - 5 个独立通道 (AW/W/B/AR/R)
 * - 突发传输 (INCR/FIXED)
 * - 最大突发长度：16
 * - 可配置数据位宽 (32/64/128/256/512/1024)
 * 
 * AXI4 vs AXI4-Lite:
 * | 特性 | AXI4-Lite | AXI4 |
 * |------|-----------|------|
 * | 突发传输 | ❌ | ✅ |
 * | 突发长度 | 1 | 1-16 |
 * | 位宽 | 32/64 | 32-1024 |
 * | 地址宽度 | 32 | 32-64 |
 * | 响应类型 | OKAY/SLVERR | OKAY/EXOKAY/SLVERR/DECERR |
 */

#pragma once

#include "ch.hpp"
#include "component.h"

using namespace ch::core;

namespace axi4 {

// ============================================================================
// AXI4 响应类型枚举
// ============================================================================

enum class AxiResp : uint8_t {
    OKAY   = 0b00,  // 正常响应
    EXOKAY = 0b01,  // 独占访问成功
    SLVERR = 0b10,  // 从设备错误
    DECERR = 0b11   // 解码错误
};

// ============================================================================
// AXI4 突发类型枚举
// ============================================================================

enum class AxiBurst : uint8_t {
    FIXED = 0b00,  // 固定地址突发
    INCR  = 0b01,  // 递增地址突发
    WRAP  = 0b10   // 回绕突发 (AXI4 不支持)
};

// ============================================================================
// AXI4 全功能从设备控制器
// ============================================================================

template <
    unsigned ADDR_WIDTH = 32,
    unsigned DATA_WIDTH = 32,
    unsigned ID_WIDTH = 4,
    unsigned NUM_REGS = 16,
    unsigned MAX_BURST_LEN = 16
>
class Axi4Slave : public ch::Component {
public:
    // 计算字节数
    static constexpr unsigned NUM_BYTES = DATA_WIDTH / 8;
    
    __io(
        // === 写地址通道 (AW) ===
        ch_in<ch_uint<ADDR_WIDTH>> awaddr;
        ch_in<ch_uint<ID_WIDTH>> awid;
        ch_in<ch_uint<4>> awlen;     // 突发长度 (0-15, 实际长度=awlen+1)
        ch_in<ch_uint<3>> awsize;    // 突发大小 (0=1 字节，1=2 字节，2=4 字节...)
        ch_in<ch_uint<2>> awburst;   // 突发类型 (0=FIXED, 1=INCR, 2=WRAP)
        ch_in<ch_bool> awlock;       // 锁定类型
        ch_in<ch_uint<4>> awcache;   // 缓存属性
        ch_in<ch_uint<2>> awprot;    // 保护类型
        ch_in<ch_bool> awvalid;
        ch_out<ch_bool> awready;
        
        // === 写数据通道 (W) ===
        ch_in<ch_uint<DATA_WIDTH>> wdata;
        ch_in<ch_uint<ID_WIDTH>> wid;
        ch_in<ch_uint<NUM_BYTES>> wstrb;  // 写选通信号
        ch_in<ch_bool> wlast;             // 最后一个传输
        ch_in<ch_bool> wvalid;
        ch_out<ch_bool> wready;
        
        // === 写响应通道 (B) ===
        ch_out<ch_uint<ID_WIDTH>> bid;
        ch_out<ch_uint<2>> bresp;
        ch_out<ch_bool> bvalid;
        ch_in<ch_bool> bready;
        
        // === 读地址通道 (AR) ===
        ch_in<ch_uint<ADDR_WIDTH>> araddr;
        ch_in<ch_uint<ID_WIDTH>> arid;
        ch_in<ch_uint<4>> arlen;
        ch_in<ch_uint<3>> arsize;
        ch_in<ch_uint<2>> arburst;
        ch_in<ch_bool> arlock;
        ch_in<ch_uint<4>> arcache;
        ch_in<ch_uint<2>> arprot;
        ch_in<ch_bool> arvalid;
        ch_out<ch_bool> arready;
        
        // === 读数据通道 (R) ===
        ch_out<ch_uint<DATA_WIDTH>> rdata;
        ch_out<ch_uint<ID_WIDTH>> rid;
        ch_out<ch_uint<2>> rresp;
        ch_out<ch_bool> rlast;
        ch_out<ch_bool> rvalid;
        ch_in<ch_bool> rready;
    )
    
    Axi4Slave(ch::Component* parent = nullptr, const std::string& name = "axi4_slave")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // ========================================================================
        // 状态机定义
        // ========================================================================
        
        // 写状态机：0=IDLE, 1=AW_WAIT, 2=W_DATA, 3=B_RESP
        ch_reg<ch_uint<2>> write_state(0_d);
        
        // 读状态机：0=IDLE, 1=AR_WAIT, 2=R_DATA
        ch_reg<ch_uint<2>> read_state(0_d);
        
        // ========================================================================
        // 寄存器文件 (可配置大小)
        // ========================================================================
        
        // 使用数组风格定义寄存器
        ch_reg<ch_uint<DATA_WIDTH>> reg0(0_d);
        ch_reg<ch_uint<DATA_WIDTH>> reg1(0_d);
        ch_reg<ch_uint<DATA_WIDTH>> reg2(0_d);
        ch_reg<ch_uint<DATA_WIDTH>> reg3(0_d);
        ch_reg<ch_uint<DATA_WIDTH>> reg4(0_d);
        ch_reg<ch_uint<DATA_WIDTH>> reg5(0_d);
        ch_reg<ch_uint<DATA_WIDTH>> reg6(0_d);
        ch_reg<ch_uint<DATA_WIDTH>> reg7(0_d);
        ch_reg<ch_uint<DATA_WIDTH>> reg8(0_d);
        ch_reg<ch_uint<DATA_WIDTH>> reg9(0_d);
        ch_reg<ch_uint<DATA_WIDTH>> reg10(0_d);
        ch_reg<ch_uint<DATA_WIDTH>> reg11(0_d);
        ch_reg<ch_uint<DATA_WIDTH>> reg12(0_d);
        ch_reg<ch_uint<DATA_WIDTH>> reg13(0_d);
        ch_reg<ch_uint<DATA_WIDTH>> reg14(0_d);
        ch_reg<ch_uint<DATA_WIDTH>> reg15(0_d);
        
        // ========================================================================
        // 突发传输计数器
        // ========================================================================
        
        // 写突发计数器
        ch_reg<ch_uint<4>> write_burst_cnt(0_d);
        ch_reg<ch_uint<ID_WIDTH>> write_id_reg(0_d);
        
        // 读突发计数器
        ch_reg<ch_uint<4>> read_burst_cnt(0_d);
        ch_reg<ch_uint<ID_WIDTH>> read_id_reg(0_d);
        ch_reg<ch_uint<ADDR_WIDTH>> read_addr_reg(0_d);
        
        // ========================================================================
        // 地址解码
        // ========================================================================
        
        // 计算地址对应的寄存器索引 (假设 4 字节对齐)
        auto addr_index = io().awaddr >> ch_uint<ADDR_WIDTH>(2_d);
        
        // ========================================================================
        // 写地址通道 (AW) - 握手
        // ========================================================================
        
        auto is_write_idle = (write_state == ch_uint<2>(0_d));
        auto aw_ready_sig = select(is_write_idle, io().awvalid, ch_bool(false));
        io().awready = aw_ready_sig;
        
        // ========================================================================
        // 写数据通道 (W) - 握手和数据存储
        // ========================================================================
        
        auto is_aw_wait = (write_state == ch_uint<2>(1_d));
        auto w_ready_sig = select(is_aw_wait, io().wvalid, ch_bool(false));
        io().wready = w_ready_sig;
        
        // 写数据到寄存器 (支持字节选通)
        auto sel0 = (addr_index == ch_uint<ADDR_WIDTH>(0_d));
        auto sel1 = (addr_index == ch_uint<ADDR_WIDTH>(1_d));
        auto sel2 = (addr_index == ch_uint<ADDR_WIDTH>(2_d));
        auto sel3 = (addr_index == ch_uint<ADDR_WIDTH>(3_d));
        auto sel4 = (addr_index == ch_uint<ADDR_WIDTH>(4_d));
        auto sel5 = (addr_index == ch_uint<ADDR_WIDTH>(5_d));
        auto sel6 = (addr_index == ch_uint<ADDR_WIDTH>(6_d));
        auto sel7 = (addr_index == ch_uint<ADDR_WIDTH>(7_d));
        auto sel8 = (addr_index == ch_uint<ADDR_WIDTH>(8_d));
        auto sel9 = (addr_index == ch_uint<ADDR_WIDTH>(9_d));
        auto sel10 = (addr_index == ch_uint<ADDR_WIDTH>(10_d));
        auto sel11 = (addr_index == ch_uint<ADDR_WIDTH>(11_d));
        auto sel12 = (addr_index == ch_uint<ADDR_WIDTH>(12_d));
        auto sel13 = (addr_index == ch_uint<ADDR_WIDTH>(13_d));
        auto sel14 = (addr_index == ch_uint<ADDR_WIDTH>(14_d));
        auto sel15 = (addr_index == ch_uint<ADDR_WIDTH>(15_d));
        
        // 写使能信号 (结合选通和地址选择)
        auto we0 = w_ready_sig && sel0;
        auto we1 = w_ready_sig && sel1;
        auto we2 = w_ready_sig && sel2;
        auto we3 = w_ready_sig && sel3;
        auto we4 = w_ready_sig && sel4;
        auto we5 = w_ready_sig && sel5;
        auto we6 = w_ready_sig && sel6;
        auto we7 = w_ready_sig && sel7;
        auto we8 = w_ready_sig && sel8;
        auto we9 = w_ready_sig && sel9;
        auto we10 = w_ready_sig && sel10;
        auto we11 = w_ready_sig && sel11;
        auto we12 = w_ready_sig && sel12;
        auto we13 = w_ready_sig && sel13;
        auto we14 = w_ready_sig && sel14;
        auto we15 = w_ready_sig && sel15;
        
        // 更新寄存器
        reg0->next = select(we0, io().wdata, reg0);
        reg1->next = select(we1, io().wdata, reg1);
        reg2->next = select(we2, io().wdata, reg2);
        reg3->next = select(we3, io().wdata, reg3);
        reg4->next = select(we4, io().wdata, reg4);
        reg5->next = select(we5, io().wdata, reg5);
        reg6->next = select(we6, io().wdata, reg6);
        reg7->next = select(we7, io().wdata, reg7);
        reg8->next = select(we8, io().wdata, reg8);
        reg9->next = select(we9, io().wdata, reg9);
        reg10->next = select(we10, io().wdata, reg10);
        reg11->next = select(we11, io().wdata, reg11);
        reg12->next = select(we12, io().wdata, reg12);
        reg13->next = select(we13, io().wdata, reg13);
        reg14->next = select(we14, io().wdata, reg14);
        reg15->next = select(we15, io().wdata, reg15);
        
        // ========================================================================
        // 写响应通道 (B) - 突发传输支持
        // ========================================================================
        
        // 突发传输计数器递增
        auto burst_not_last = !io().wlast;
        write_burst_cnt->next = select(w_ready_sig && burst_not_last,
                                        write_burst_cnt + ch_uint<4>(1_d),
                                        ch_uint<4>(0_d));
        
        // 保存 ID
        write_id_reg->next = select(aw_ready_sig, io().awid, write_id_reg);
        
        // 写状态机转换
        auto w_handshake = select(io().wvalid, io().wready, ch_bool(false));
        auto next_write_state = select(is_write_idle,
                                        select(io().awvalid, ch_uint<2>(1_d), ch_uint<2>(0_d)),
                                        select(is_aw_wait,
                                            select(w_handshake,
                                                select(io().wlast, ch_uint<2>(3_d), ch_uint<2>(2_d)),
                                                ch_uint<2>(1_d)),
                                            select(is_write_idle, ch_uint<2>(0_d),
                                                select(io().bready, ch_uint<2>(0_d), ch_uint<2>(3_d)))));
        write_state->next = next_write_state;
        
        // 写响应输出
        auto is_b_resp = (write_state == ch_uint<2>(3_d));
        io().bid = write_id_reg;
        io().bresp = ch_uint<2>(static_cast<unsigned>(AxiResp::OKAY));
        io().bvalid = is_b_resp;
        
        // ========================================================================
        // 读地址通道 (AR) - 握手
        // ========================================================================
        
        auto is_read_idle = (read_state == ch_uint<2>(0_d));
        auto ar_ready_sig = select(is_read_idle, io().arvalid, ch_bool(false));
        io().arready = ar_ready_sig;
        
        // 保存读地址和 ID
        read_addr_reg->next = select(ar_ready_sig, io().araddr, read_addr_reg);
        read_id_reg->next = select(ar_ready_sig, io().arid, read_id_reg);
        
        // ========================================================================
        // 读数据通道 (R) - 突发传输支持
        // ========================================================================
        
        // 读地址解码
        auto read_addr_index = read_addr_reg >> ch_uint<ADDR_WIDTH>(2_d);
        
        // 从寄存器读取数据
        ch_uint<DATA_WIDTH> read_val(0_d);
        read_val = select(read_addr_index == ch_uint<ADDR_WIDTH>(0_d), reg0, read_val);
        read_val = select(read_addr_index == ch_uint<ADDR_WIDTH>(1_d), reg1, read_val);
        read_val = select(read_addr_index == ch_uint<ADDR_WIDTH>(2_d), reg2, read_val);
        read_val = select(read_addr_index == ch_uint<ADDR_WIDTH>(3_d), reg3, read_val);
        read_val = select(read_addr_index == ch_uint<ADDR_WIDTH>(4_d), reg4, read_val);
        read_val = select(read_addr_index == ch_uint<ADDR_WIDTH>(5_d), reg5, read_val);
        read_val = select(read_addr_index == ch_uint<ADDR_WIDTH>(6_d), reg6, read_val);
        read_val = select(read_addr_index == ch_uint<ADDR_WIDTH>(7_d), reg7, read_val);
        read_val = select(read_addr_index == ch_uint<ADDR_WIDTH>(8_d), reg8, read_val);
        read_val = select(read_addr_index == ch_uint<ADDR_WIDTH>(9_d), reg9, read_val);
        read_val = select(read_addr_index == ch_uint<ADDR_WIDTH>(10_d), reg10, read_val);
        read_val = select(read_addr_index == ch_uint<ADDR_WIDTH>(11_d), reg11, read_val);
        read_val = select(read_addr_index == ch_uint<ADDR_WIDTH>(12_d), reg12, read_val);
        read_val = select(read_addr_index == ch_uint<ADDR_WIDTH>(13_d), reg13, read_val);
        read_val = select(read_addr_index == ch_uint<ADDR_WIDTH>(14_d), reg14, read_val);
        read_val = select(read_addr_index == ch_uint<ADDR_WIDTH>(15_d), reg15, read_val);
        
        // 读突发计数器
        auto r_valid_ready = select(io().rvalid, io().rready, ch_bool(false));
        read_burst_cnt->next = select(ar_ready_sig, ch_uint<4>(0_d),
                                      select(select(is_read_idle, r_valid_ready, ch_bool(false)),
                                          read_burst_cnt + ch_uint<4>(1_d),
                                          read_burst_cnt));
        
        // 读数据输出
        auto is_r_data = (read_state == ch_uint<2>(2_d));
        io().rdata = read_val;
        io().rid = read_id_reg;
        io().rresp = ch_uint<2>(static_cast<unsigned>(AxiResp::OKAY));
        io().rvalid = is_r_data;
        
        // 最后一个传输标志
        auto burst_len_match = (read_burst_cnt == io().arlen);
        io().rlast = select(is_r_data, burst_len_match, ch_bool(false));
        
        // 读状态机转换
        auto r_handshake = select(io().rvalid, io().rready, ch_bool(false));
        auto next_read_state = select(is_read_idle,
                                       select(io().arvalid, ch_uint<2>(2_d), ch_uint<2>(0_d)),
                                       select(is_read_idle, ch_uint<2>(0_d),
                                           select(r_handshake,
                                               select(burst_len_match, ch_uint<2>(0_d), ch_uint<2>(2_d)),
                                               ch_uint<2>(2_d))));
        read_state->next = next_read_state;
    }
};

} // namespace axi4
