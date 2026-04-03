/**
 * AXI4-Lite GPIO Controller
 * 
 * 将 Phase 1+2 的 GPIO 封装成 AXI4 从设备
 * 
 * 寄存器映射:
 * - 0x00: DATA_OUT - 数据输出寄存器
 * - 0x04: DATA_IN  - 数据输入寄存器
 * - 0x08: DIRECTION - 方向寄存器 (1=输出，0=输入)
 * - 0x0C: INTERRUPT - 中断状态寄存器
 */

#pragma once

#include "ch.hpp"
#include "component.h"

using namespace ch::core;

namespace axi4 {

template <unsigned NUM_GPIOS = 8>
class AxiLiteGpio : public ch::Component {
public:
    __io(
        // AXI4-Lite 接口
        ch_in<ch_uint<32>> awaddr;
        ch_in<ch_uint<2>> awprot;
        ch_in<ch_bool> awvalid;
        ch_out<ch_bool> awready;
        
        ch_in<ch_uint<32>> wdata;
        ch_in<ch_uint<4>> wstrb;
        ch_in<ch_bool> wvalid;
        ch_out<ch_bool> wready;
        
        ch_out<ch_uint<2>> bresp;
        ch_out<ch_bool> bvalid;
        ch_in<ch_bool> bready;
        
        ch_in<ch_uint<32>> araddr;
        ch_in<ch_uint<2>> arprot;
        ch_in<ch_bool> arvalid;
        ch_out<ch_bool> arready;
        
        ch_out<ch_uint<32>> rdata;
        ch_out<ch_uint<2>> rresp;
        ch_out<ch_bool> rvalid;
        ch_in<ch_bool> rready;
        
        // GPIO 物理接口
        ch_out<ch_uint<NUM_GPIOS>> gpio_out;
        ch_in<ch_uint<NUM_GPIOS>> gpio_in;
        ch_out<ch_uint<NUM_GPIOS>> gpio_dir;
        ch_out<ch_bool> irq;
    )
    
    AxiLiteGpio(ch::Component* parent = nullptr, const std::string& name = "axi_gpio")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 状态机
        ch_reg<ch_bool> busy(ch_bool(false));
        
        // 寄存器
        ch_reg<ch_uint<32>> data_out_reg(0_d);
        ch_reg<ch_uint<32>> direction_reg(0_d);
        ch_reg<ch_uint<32>> interrupt_reg(0_d);
        
        // 地址解码 (2 位，4 字节对齐)
        auto addr_bits = io().awaddr >> ch_uint<32>(2_d);
        addr_bits = addr_bits & ch_uint<32>(3_d);
        
        // 写地址握手
        auto aw_handshake = select(io().awvalid, select(busy, ch_bool(false), ch_bool(true)), ch_bool(false));
        io().awready = aw_handshake;
        
        // 写数据握手
        auto w_handshake = select(io().wvalid, aw_handshake, ch_bool(false));
        io().wready = w_handshake;
        
        // 写寄存器
        auto sel_data_out = (addr_bits == ch_uint<32>(0_d));
        auto sel_direction = (addr_bits == ch_uint<32>(1_d));
        auto sel_interrupt = (addr_bits == ch_uint<32>(2_d));
        
        auto we_data_out = select(w_handshake, sel_data_out, ch_bool(false));
        auto we_direction = select(w_handshake, sel_direction, ch_bool(false));
        auto we_interrupt = select(w_handshake, sel_interrupt, ch_bool(false));
        data_out_reg->next = select(we_data_out, io().wdata, data_out_reg);
        direction_reg->next = select(we_direction, io().wdata, direction_reg);
        interrupt_reg->next = select(we_interrupt, io().wdata, interrupt_reg);
        
        // 写响应
        io().bvalid = w_handshake;
        io().bresp = ch_uint<2>(0_d);  // OKAY
        
        // 读地址握手
        auto ar_addr_bits = io().araddr >> ch_uint<32>(2_d);
        ar_addr_bits = ar_addr_bits & ch_uint<32>(3_d);
        
        auto ar_handshake = select(io().arvalid, select(busy, ch_bool(false), ch_bool(true)), ch_bool(false));
        io().arready = ar_handshake;
        
        // 读数据
        auto read_sel_data_out = (ar_addr_bits == ch_uint<32>(0_d));
        auto read_sel_data_in = (ar_addr_bits == ch_uint<32>(1_d));
        auto read_sel_direction = (ar_addr_bits == ch_uint<32>(2_d));
        auto read_sel_interrupt = (ar_addr_bits == ch_uint<32>(3_d));
        
        ch_uint<32> read_val(0_d);
        read_val = select(read_sel_data_out, data_out_reg, read_val);
        read_val = select(read_sel_data_in, ch_uint<32>(ch_uint<NUM_GPIOS>(io().gpio_in)), read_val);
        read_val = select(read_sel_direction, direction_reg, read_val);
        read_val = select(read_sel_interrupt, interrupt_reg, read_val);
        
        io().rdata = read_val;
        io().rvalid = ar_handshake;
        io().rresp = ch_uint<2>(0_d);
        
        // GPIO 输出
        io().gpio_out = data_out_reg;
        io().gpio_dir = direction_reg;
        io().irq = (interrupt_reg != ch_uint<32>(0_d));
        
        // 状态更新
        auto any_handshake = select(aw_handshake, ch_bool(true), ar_handshake);
        auto any_ready = select(io().bready, ch_bool(true), select(io().rready, ch_bool(true), ch_bool(false)));
        busy->next = select(any_handshake, ch_bool(true), select(any_ready, ch_bool(false), busy));
    }
};

} // namespace axi4
