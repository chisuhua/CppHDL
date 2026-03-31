/**
 * UART TX Example - SpinalHDL Port using State Machine DSL
 * 
 * CppHDL 移植版本 - 使用状态机 DSL
 */

#include "chlib/state_machine.h"
#include "ch.hpp"
#include "component.h"
#include "module.h"
#include "simulator.h"
#include "codegen_verilog.h"
#include <iostream>

using namespace ch;
using namespace ch::core;
using namespace chlib;

// UART TX 状态定义
enum class UartTxState : uint8_t {
    IDLE,     // 空闲状态
    START,    // 发送起始位
    DATA,     // 发送数据位
    STOP      // 发送停止位
};

// ============================================================================
// UART TX 模块（简化版 - 固定波特率）
// ============================================================================

class UartTx : public ch::Component {
public:
    // IO 端口定义
    __io(
        ch_in<ch_bool> frame_start;     // 帧开始信号
        ch_in<ch_uint<8>> data;         // 发送数据
        ch_out<ch_bool> uart_tx;        // UART 发送输出
        ch_out<ch_bool> busy;           // 忙标志
    )
    
    UartTx(ch::Component* parent = nullptr, 
           const std::string& name = "uart_tx")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 波特率计数器 (16 位)
        ch_reg<ch_uint<16>> baud_counter(0_d);
        
        // 位计数器 (3 位，0-7)
        ch_reg<ch_uint<3>> bit_counter(0_d);
        
        // 移位寄存器 (9 位：起始位 +8 数据位)
        ch_reg<ch_uint<9>> shift_reg(511_d);  // 空闲状态为全 1
        
        // 波特率限制值 (50MHz / 115200 ≈ 434)
        ch_uint<16> baud_limit(434_d);
        
        // 创建状态机
        ch_state_machine<UartTxState, 4> sm;
        
        // 状态：IDLE
        sm.state(UartTxState::IDLE)
          .on_entry([this, &shift_reg]() {
              // 入口动作：设置移位寄存器为空闲状态
              shift_reg->next = ch_uint<9>(511_d);
          })
          .on_active([this, &sm, &shift_reg]() {
              io().uart_tx = ch_bool(true);
              io().busy = ch_bool(false);
              
              // 检测到 frame_start 信号，开始发送
              if (io().frame_start == ch_bool(true)) {
                  // 加载数据到移位寄存器：{data, 1'b0} (起始位为 0)
                  shift_reg->next = (io().data << ch_uint<9>(1_d)) | ch_uint<9>(0_d);
                  sm.transition_to(UartTxState::START);
              }
          });
        
        // 状态：START
        sm.state(UartTxState::START)
          .on_active([this, &sm, &baud_counter, &bit_counter]() {
              io().uart_tx = ch_bool(false);  // 起始位为 0
              io().busy = ch_bool(true);
              
              // 波特率计数
              if (baud_counter == ch_uint<16>(433_d)) {
                  baud_counter->next = ch_uint<16>(0_d);
                  bit_counter->next = ch_uint<3>(0_d);
                  sm.transition_to(UartTxState::DATA);
              } else {
                  baud_counter->next = baud_counter + ch_uint<16>(1_d);
              }
          });
        
        // 状态：DATA
        sm.state(UartTxState::DATA)
          .on_active([this, &sm, &baud_counter, &bit_counter, &shift_reg]() {
              io().busy = ch_bool(true);
              
              // 发送当前位（LSB 先发送）
              io().uart_tx = ch_bool((shift_reg & ch_uint<9>(1_d)) != ch_uint<9>(0_d));
              
              // 波特率计数
              if (baud_counter == ch_uint<16>(433_d)) {
                  baud_counter->next = ch_uint<16>(0_d);
                  
                  // 移位
                  shift_reg->next = ch_uint<9>(1_d) | (shift_reg >> ch_uint<9>(1_d));
                  
                  // 位计数
                  if (bit_counter == ch_uint<3>(7_d)) {
                      bit_counter->next = ch_uint<3>(0_d);
                      sm.transition_to(UartTxState::STOP);
                  } else {
                      bit_counter->next = bit_counter + ch_uint<3>(1_d);
                  }
              } else {
                  baud_counter->next = baud_counter + ch_uint<16>(1_d);
              }
          });
        
