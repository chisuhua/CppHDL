/**
 * WS2812 LED Controller Example - SpinalHDL Port
 * 
 * 功能:
 * - WS2812B LED 时序生成
 * - 24 位颜色数据串行输出 (GRB 顺序)
 * - 可配置 LED 数量
 * - 50μs 复位时间控制
 * 
 * WS2812B 时序 (T0H/T0L/T1H/T1L):
 * - T0H: 0.4μs (高电平，表示 0)
 * - T0L: 0.85μs (低电平)
 * - T1H: 0.8μs (高电平，表示 1)
 * - T1L: 0.45μs (低电平)
 * - 周期：1.25μs (800kHz)
 * - 复位：>50μs 低电平
 */

#include "ch.hpp"
#include "component.h"
#include "module.h"
#include "simulator.h"
#include "codegen_verilog.h"
#include <iostream>

using namespace ch;
using namespace ch::core;

// ============================================================================
// WS2812 Controller 模块
// ============================================================================

template <unsigned NUM_LEDS = 4>
class WS2812Controller : public ch::Component {
public:
    __io(
        // WS2812 数据输出
        ch_out<ch_bool> dout;
        
        // 控制接口
        ch_in<ch_bool> start;          // 启动传输
        ch_in<ch_uint<24>> led_data[NUM_LEDS];  // 每个 LED 24 位颜色 (GRB)
        ch_out<ch_bool> done;          // 传输完成
        ch_out<ch_uint<16>> bit_cnt;   // 当前位计数 (调试)
    )
    
    WS2812Controller(ch::Component* parent = nullptr, const std::string& name = "ws2812_ctrl")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 状态机：0=IDLE, 1=RESET, 2=TRANSMIT, 3=DONE
        ch_reg<ch_uint<2>> state_reg(0_d);
        
        // 位计数器 (NUM_LEDS * 24 bits)
        constexpr unsigned TOTAL_BITS = NUM_LEDS * 24;
        ch_reg<ch_uint<16>> bit_cnt(0_d);
        
        // 时序计数器 (假设系统时钟 80MHz, 1 周期=12.5ns)
        // T0H = 0.4μs = 32 周期
        // T1H = 0.8μs = 64 周期
        ch_reg<ch_uint<8>> tick_cnt(0_d);
        
        // 当前数据位
        ch_reg<ch_bool> current_bit(0_d);
        
        auto is_idle = (state_reg == ch_uint<2>(0_d));
        auto is_reset = (state_reg == ch_uint<2>(1_d));
        auto is_transmit = (state_reg == ch_uint<2>(2_d));
        auto is_done = (state_reg == ch_uint<2>(3_d));
        
        // ==================== 状态机 ====================
        
        // IDLE -> RESET (当 start 信号 asserted)
        auto next_state_idle = select(io().start, ch_uint<2>(1_d), ch_uint<2>(0_d));
        
        // RESET -> TRANSMIT (tick_cnt 达到 100)
        auto reset_done = (tick_cnt == ch_uint<8>(100_d));
        auto next_state_reset = select(reset_done, ch_uint<2>(2_d), ch_uint<2>(1_d));
        
        // TRANSMIT -> DONE (所有位传输完成)
        auto transmit_done = (bit_cnt == ch_uint<16>(TOTAL_BITS - 1));
        auto next_state_transmit = select(transmit_done && (tick_cnt == ch_uint<8>(100_d)),
                                           ch_uint<2>(3_d), ch_uint<2>(2_d));
        
        // DONE -> IDLE
        auto next_state_done = ch_uint<2>(0_d);
        
        auto next_state = select(is_idle, next_state_idle,
                                 select(is_reset, next_state_reset,
                                        select(is_transmit, next_state_transmit,
                                               next_state_done)));
        state_reg->next = next_state;
        
        // ==================== 时序计数器 ====================
        
        // tick_cnt 在 RESET 和 TRANSMIT 状态递增
        auto tick_advance = (tick_cnt == ch_uint<8>(100_d));
        tick_cnt->next = select(!is_reset && !is_transmit, ch_uint<8>(0_d),
                                select(tick_advance, ch_uint<8>(0_d),
                                       tick_cnt + ch_uint<8>(1_d)));
        
        // ==================== 位计数器 ====================
        
        auto bit_advance = is_transmit && (tick_cnt == ch_uint<8>(100_d));
        auto next_bit_cnt = select(is_reset, ch_uint<16>(0_d),
                                   select(bit_advance && (bit_cnt != ch_uint<16>(TOTAL_BITS-1)),
                                          bit_cnt + ch_uint<16>(1_d), bit_cnt));
        bit_cnt->next = next_bit_cnt;
        
