/**
 * AXI4-Lite PWM Controller
 * 
 * 4 通道 PWM 控制器，支持独立占空比配置
 * 
 * 寄存器映射:
 * - 0x00: PERIOD      - 周期寄存器 (共用，所有通道)
 * - 0x10: DUTY_CYCLE0 - 通道 0 占空比寄存器
 * - 0x14: DUTY_CYCLE1 - 通道 1 占空比寄存器
 * - 0x18: DUTY_CYCLE2 - 通道 2 占空比寄存器
 * - 0x1C: DUTY_CYCLE3 - 通道 3 占空比寄存器
 * - 0x20: ENABLE      - 使能寄存器 (每通道 1 位，bit0=ch0, bit1=ch1, ...)
 * - 0x24: INTERRUPT   - 中断状态寄存器 (计数器溢出标志)
 * 
 * 工作原理:
 * - 计数器从 0 递增到 PERIOD-1，然后回绕到 0
 * - 每个通道独立比较：当 counter < DUTY_CYCLE 时输出高电平
 * - 使能寄存器控制每个通道的输出使能
 * - 计数器回绕时产生中断脉冲
 */

#pragma once

#include "ch.hpp"
#include "component.h"

using namespace ch::core;

namespace axi4 {

template <unsigned COUNTER_WIDTH = 16, unsigned NUM_CHANNELS = 4>
class AxiLitePwm : public ch::Component {
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
        
        // PWM 物理接口
        ch_out<ch_bool> pwm_out_0;
        ch_out<ch_bool> pwm_out_1;
        ch_out<ch_bool> pwm_out_2;
        ch_out<ch_bool> pwm_out_3;
        
        // 中断输出 (计数器溢出)
        ch_out<ch_bool> irq;
    )
    
    AxiLitePwm(ch::Component* parent = nullptr, const std::string& name = "axi_pwm")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // ========================================================================
        // AXI Transaction State Machine
        // ========================================================================
        
        // AXI write/read state: 0=IDLE, 1=ADDR_WAIT
        ch_reg<ch_uint<2>> axi_wr_state(ch_uint<2>(0_d));
        ch_reg<ch_uint<2>> axi_rd_state(ch_uint<2>(0_d));
        
        // State decode
        auto axi_wr_idle = (axi_wr_state == ch_uint<2>(0_d));
        auto axi_wr_addr = (axi_wr_state == ch_uint<2>(1_d));
        auto axi_rd_idle = (axi_rd_state == ch_uint<2>(0_d));
        auto axi_rd_addr = (axi_rd_state == ch_uint<2>(1_d));
        
        // 配置寄存器
        ch_reg<ch_uint<32>> period_reg(ch_uint<32>(255_d));      // 默认周期 255
        ch_reg<ch_uint<32>> duty0_reg(ch_uint<32>(128_d));       // 默认 50% 占空比
        ch_reg<ch_uint<32>> duty1_reg(ch_uint<32>(128_d));
        ch_reg<ch_uint<32>> duty2_reg(ch_uint<32>(128_d));
        ch_reg<ch_uint<32>> duty3_reg(ch_uint<32>(128_d));
        ch_reg<ch_uint<32>> enable_reg(ch_uint<32>(0_d));        // 默认全关闭
        ch_reg<ch_uint<32>> interrupt_reg(ch_uint<32>(0_d));     // 中断状态
        
        // PWM 计数器
        ch_reg<ch_uint<COUNTER_WIDTH>> counter(0_d);
        
        // ====================================================================
        // AXI4-Lite Write Channel
        // ====================================================================
        
        // Write address handshake
        io().awready = select(axi_wr_idle, io().awvalid, ch_bool(false));
        auto aw_handshake = select(axi_wr_idle, io().awvalid, ch_bool(false));
        
        // Write data handshake
        auto w_handshake = select(axi_wr_addr, io().wvalid, ch_bool(false));
        io().wready = w_handshake;
        