        // 状态：STOP
        sm.state(UartTxState::STOP)
          .on_active([this, &sm, &baud_counter, &shift_reg]() {
              io().uart_tx = ch_bool(true);  // 停止位为 1
              io().busy = ch_bool(false);
              
              // 波特率计数
              if (baud_counter == ch_uint<16>(433_d)) {
                  baud_counter->next = ch_uint<16>(0_d);
                  shift_reg->next = ch_uint<9>(511_d);  // 恢复空闲状态
                  sm.transition_to(UartTxState::IDLE);
              } else {
                  baud_counter->next = baud_counter + ch_uint<16>(1_d);
              }
          });
        
        // 设置入口状态并构建
        sm.set_entry(UartTxState::IDLE);
        sm.build();
    }
};

// ============================================================================
// 顶层模块（用于测试）
// ============================================================================

class UartTxTop : public ch::Component {
public:
    __io(
        ch_in<ch_bool> frame_start;
        ch_in<ch_uint<8>> data;
        ch_out<ch_bool> uart_tx;
        ch_out<ch_bool> busy;
    )
    
    UartTxTop(ch::Component* parent = nullptr, const std::string& name = "uart_tx_top")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        CH_MODULE(UartTx, uart_tx_inst);
        
        uart_tx_inst.io().frame_start <<= io().frame_start;
        uart_tx_inst.io().data <<= io().data;
        io().uart_tx <<= uart_tx_inst.io().uart_tx;
        io().busy <<= uart_tx_inst.io().busy;
    }
};

// ============================================================================
// 主函数 - 仿真测试
// ============================================================================

int main() {
    std::cout << "CppHDL vs SpinalHDL - UART TX Example (State Machine DSL)" << std::endl;
    std::cout << "========================================================" << std::endl;
    
    {
        ch_device<UartTxTop> device;
        Simulator simulator(device.context());
        
        std::cout << "\n=== UART TX Simulation ===" << std::endl;
        
        // 初始化
        device.instance().io().frame_start = ch_bool(false);
        device.instance().io().data = ch_uint<8>(85_d);  // 0x55 = 01010101
        
        // 空闲几个周期
        for (int i = 0; i < 5; i++) {
            simulator.tick();
        }
        
        // 启动发送
        std::cout << "Starting UART TX with data 85 (0x55)..." << std::endl;
        device.instance().io().frame_start = ch_bool(true);
        simulator.tick();
        device.instance().io().frame_start = ch_bool(false);
        
        // 仿真足够周期以完成一帧（10 位 * 434 波特率周期 + 余量）
        constexpr unsigned FRAME_CYCLES = 4500;
        
        std::cout << "Simulating " << FRAME_CYCLES << " cycles..." << std::endl;
        for (unsigned i = 0; i < FRAME_CYCLES; i++) {
            simulator.tick();
            
            // 打印关键信号
            if (i % 500 == 0 || i < 20) {
                auto uart_tx_val = simulator.get_value(device.instance().io().uart_tx);
                auto busy_val = simulator.get_value(device.instance().io().busy);
                std::cout << "  Cycle " << i << ": uart_tx=" 
                          << static_cast<uint64_t>(uart_tx_val)
                          << ", busy=" << static_cast<uint64_t>(busy_val)
                          << std::endl;
            }
        }
        
        std::cout << "\n=== Simulation Complete ===" << std::endl;
        
        // 生成 Verilog
        std::cout << "\n=== Generating Verilog ===" << std::endl;
        toVerilog("uart_tx.v", device.context());
        std::cout << "Verilog generated: uart_tx.v" << std::endl;
    }
    
    std::cout << "\nProgram completed successfully" << std::endl;
    
    return 0;
}
