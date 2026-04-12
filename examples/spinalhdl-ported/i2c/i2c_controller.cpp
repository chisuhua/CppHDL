/**
 * I2C Controller Example - SpinalHDL Port (简化验证版)
 * 
 * 功能验证:
 * - START/STOP 条件生成
 * - 8 位数据移位 (MSB first)
 * - 基本时序
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
// I2C Controller 模块
// ============================================================================

template <unsigned DATA_WIDTH = 8>
class I2CController : public ch::Component {
public:
    __io(
        ch_out<ch_bool> sda;
        ch_out<ch_bool> scl;
        ch_in<ch_bool> sda_in;
        ch_in<ch_uint<DATA_WIDTH>> tx_data;
        ch_in<ch_bool> start;
        ch_out<ch_bool> done;
        ch_out<ch_bool> ack;
        ch_out<ch_uint<DATA_WIDTH>> rx_data;
    )
    
    I2CController(ch::Component* parent = nullptr, const std::string& name = "i2c_ctrl")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 状态机：0=IDLE, 1=START, 2=TRANSFER, 3=ACK, 4=STOP
        ch_reg<ch_uint<3>> state_reg(0_d);
        ch_reg<ch_uint<DATA_WIDTH>> shift_reg(0_d);
        ch_reg<ch_uint<3>> bit_counter(0_d);
        ch_reg<ch_uint<5>> clk_div(0_d);
        ch_reg<ch_bool> sda_out_reg(ch_bool(true));
        ch_reg<ch_bool> scl_out_reg(ch_bool(true));
        
        auto is_idle = (state_reg == ch_uint<3>(0_d));
        auto is_start_state = (state_reg == ch_uint<3>(1_d));
        auto is_transfer = (state_reg == ch_uint<3>(2_d));
        auto is_ack = (state_reg == ch_uint<3>(3_d));
        auto is_stop = (state_reg == ch_uint<3>(4_d));
        
        auto start_sig = io().start;
        auto tx_data_sig = io().tx_data;
        auto sda_in_sig = io().sda_in;
        
        // ==================== 状态机 ====================
        
        auto next_state_idle = select(start_sig, ch_uint<3>(1_d), ch_uint<3>(0_d));
        auto next_state_start = select((clk_div == ch_uint<5>(31_d)), ch_uint<3>(2_d), ch_uint<3>(1_d));
        auto next_state_transfer = select((bit_counter == ch_uint<3>(7_d)),
                                           ch_uint<3>(3_d), ch_uint<3>(2_d));
        auto next_state_ack = select((clk_div == ch_uint<5>(31_d)), ch_uint<3>(4_d), ch_uint<3>(3_d));
        auto next_state_stop = select((clk_div == ch_uint<5>(31_d)), ch_uint<3>(0_d), ch_uint<3>(4_d));
        
        auto next_state = select(is_idle, next_state_idle,
                                 select(is_start_state, next_state_start,
                                        select(is_transfer, next_state_transfer,
                                               select(is_ack, next_state_ack,
                                                      next_state_stop))));
        state_reg->next = next_state;
        
        // ==================== 时钟分频 ====================
        
        auto clk_en = !is_idle;
        clk_div->next = select(clk_en, clk_div + ch_uint<5>(1_d), ch_uint<5>(0_d));
        
        // SCL 输出：IDLE=高，START/TRANSFER/ACK/SOP 期间 toggling
        // clk_div 0-15: SCL=低，16-31: SCL=高
        auto scl_target = select(is_idle, ch_bool(true),
                                 select((clk_div >> ch_uint<5>(4_d)) != ch_uint<5>(0_d),
                                        ch_bool(true), ch_bool(false)));
        
        // SCL 只在 SCL 低时变化 (模拟开漏 + 上拉)
        scl_out_reg->next = scl_target;
        io().scl = scl_out_reg;
        
        // ==================== SDA 输出 ====================
        
        // START 条件：SCL 高时 SDA 从高变低 (在 clk_div=16, SCL 刚变高时拉低 SDA)
        auto start_condition = is_start_state && (clk_div == ch_uint<5>(16_d));
        
        // STOP 条件：SCL 高时 SDA 从低变高 (在 clk_div=16, SCL 刚变高时拉高 SDA)
        auto stop_condition = is_stop && (clk_div == ch_uint<5>(16_d));
        
        // 数据传输：输出 shift_reg 的 MSB
        auto shift_msb = (shift_reg >> ch_uint<DATA_WIDTH>(DATA_WIDTH - 1)) & ch_uint<DATA_WIDTH>(1_d);
        auto sda_transfer = (shift_msb != ch_uint<DATA_WIDTH>(0_d));
        
        // 综合 SDA 目标值
        // START/STOP 条件：立即变化 (SCL 高时)
        // 数据传输：在 SCL 低时变化
        auto sda_immediate = select(start_condition, ch_bool(false),
                                    select(stop_condition, ch_bool(true), sda_out_reg));
        auto sda_delayed = select(is_transfer, sda_transfer,
                                  select(is_ack, ch_bool(true),
                                         select(is_idle, ch_bool(true), sda_out_reg)));
        
        // START/STOP 立即变化，数据传输在 SCL 低时变化
        auto scl_low = (io().scl == ch_bool(false));
        auto sda_target = select(start_condition || stop_condition, sda_immediate,
                                 select(scl_low, sda_delayed, sda_out_reg));
        sda_out_reg->next = sda_target;
        io().sda = sda_out_reg;
        
        // ==================== 移位寄存器 ====================
        
        // I2C 正确时序:
        // - SCL 上升沿 (clk_div=16): 从机采样 SDA
        // - SCL 下降沿 (clk_div=0): 主机改变 SDA (移位)
        
        // 在 START 状态结束时加载数据 (clk_div=31)
        auto load_data = is_start_state && (clk_div == ch_uint<5>(31_d));
        
        // SCL 下降沿 (clk_div=0) 移位 - 为下一个上升沿采样准备数据
        auto sclk_falling = (clk_div == ch_uint<5>(0_d));
        auto shift_en = is_transfer && sclk_falling;
        
        // SCL 上升沿 (clk_div=16) 采样接收数据
        auto sclk_rising = (clk_div == ch_uint<5>(16_d));
        auto sample_en = is_transfer && sclk_rising;
        
        // 位计数 (在采样时递增)
        auto next_bit = select(load_data, ch_uint<3>(0_d),
                               select(sample_en && (bit_counter != ch_uint<3>(7_d)),
                                      bit_counter + ch_uint<3>(1_d), bit_counter));
        bit_counter->next = next_bit;
        
        // 发送移位寄存器：下降沿移位
        auto miso_uint = select(sda_in_sig, ch_uint<DATA_WIDTH>(1_d), ch_uint<DATA_WIDTH>(0_d));
        auto shifted = (shift_reg << ch_uint<DATA_WIDTH>(1_d)) | miso_uint;
        shift_reg->next = select(load_data, tx_data_sig,
                                 select(shift_en, shifted, shift_reg));
        
        // 接收寄存器：上升沿采样 (单独存储，避免被移位覆盖)
        ch_reg<ch_uint<DATA_WIDTH>> rx_reg(0_d);
        auto rx_shifted = (rx_reg << ch_uint<DATA_WIDTH>(1_d)) | miso_uint;
        rx_reg->next = select(load_data, ch_uint<DATA_WIDTH>(0_d),
                              select(sample_en, rx_shifted, rx_reg));
        
        io().rx_data = rx_reg;
        
        // ==================== ACK ====================
        
        // 在 ACK 状态 clk_div=16 时采样 SDA
        io().ack = select(is_ack && (clk_div == ch_uint<5>(16_d)), sda_in_sig, ch_bool(false));
        
        // ==================== DONE ====================
        
        io().done = is_stop && (clk_div == ch_uint<5>(31_d));
    }
};

// ============================================================================
// 顶层模块
// ============================================================================

class I2cTop : public ch::Component {
public:
    __io(
        ch_out<ch_bool> sda;
        ch_out<ch_bool> scl;
        ch_in<ch_bool> sda_in;
        ch_out<ch_bool> done;
        ch_out<ch_bool> ack;
        ch_in<ch_uint<8>> tx_data;
        ch_in<ch_bool> start;
        ch_out<ch_uint<8>> rx_data;
    )
    
    I2cTop(ch::Component* parent = nullptr, const std::string& name = "i2c_top")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        ch::ch_module<I2CController<8>> i2c_inst{"i2c_inst"};
        
        i2c_inst.io().tx_data <<= io().tx_data;
        i2c_inst.io().start <<= io().start;
        i2c_inst.io().sda_in <<= io().sda_in;
        io().sda <<= i2c_inst.io().sda;
        io().scl <<= i2c_inst.io().scl;
        io().done <<= i2c_inst.io().done;
        io().ack <<= i2c_inst.io().ack;
        io().rx_data <<= i2c_inst.io().rx_data;
    }
};

// ============================================================================
// 主函数
// ============================================================================

int main() {
    std::cout << "CppHDL vs SpinalHDL - I2C Controller Example" << std::endl;
    std::cout << "============================================" << std::endl;
    
    ch::ch_device<I2cTop> top_device;
    ch::Simulator sim(top_device.context());
    
    sim.set_input_value(top_device.instance().io().tx_data, 0);
    sim.set_input_value(top_device.instance().io().start, false);
    sim.set_input_value(top_device.instance().io().sda_in, 1);
    
    std::cout << "\n=== Test 1: Single Byte Transfer (Loopback) ===" << std::endl;
    std::cout << "Sending: 0x5A (01011010)" << std::endl;
    
    sim.set_input_value(top_device.instance().io().tx_data, 0x5A);
    sim.set_input_value(top_device.instance().io().start, true);
    
    for (int cycle = 0; cycle < 3000; ++cycle) {
        sim.tick();
        
        auto sda = sim.get_port_value(top_device.instance().io().sda);
        auto scl = sim.get_port_value(top_device.instance().io().scl);
        auto done = sim.get_port_value(top_device.instance().io().done);
        
        sim.set_input_value(top_device.instance().io().sda_in, static_cast<uint64_t>(sda));
        
        if (cycle < 100 || cycle % 500 == 0 || static_cast<uint64_t>(done)) {
            std::cout << "Cycle " << cycle << ": scl=" << static_cast<uint64_t>(scl)
                      << ", sda=" << static_cast<uint64_t>(sda)
                      << ", done=" << static_cast<uint64_t>(done) << std::endl;
        }
        
        if (static_cast<uint64_t>(done)) {
            auto rx_data = sim.get_port_value(top_device.instance().io().rx_data);
            std::cout << "Transfer complete! RX data: 0x" << std::hex
                      << static_cast<uint64_t>(rx_data) << std::dec << std::endl;
            if (static_cast<uint64_t>(rx_data) != 0x5A) {
                std::cerr << "FAILURE: Expected 0x5A, got 0x" << std::hex << static_cast<uint64_t>(rx_data) << std::dec << std::endl;
                return 1;
            }
            break;
        }
    }
    
    std::cout << "\n=== Generating Verilog ===" << std::endl;
    toVerilog("i2c_controller.v", top_device.context());
    std::cout << "Verilog generated: i2c_controller.v" << std::endl;
    
    std::cout << "\nI2C Controller test completed!" << std::endl;
    return 0;
}