        // Address decode (2-bit, 4-byte aligned)
        auto wr_addr = io().awaddr >> ch_uint<32>(2_d);
        wr_addr = wr_addr & ch_uint<32>(15_d);
        
        // 寄存器选择信号 (使用右移后的地址)
        auto sel_period = (wr_addr == ch_uint<32>(0_d));
        auto sel_duty0 = (wr_addr == ch_uint<32>(4_d));
        auto sel_duty1 = (wr_addr == ch_uint<32>(5_d));
        auto sel_duty2 = (wr_addr == ch_uint<32>(6_d));
        auto sel_duty3 = (wr_addr == ch_uint<32>(7_d));
        auto sel_enable = (wr_addr == ch_uint<32>(8_d));
        auto sel_interrupt = (wr_addr == ch_uint<32>(9_d));
        
        // 写使能信号
        auto we_period = select(w_handshake, sel_period, ch_bool(false));
        auto we_duty0 = select(w_handshake, sel_duty0, ch_bool(false));
        auto we_duty1 = select(w_handshake, sel_duty1, ch_bool(false));
        auto we_duty2 = select(w_handshake, sel_duty2, ch_bool(false));
        auto we_duty3 = select(w_handshake, sel_duty3, ch_bool(false));
        auto we_enable = select(w_handshake, sel_enable, ch_bool(false));
        auto we_interrupt = select(w_handshake, sel_interrupt, ch_bool(false));
        
        // 写寄存器
        period_reg->next = select(we_period, io().wdata, period_reg);
        duty0_reg->next = select(we_duty0, io().wdata, duty0_reg);
        duty1_reg->next = select(we_duty1, io().wdata, duty1_reg);
        duty2_reg->next = select(we_duty2, io().wdata, duty2_reg);
        duty3_reg->next = select(we_duty3, io().wdata, duty3_reg);
        enable_reg->next = select(we_enable, io().wdata, enable_reg);
        interrupt_reg->next = select(we_interrupt, io().wdata, interrupt_reg);
        
        // Write response (combinational)
        io().bvalid = w_handshake;
        io().bresp = ch_uint<2>(0_d);  // OKAY
        
        // ====================================================================
        // AXI4-Lite Read Channel
        // ====================================================================
        
        // Read address decode
        auto rd_addr = io().araddr >> ch_uint<32>(2_d);
        rd_addr = rd_addr & ch_uint<32>(15_d);
        
        // Read handshake
        io().arready = select(axi_rd_idle, io().arvalid, ch_bool(false));
        auto ar_handshake = select(axi_rd_idle, io().arvalid, ch_bool(false));
        
        // Read register select
        auto read_sel_period = (rd_addr == ch_uint<32>(0_d));
        auto read_sel_duty0 = (rd_addr == ch_uint<32>(4_d));
        auto read_sel_duty1 = (rd_addr == ch_uint<32>(5_d));
        auto read_sel_duty2 = (rd_addr == ch_uint<32>(6_d));
        auto read_sel_duty3 = (rd_addr == ch_uint<32>(7_d));
        auto read_sel_enable = (rd_addr == ch_uint<32>(8_d));
        auto read_sel_interrupt = (rd_addr == ch_uint<32>(9_d));
        
        // Read data mux
        ch_uint<32> read_val(0_d);
        read_val = select(read_sel_period, period_reg, read_val);
        read_val = select(read_sel_duty0, duty0_reg, read_val);
        read_val = select(read_sel_duty1, duty1_reg, read_val);
        read_val = select(read_sel_duty2, duty2_reg, read_val);
        read_val = select(read_sel_duty3, duty3_reg, read_val);
        read_val = select(read_sel_enable, enable_reg, read_val);
        read_val = select(read_sel_interrupt, interrupt_reg, read_val);
        
        io().rdata = read_val;
        io().rresp = ch_uint<2>(0_d);  // OKAY
        