        io().bit_cnt = bit_cnt;
        
        // ==================== 数据位提取 ====================
        
        // 根据 bit_cnt 选择 LED 和位位置
        // 简化：使用第一个 LED 的数据
        auto led_data_0 = io().led_data[0];
        auto bit_pos = ch_uint<5>(23_d) - (bit_cnt % ch_uint<16>(24_d));
        current_bit->next = select(is_reset, ch_bool(false),
                                   (led_data_0 >> bit_pos) & ch_uint<24>(1_d));
        
        // ==================== WS2812 输出 ====================
        
        // 根据 current_bit 生成时序
        // bit=0: T0H=32 周期高，T0L=68 周期低
        // bit=1: T1H=64 周期高，T1L=36 周期低
        auto is_t0h = (tick_cnt < ch_uint<8>(32_d));
        auto is_t1h = (tick_cnt < ch_uint<8>(64_d));
        
        auto dout_val = select(is_transmit,
                               select(current_bit, is_t1h, is_t0h),
                               ch_bool(false));
        io().dout = dout_val;
        
        // ==================== DONE 信号 ====================
        
        io().done = is_done;
    }
};

// ============================================================================
// 顶层模块
// ============================================================================

class WS2812Top : public ch::Component {
public:
    __io(
        ch_out<ch_bool> dout;
        ch_out<ch_bool> done;
        ch_in<ch_bool> start;
        ch_in<ch_uint<24>> led0;
        ch_out<ch_uint<16>> bit_cnt;
    )
    
    WS2812Top(ch::Component* parent = nullptr, const std::string& name = "ws2812_top")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        ch::ch_module<WS2812Controller<4>> ws2812{"ws2812"};
        
        ws2812.io().start <<= io().start;
        ws2812.io().led_data[0] <<= io().led0;
        io().dout <<= ws2812.io().dout;
        io().done <<= ws2812.io().done;
        io().bit_cnt <<= ws2812.io().bit_cnt;
    }
};

// ============================================================================
// 主函数 - 仿真测试
// ============================================================================

int main() {
    std::cout << "CppHDL vs SpinalHDL - WS2812 LED Controller" << std::endl;
    std::cout << "===========================================" << std::endl;
    std::cout << "WS2812B Timing: 800kHz, T0H=0.4us, T1H=0.8us" << std::endl;
    
    ch::ch_device<WS2812Top> top_device;
    ch::Simulator sim(top_device.context());
    
    // 初始化：LED0 = GRB(0x00, 0xFF, 0x00) = 绿色
    // GRB 顺序：G=0x00, R=0xFF, B=0x00 → 24 位 = 0x00FF00 = 65280
    sim.set_input_value(top_device.instance().io().start, false);
    sim.set_input_value(top_device.instance().io().led0, 65280_d);
    
    std::cout << "\n=== Test 1: Transmission ===" << std::endl;
    std::cout << "LED0 color: GRB(0x00, 0xFF, 0x00) = Green" << std::endl;
    
    // 启动传输
    sim.set_input_value(top_device.instance().io().start, true);
    sim.tick();
    sim.set_input_value(top_device.instance().io().start, false);
    
    // 运行直到完成
    unsigned tick_count = 0;
    bool done_seen = false;
    for (int cycle = 0; cycle < 10000; ++cycle) {
        sim.tick();
        tick_count++;
        
        auto done = sim.get_port_value(top_device.instance().io().done);
        auto dout = sim.get_port_value(top_device.instance().io().dout);
        auto bit_cnt = sim.get_port_value(top_device.instance().io().bit_cnt);
        
        if (static_cast<uint64_t>(done) && !done_seen) {
            done_seen = true;
            std::cout << "Transmission complete at cycle " << cycle << std::endl;
            std::cout << "Total ticks: " << tick_count << std::endl;
            break;
        }
        
        // 打印前几个周期
        if (cycle < 20) {
            std::cout << "Cyc " << cycle << ": bit_cnt=" << static_cast<uint64_t>(bit_cnt)
                      << ", dout=" << static_cast<uint64_t>(dout) << std::endl;
        }
    }
    
    std::cout << "\n=== Generating Verilog ===" << std::endl;
    toVerilog("ws2812_controller.v", top_device.context());
    std::cout << "Verilog generated: ws2812_controller.v" << std::endl;
    
    if (done_seen) {
        std::cout << "\n✅ WS2812 Controller test PASSED!" << std::endl;
    } else {
        std::cout << "\n❌ WS2812 Controller test FAILED (timeout)!" << std::endl;
    }
    
    return 0;
}
