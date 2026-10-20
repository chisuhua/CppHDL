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
#include "axi4/byte_lane.h"

namespace axi4 {

using namespace ch::core;

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
        
        // 异步复位 (低电平有效)
        ch_in<ch_bool> rst_n;
    )
    
    AxiLitePwm(ch::Component* parent = nullptr, const std::string& name = "axi_pwm")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // ========================================================================
        // Simple AXI handshake with busy register
        // Supports separated AW/W and AR/R phases
        // ========================================================================
        
        ch_reg<ch_bool> busy(ch_bool(false));
        ch_reg<ch_bool> aw_done(ch_bool(false));
        ch_reg<ch_bool> b_valid(ch_bool(false));   // W 已接收，B 响应待取
        ch_reg<ch_bool> r_valid(ch_bool(false));

        // 地址锁存寄存器：AXI 只要求 AWADDR/ARADDR 保持到对应握手，
        // 握手后 master 即可改写它们；W 可能延后多个周期到达，
        // 因此地址必须在握手时锁存，否则写会落到改变后 AWADDR 指向的寄存器。
        ch_reg<ch_uint<32>> addr_reg(0_d);
        ch_reg<ch_uint<32>> ar_addr_reg(0_d);
        
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
        auto overflow = ch_bool(false);  // 初始化溢出标志
        
        // ====================================================================
        // AXI4-Lite Write Channel
        // ====================================================================
        
        // Write address handshake: accept when !busy
        auto aw_handshake = select(io().awvalid, select(busy, ch_bool(false), ch_bool(true)), ch_bool(false));
        io().awready = aw_handshake;
        
        // Write data handshake: accept when AW was done
        auto w_handshake = select(io().wvalid, aw_done, ch_bool(false));
        io().wready = w_handshake;
        
        // Write response: 由 b_valid 寄存器驱动，WDATA 被接受后才拉高，保持到 BREADY
        io().bvalid = select(b_valid, ch_bool(true), ch_bool(false));

        // Address decode (2-bit, 4-byte aligned) —— 基于**锁存**的地址
        auto wr_addr = addr_reg >> ch_uint<32>(2_d);
        wr_addr = wr_addr & ch_uint<32>(15_d);
        
        // 寄存器选择信号 (使用右移后的地址)
        auto sel_period = (wr_addr == ch_uint<32>(0_d));
        auto sel_duty0 = (wr_addr == ch_uint<32>(4_d));
        auto sel_duty1 = (wr_addr == ch_uint<32>(5_d));
        auto sel_duty2 = (wr_addr == ch_uint<32>(6_d));
        auto sel_duty3 = (wr_addr == ch_uint<32>(7_d));
        auto sel_enable = (wr_addr == ch_uint<32>(8_d));
        auto sel_interrupt = (wr_addr == ch_uint<32>(9_d));

        // 未映射地址 → DECERR
        auto write_mapped = sel_period | sel_duty0 | sel_duty1 | sel_duty2
                          | sel_duty3 | sel_enable | sel_interrupt;
        io().bresp = select(write_mapped, ch_uint<2>(0_d), ch_uint<2>(3_d));
        
        // 写使能信号
        auto we_period = select(w_handshake, sel_period, ch_bool(false));
        auto we_duty0 = select(w_handshake, sel_duty0, ch_bool(false));
        auto we_duty1 = select(w_handshake, sel_duty1, ch_bool(false));
        auto we_duty2 = select(w_handshake, sel_duty2, ch_bool(false));
        auto we_duty3 = select(w_handshake, sel_duty3, ch_bool(false));
        auto we_enable = select(w_handshake, sel_enable, ch_bool(false));
        auto we_interrupt = select(w_handshake, sel_interrupt, ch_bool(false));
        
        // 写寄存器 (带异步复位)
        // 字节写：WSTRB 逐字节展开（语义见 include/axi4/AGENTS.md 字节道语义）
        const auto wstrb_mask = axi4::detail::byte_lane_mask<32>(io().wstrb);
        period_reg->next = select(!io().rst_n, ch_uint<32>(255_d),
                                  select(we_period,
                                         axi4::detail::byte_lane_apply<32>(period_reg, io().wdata, wstrb_mask),
                                         period_reg));
        duty0_reg->next = select(!io().rst_n, ch_uint<32>(128_d),
                                 select(we_duty0,
                                        axi4::detail::byte_lane_apply<32>(duty0_reg, io().wdata, wstrb_mask),
                                        duty0_reg));
        duty1_reg->next = select(!io().rst_n, ch_uint<32>(128_d),
                                 select(we_duty1,
                                        axi4::detail::byte_lane_apply<32>(duty1_reg, io().wdata, wstrb_mask),
                                        duty1_reg));
        duty2_reg->next = select(!io().rst_n, ch_uint<32>(128_d),
                                 select(we_duty2,
                                        axi4::detail::byte_lane_apply<32>(duty2_reg, io().wdata, wstrb_mask),
                                        duty2_reg));
        duty3_reg->next = select(!io().rst_n, ch_uint<32>(128_d),
                                 select(we_duty3,
                                        axi4::detail::byte_lane_apply<32>(duty3_reg, io().wdata, wstrb_mask),
                                        duty3_reg));
        enable_reg->next = select(!io().rst_n, ch_uint<32>(0_d),
                                  select(we_enable,
                                         axi4::detail::byte_lane_apply<32>(enable_reg, io().wdata, wstrb_mask),
                                         enable_reg));
        // interrupt_reg: clear on reset or write, set on overflow
        interrupt_reg->next = select(!io().rst_n, ch_uint<32>(0_d),
                                     select(we_interrupt,
                                            axi4::detail::byte_lane_apply<32>(interrupt_reg, io().wdata, wstrb_mask),
                                            select(overflow, interrupt_reg | ch_uint<32>(1_d), interrupt_reg)));
        
        // Write response (由 b_valid 寄存器驱动，见上)
        
        // ====================================================================
        // AXI4-Lite Read Channel
        // ====================================================================
        
        // Read address decode —— 基于**锁存**的地址
        auto rd_addr = ar_addr_reg >> ch_uint<32>(2_d);
        rd_addr = rd_addr & ch_uint<32>(15_d);
        
        // Read address handshake: accept when !busy
        auto ar_handshake = select(io().arvalid, select(busy, ch_bool(false), ch_bool(true)), ch_bool(false));
        io().arready = ar_handshake;
        
        // Read response
        io().rvalid = select(r_valid, ch_bool(true), ch_bool(false));
        
        // Read register select
        auto read_sel_period = (rd_addr == ch_uint<32>(0_d));
        auto read_sel_duty0 = (rd_addr == ch_uint<32>(4_d));
        auto read_sel_duty1 = (rd_addr == ch_uint<32>(5_d));
        auto read_sel_duty2 = (rd_addr == ch_uint<32>(6_d));
        auto read_sel_duty3 = (rd_addr == ch_uint<32>(7_d));
        auto read_sel_enable = (rd_addr == ch_uint<32>(8_d));
        auto read_sel_interrupt = (rd_addr == ch_uint<32>(9_d));

        // 未映射地址 → DECERR
        auto read_mapped = read_sel_period | read_sel_duty0 | read_sel_duty1
                         | read_sel_duty2 | read_sel_duty3 | read_sel_enable
                         | read_sel_interrupt;
        io().rresp = select(read_mapped, ch_uint<2>(0_d), ch_uint<2>(3_d));
        
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
        
        // ====================================================================
        // PWM 计数器逻辑
        // ====================================================================
        
        // 计数器比较：是否达到周期值
        auto counter_width_period = ch_uint<COUNTER_WIDTH>(period_reg);
        auto counter_max = (counter >= counter_width_period);
        overflow = counter_max;  // 更新溢出标志
        
        // 计数器更新：递增或回绕 (带异步复位)
        counter->next = select(!io().rst_n, ch_uint<COUNTER_WIDTH>(0_d),
                               select(counter_max, 
                                      ch_uint<COUNTER_WIDTH>(0_d),  // 回绕到 0
                                      counter + ch_uint<COUNTER_WIDTH>(1_d)));
        
        // 中断生成：计数器回绕时产生脉冲 (已在前面与写逻辑合并)
        
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
        
        // 事务真正完成的条件（不是裸 bready/rready）
        auto write_done = select(b_valid, io().bready, ch_bool(false));
        auto read_done = select(r_valid, io().rready, ch_bool(false));

        // State update: aw_done (with async reset)
        // aw_done 仅在 B 握手完成时清零 —— 若在裸 bready 上清零，
        // master 常态拉高 BREADY 时 W 永远不会被接受。
        aw_done->next = select(!io().rst_n, ch_bool(false),
                               select(aw_handshake, ch_bool(true),
                                      select(write_done, ch_bool(false), aw_done)));

        // State update: b_valid (with async reset) —— BVALID 保持到 BREADY
        b_valid->next = select(!io().rst_n, ch_bool(false),
                               select(w_handshake, ch_bool(true),
                                      select(io().bready, ch_bool(false), b_valid)));

        // State update: r_valid (with async reset)
        r_valid->next = select(!io().rst_n, ch_bool(false),
                               select(ar_handshake, ch_bool(true), select(io().rready, ch_bool(false), r_valid)));

        // 地址锁存（AW / AR 握手时采样）
        addr_reg->next = select(!io().rst_n, ch_uint<32>(0_d),
                                select(aw_handshake, io().awaddr, addr_reg));
        ar_addr_reg->next = select(!io().rst_n, ch_uint<32>(0_d),
                                   select(ar_handshake, io().araddr, ar_addr_reg));

        // Busy update (with async reset) —— 事务真正完成才释放
        busy->next = select(!io().rst_n, ch_bool(false),
                            select(aw_handshake, ch_bool(true),
                                   select(ar_handshake, ch_bool(true),
                                          select(write_done, ch_bool(false),
                                                 select(read_done, ch_bool(false), busy)))));
    }
};

} // namespace axi4
