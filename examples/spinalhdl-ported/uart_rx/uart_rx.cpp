/**
 * UART RX - 简化调试版
 */

#include "ch.hpp"
#include "component.h"
#include "simulator.h"
#include "codegen_verilog.h"
#include <iostream>

using namespace ch;
using namespace ch::core;

class UartRx : public ch::Component {
public:
    __io(
        ch_in<ch_bool> rx;
        ch_in<ch_uint<16>> prescale;
        ch_out<ch_uint<8>> data;
        ch_out<ch_bool> valid;
    )
    
    UartRx(ch::Component* p = nullptr, const std::string& n = "uart_rx")
        : ch::Component(p, n) {}
    
    void create_ports() override { new (io_storage_) io_type; }
    
    void describe() override {
        ch_reg<ch_bool> idle(ch_bool(true));
        ch_reg<ch_uint<8>> shift(0_d);
        ch_reg<ch_uint<4>> cnt(0_d);
        ch_reg<ch_uint<16>> div(0_d);
        ch_reg<ch_bool> rx_d(ch_bool(true));
        
        rx_d->next = io().rx;
        
        // 起始位检测：下降沿 (rx_d=高 且 rx=低)
        auto start = idle && rx_d && (io().rx == ch_bool(false));
        
        // 采样点：每位中间 (div == prescale/2)
        // prescale=9, prescale/2=4, 所以采样点在 div=4
        auto sample_point = (div == ch_uint<16>(4_d));
        
        // 分频器 - 只在 start 时重置
        auto div_done = (div == io().prescale);
        div->next = select(start, ch_uint<16>(0_d),
                           select(div_done, ch_uint<16>(0_d), div + ch_uint<16>(1_d)));
        
        // 位计数：0-8 (起始位 +8 数据位)
        // cnt=0: 起始位, cnt=1-8: bit0-bit7
        auto cnt_done = (cnt == ch_uint<4>(8_d));
        auto cnt_inc = cnt + ch_uint<4>(1_d);
        auto not_idle = (idle == ch_bool(false));
        
        // 在 sample_point 时计数和采样
        auto sample = not_idle && sample_point;
        auto sample_and_not_done = sample && (!cnt_done);
        auto next_cnt = select(start, ch_uint<4>(0_d),
                               select(sample_and_not_done, cnt_inc, cnt));
        cnt->next = next_cnt;
        
        // 移位 - LSB 优先：左移，新数据从 LSB 进入
        // cnt=1-8 时移位 (bit0-bit7)
        auto rx_bit = select(io().rx, ch_uint<8>(1_d), ch_uint<8>(0_d));
        auto shifted = shift << ch_uint<8>(1_d);
        auto new_shift = shifted | rx_bit;
        auto cnt_ge_1 = (cnt != ch_uint<4>(0_d));
        shift->next = select(start, ch_uint<8>(0_d),
                             select(sample && cnt_ge_1, new_shift, shift));
        
        // 状态转换：9 个采样点完成后回到 IDLE
        // done 在 cnt=8 且 sample 时触发，valid 延迟一周期
        auto done = sample && cnt_done;
        idle->next = select(start, ch_bool(false), select(done, ch_bool(true), idle));
        
        io().data = shift;
        
        // valid 延迟一周期，确保数据已更新
        ch_reg<ch_bool> valid_reg(ch_bool(false));
        valid_reg->next = done;
        io().valid = valid_reg;
    }
};

class Top : public ch::Component {
public:
    __io(
        ch_in<ch_bool> rx;
        ch_out<ch_uint<8>> data;
        ch_out<ch_bool> valid;
        ch_in<ch_uint<16>> prescale;
    )
    Top(ch::Component* p = nullptr, const std::string& n = "top") : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        ch::ch_module<UartRx> u{"u"};
        u.io().rx <<= io().rx;
        u.io().prescale <<= io().prescale;
        io().data <<= u.io().data;
        io().valid <<= u.io().valid;
    }
};

int main() {
    std::cout << "UART RX Test\n";
    
    ch::ch_device<Top> top;
    ch::Simulator sim(top.context());
    
    sim.set_input_value(top.instance().io().rx, true);
    sim.set_input_value(top.instance().io().prescale, 9_d);
    
    std::cout << "Running simulation...\n";
    
    // 在仿真过程中发送数据
    for (int c = 0; c < 300; ++c) {
        // 确定当前 RX 值
        int rx_val = 1;  // 默认空闲
        if (c >= 20 && c < 30) rx_val = 0;  // 起始位
        else if (c >= 30 && c < 40) rx_val = 0;  // bit0=0
        else if (c >= 40 && c < 50) rx_val = 1;  // bit1=1
        else if (c >= 50 && c < 60) rx_val = 0;  // bit2=0
        else if (c >= 60 && c < 70) rx_val = 1;  // bit3=1
        else if (c >= 70 && c < 80) rx_val = 1;  // bit4=1
        else if (c >= 80 && c < 90) rx_val = 0;  // bit5=0
        else if (c >= 90 && c < 100) rx_val = 1;  // bit6=1
        else if (c >= 100 && c < 110) rx_val = 0;  // bit7=0
        else if (c >= 110) rx_val = 1;  // 停止位
        
        // 在周期边界打印
        if (c == 20) std::cout << "Start bit...\n";
        else if (c == 30) std::cout << "bit0=0...\n";
        else if (c == 40) std::cout << "bit1=1...\n";
        else if (c == 50) std::cout << "bit2=0...\n";
        else if (c == 60) std::cout << "bit3=1...\n";
        else if (c == 70) std::cout << "bit4=1...\n";
        else if (c == 80) std::cout << "bit5=0...\n";
        else if (c == 90) std::cout << "bit6=1...\n";
        else if (c == 100) std::cout << "bit7=0...\n";
        else if (c == 110) std::cout << "Stop bit...\n";
        
        sim.set_input_value(top.instance().io().rx, rx_val);
        sim.tick();
        
        auto v = sim.get_port_value(top.instance().io().valid);
        auto d = sim.get_port_value(top.instance().io().data);
        if (c < 150 || static_cast<uint64_t>(v)) {
            std::cout << "Cyc " << c << ": v=" << static_cast<uint64_t>(v)
                      << " d=0x" << std::hex << static_cast<uint64_t>(d) << std::dec << "\n";
        }
        if (static_cast<uint64_t>(v)) {
            std::cout << (static_cast<uint64_t>(d) == 0x5A ? "✅ PASS" : "❌ FAIL") << "\n";
            goto done;
        }
    }
    std::cout << "❌ Timeout\n";
done:
    
    toVerilog("uart_rx.v", top.context());
    std::cout << "Done\n";
    return 0;
}