        // rvalid: combinational output, valid when in axi_rd_addr state
        io().rvalid = axi_rd_addr;
        
        // ====================================================================
        // PWM 计数器逻辑
        // ====================================================================
        
        // 计数器比较：是否达到周期值
        auto counter_width_period = ch_uint<COUNTER_WIDTH>(period_reg);
        auto counter_max = (counter >= counter_width_period);
        
        // 计数器更新：递增或回绕
        counter->next = select(counter_max, 
                               ch_uint<COUNTER_WIDTH>(0_d),  // 回绕到 0
                               counter + ch_uint<COUNTER_WIDTH>(1_d));
        
        // 中断生成：计数器回绕时产生脉冲
        auto overflow = counter_max;
        interrupt_reg->next = select(overflow, 
                                     interrupt_reg | ch_uint<32>(1_d),  // 设置中断标志
                                     interrupt_reg);
        
        // ====================================================================
        // PWM 比较逻辑 (每通道独立)
        // ====================================================================
        
        // Extract enable bits using bits<> instead of & operator
        auto en_bit0 = bits<0, 0>(enable_reg);
        auto en_bit1 = bits<1, 1>(enable_reg);
        auto en_bit2 = bits<2, 2>(enable_reg);
        auto en_bit3 = bits<3, 3>(enable_reg);
        auto irq_bit = bits<0, 0>(interrupt_reg);
        
        // 通道 0
        auto counter_width_duty0 = ch_uint<COUNTER_WIDTH>(duty0_reg);
        auto cmp0 = (counter < counter_width_duty0);
        io().pwm_out_0 = select(en_bit0 != ch_uint<1>(0_d), cmp0, ch_bool(false));
        
        // 通道 1
        auto counter_width_duty1 = ch_uint<COUNTER_WIDTH>(duty1_reg);
        auto cmp1 = (counter < counter_width_duty1);
        io().pwm_out_1 = select(en_bit1 != ch_uint<1>(0_d), cmp1, ch_bool(false));
        
        // 通道 2
        auto counter_width_duty2 = ch_uint<COUNTER_WIDTH>(duty2_reg);
        auto cmp2 = (counter < counter_width_duty2);
        io().pwm_out_2 = select(en_bit2 != ch_uint<1>(0_d), cmp2, ch_bool(false));
        
        // 通道 3
        auto counter_width_duty3 = ch_uint<COUNTER_WIDTH>(duty3_reg);
        auto cmp3 = (counter < counter_width_duty3);
        io().pwm_out_3 = select(en_bit3 != ch_uint<1>(0_d), cmp3, ch_bool(false));
        
        // 中断输出
        io().irq = irq_bit != ch_uint<1>(0_d);
        
        // ========================================================================
        // AXI Transaction State Machine Transitions (Write)
        // ========================================================================
        
        auto w_resp_handshake = select(axi_wr_addr, io().bready, ch_bool(false));
        
        ch_uint<2> axi_wr_next;
        axi_wr_next = select(axi_wr_idle,
                             select(aw_handshake, ch_uint<2>(1_d), ch_uint<2>(0_d)),
                             select(w_resp_handshake, ch_uint<2>(0_d), ch_uint<2>(1_d)));
        axi_wr_state->next = axi_wr_next;
        
        // ========================================================================
        // AXI Transaction State Machine Transitions (Read)
        // ========================================================================
        
        auto r_handshake = select(axi_rd_addr, io().rready, ch_bool(false));
        
        ch_uint<2> axi_rd_next;
        axi_rd_next = select(axi_rd_idle,
                             select(ar_handshake, ch_uint<2>(1_d), ch_uint<2>(0_d)),
                             select(r_handshake, ch_uint<2>(0_d), ch_uint<2>(1_d)));
        axi_rd_state->next = axi_rd_next;
    }
};

} // namespace axi4
