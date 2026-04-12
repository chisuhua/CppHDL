/**
 * SPI Controller Example - SpinalHDL Port
 * 
 * 对应 SpinalHDL SPI Controller：
 * @code{.scala}
 * class SPIMaster extends Component {
 *   val io = new Bundle {
 *     val mosi = out Bool()
 *     val miso = in Bool()
 *     val sclk = out Bool()
 *     val cs = out Bool()
 *     val data = in UInt(8 bits)
 *     val start = in Bool()
 *     val done = out Bool()
 *   }
 *   
 *   // 状态机：IDLE -> START -> TRANSFER -> STOP
 *   // 移位寄存器：发送/接收 8 位数据
 * }
 * @endcode
 * 
 * CppHDL 移植版本 - 状态机 + 移位寄存器
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
// SPI 状态定义
// ============================================================================

enum class SpiState : uint8_t {
    IDLE,       // 空闲状态
    START,      // 拉低 CS
    TRANSFER,   // 传输 8 位数据
    STOP        // 拉高 CS，完成
};

// ============================================================================
// SPI Controller 模块
// ============================================================================

template <unsigned DATA_WIDTH = 8>
class SPIController : public ch::Component {
public:
    __io(
        // SPI 接口
        ch_out<ch_bool> mosi;       // Master Out Slave In
        ch_in<ch_bool> miso;        // Master In Slave Out
        ch_out<ch_bool> sclk;       // Serial Clock
        ch_out<ch_bool> cs;         // Chip Select (active low)
        
        // 控制接口
        ch_in<ch_uint<DATA_WIDTH>> tx_data;  // 发送数据
        ch_in<ch_bool> start;                // 启动传输
        ch_out<ch_bool> done;                // 传输完成
        ch_out<ch_uint<DATA_WIDTH>> rx_data; // 接收数据
    )
    
    SPIController(ch::Component* parent = nullptr, const std::string& name = "spi_ctrl")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 状态机寄存器
        ch_reg<ch_uint<2>> state_reg(0_d);
        
        // 移位寄存器
        ch_reg<ch_uint<DATA_WIDTH>> shift_reg(0_d);
        
        // 位计数器 (0-7)
        ch_reg<ch_uint<3>> bit_counter(0_d);
        
        // SPI 时钟分频计数器
        ch_reg<ch_uint<4>> clk_divider(0_d);
        
        // 状态解码
        auto is_idle = (state_reg == ch_uint<2>(0_d));
        auto is_start = (state_reg == ch_uint<2>(1_d));
        auto is_transfer = (state_reg == ch_uint<2>(2_d));
        auto is_stop = (state_reg == ch_uint<2>(3_d));
        
        // ==================================================================
        // 状态机逻辑
        // ==================================================================
        
        // 先将 IO 端口赋值给局部变量
        auto start_sig = io().start;
        auto miso_sig = io().miso;
        auto tx_data_sig = io().tx_data;
        
        // IDLE 状态
        auto next_state_idle = select(start_sig, 
                                       ch_uint<2>(1_d),  // 转到 START
                                       ch_uint<2>(0_d)); // 保持 IDLE
        
        // START 状态 - 保持 1 个周期用于加载数据，然后转到 TRANSFER
        auto next_state_start = ch_uint<2>(2_d);  // 转到 TRANSFER
        
        // TRANSFER 状态 - 传输 8 位数据 (bit_counter 0-7)
        auto next_state_transfer = select((bit_counter == ch_uint<3>(7_d)),
                                           ch_uint<2>(3_d),  // 转到 STOP
                                           ch_uint<2>(2_d)); // 保持 TRANSFER
        
        // STOP 状态
        auto next_state_stop = ch_uint<2>(0_d);  // 转到 IDLE
        
        // 状态更新
        auto next_state = select(is_idle, next_state_idle,
                                 select(is_start, next_state_start,
                                        select(is_transfer, next_state_transfer,
                                               next_state_stop)));
        state_reg->next = next_state;
        
        // 数据加载信号 - 仅在 START 状态加载
        auto load_data = is_start;
        
        // ==================================================================
        // 输出逻辑
        // ==================================================================
        
        // CS: START 和 TRANSFER 状态为低
        io().cs = select(is_idle, ch_bool(true), ch_bool(false));
        
        // MOSI: 输出移位寄存器的 MSB
        // 在 START 状态直接输出 tx_data 的 MSB（因为 shift_reg 还没更新）
        // 在 TRANSFER 状态输出 shift_reg 的 MSB
        auto tx_msb = (tx_data_sig >> ch_uint<DATA_WIDTH>(DATA_WIDTH - 1)) & ch_uint<DATA_WIDTH>(1_d);
        auto shift_msb = (shift_reg >> ch_uint<DATA_WIDTH>(DATA_WIDTH - 1)) & ch_uint<DATA_WIDTH>(1_d);
        auto mosi_bit = select(is_start, tx_msb, shift_msb);
        io().mosi = select((mosi_bit != ch_uint<DATA_WIDTH>(0_d)), ch_bool(true), ch_bool(false));
        
        // SCLK: TRANSFER 状态时分频输出
        // 修改相位：让 SCLK 在 clk_divider=0-7 为高，8-15 为低
        // 这样上升沿在 clk_divider=0，下降沿在 clk_divider=8
        clk_divider->next = select(is_transfer, 
                                    clk_divider + ch_uint<4>(1_d),
                                    ch_uint<4>(0_d));
        auto sclk_bit = (clk_divider >> ch_uint<4>(3_d));  // 最高位
        io().sclk = select(is_transfer,
                           select((sclk_bit == ch_uint<4>(0_d)), ch_bool(true), ch_bool(false)),
                           ch_bool(false));
        
        // DONE: STOP 状态脉冲
        io().done = is_stop;
        
        // ==================================================================
        // 移位寄存器逻辑 (load_data 已在上面定义)
        // ==================================================================
        
        // SCLK 波形：0-7 为高，8-15 为低（反相后）
        // 上升沿在 clk_divider==0 (采样点)，下降沿在 clk_divider==8 (移位点)
        
        // 移位 (TRANSFER 状态 + SCLK 下降沿 - 在采样之后)
        auto sclk_falling = (clk_divider == ch_uint<4>(8_d));
        auto shift_enable = is_transfer && sclk_falling;
        
        // 位计数 (在 SCLK 上升沿采样时递增)
        auto sclk_rising = (clk_divider == ch_uint<4>(0_d));
        auto sample_enable = is_transfer && sclk_rising;
        auto next_bit_counter = select(load_data,
                                        ch_uint<3>(0_d),
                                        select(sample_enable && (bit_counter != ch_uint<3>(7_d)),
                                               bit_counter + ch_uint<3>(1_d),
                                               bit_counter));
        bit_counter->next = next_bit_counter;
        
        // 移位寄存器更新 - 在 SCLK 下降沿移位，为下一个上升沿采样做准备
        // miso_sig 是 ch_bool，需要转换为 ch_uint<DATA_WIDTH>
        auto miso_uint = select(miso_sig, ch_uint<DATA_WIDTH>(1_d), ch_uint<DATA_WIDTH>(0_d));
        auto shifted_data = (shift_reg << ch_uint<DATA_WIDTH>(1_d)) | miso_uint;
        shift_reg->next = select(load_data, tx_data_sig,
                                 select(shift_enable, shifted_data, shift_reg));
        
        // 接收数据输出
        io().rx_data = shift_reg;
    }
};

// ============================================================================
// 顶层模块
// ============================================================================

class SpiTop : public ch::Component {
public:
    __io(
        ch_out<ch_bool> mosi;
        ch_in<ch_bool> miso;
        ch_out<ch_bool> sclk;
        ch_out<ch_bool> cs;
        ch_out<ch_bool> done;
        ch_in<ch_uint<8>> tx_data;
        ch_in<ch_bool> start;
        ch_out<ch_uint<8>> rx_data;
    )
    
    SpiTop(ch::Component* parent = nullptr, const std::string& name = "spi_top")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        ch::ch_module<SPIController<8>> spi_inst{"spi_inst"};
        
        spi_inst.io().tx_data <<= io().tx_data;
        spi_inst.io().start <<= io().start;
        spi_inst.io().miso <<= io().miso;
        io().mosi <<= spi_inst.io().mosi;
        io().sclk <<= spi_inst.io().sclk;
        io().cs <<= spi_inst.io().cs;
        io().done <<= spi_inst.io().done;
        io().rx_data <<= spi_inst.io().rx_data;
    }
};

// ============================================================================
// 主函数 - 仿真测试
// ============================================================================

int main() {
    std::cout << "CppHDL vs SpinalHDL - SPI Controller Example" << std::endl;
    std::cout << "============================================" << std::endl;
    
    ch::ch_device<SpiTop> top_device;
    ch::Simulator sim(top_device.context());
    
    // 初始化输入
    sim.set_input_value(top_device.instance().io().tx_data, 0);
    sim.set_input_value(top_device.instance().io().start, false);
    sim.set_input_value(top_device.instance().io().miso, 0);
    
    std::cout << "\n=== Test 1: Single Byte Transfer (Loopback) ===" << std::endl;
    std::cout << "Sending: 0xAA (10101010)" << std::endl;
    std::cout << "MISO tied to MOSI (loopback mode)" << std::endl;
    std::cout << "Expected: 8 bits, MSB first, SPI Mode 0 (CPOL=0, CPHA=0)" << std::endl;
    
    sim.set_input_value(top_device.instance().io().tx_data, 0xAA);
    sim.set_input_value(top_device.instance().io().start, true);
    
    // 运行直到完成
    bool transfer_started = false;
    for (int cycle = 0; cycle < 300; ++cycle) {
        sim.tick();
        
        // 读取当前输出
        auto mosi_val = sim.get_port_value(top_device.instance().io().mosi);
        auto sclk = sim.get_port_value(top_device.instance().io().sclk);
        auto cs = sim.get_port_value(top_device.instance().io().cs);
        auto done = sim.get_port_value(top_device.instance().io().done);
        
        // Loopback: MISO = MOSI (模拟从设备回环)
        sim.set_input_value(top_device.instance().io().miso, static_cast<uint64_t>(mosi_val));
        
        // 详细打印前 20 个周期和 SCLK 边沿
        auto miso_val = sim.get_port_value(top_device.instance().io().miso);
        if (cycle < 20 || static_cast<uint64_t>(done)) {
            std::cout << "Cycle " << cycle << ": cs=" << static_cast<uint64_t>(cs)
                      << ", sclk=" << static_cast<uint64_t>(sclk)
                      << ", mosi=" << static_cast<uint64_t>(mosi_val)
                      << ", miso=" << static_cast<uint64_t>(miso_val)
                      << ", done=" << static_cast<uint64_t>(done)
                      << std::endl;
        } else if (static_cast<uint64_t>(sclk) && !transfer_started) {
            std::cout << "Cycle " << cycle << ": SCLK rising edge (sampling)" << std::endl;
            transfer_started = true;
        }
        
        if (static_cast<uint64_t>(done)) {
            auto rx_data = sim.get_port_value(top_device.instance().io().rx_data);
            std::cout << "Transfer complete at cycle " << cycle << "! RX data: 0x" << std::hex
                      << static_cast<uint64_t>(rx_data) << std::dec << std::endl;
            
            if (static_cast<uint64_t>(rx_data) != 0xAA) {
                std::cerr << "FAILURE: Expected 0xAA, got 0x" << std::hex << static_cast<uint64_t>(rx_data) << std::dec << std::endl;
                return 1;
            }
            if (cycle > 150) {
                std::cerr << "FAILURE: SPI transfer took " << cycle << " cycles (expected ~100)" << std::endl;
                return 1;
            }
            break;
        }
    }
    
    // 生成 Verilog
    std::cout << "\n=== Generating Verilog ===" << std::endl;
    toVerilog("spi_controller.v", top_device.context());
    std::cout << "Verilog generated: spi_controller.v" << std::endl;
    
    std::cout << "\nSPI Controller test completed!" << std::endl;
    return 0;
}
