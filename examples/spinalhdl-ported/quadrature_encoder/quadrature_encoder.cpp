/**
 * Quadrature Encoder Example - SpinalHDL Port
 * 
 * 功能:
 * - 正交编码器 (A/B 相) 解码
 * - 4 倍频计数 (上升沿 + 下降沿)
 * - 方向检测
 * - 可配置计数器宽度
 * - 索引 (Z) 信号清零
 * 
 * 正交编码原理:
 * - A 相领先 B 相 = 顺时针 (CW)
 * - B 相领先 A 相 = 逆时针 (CCW)
 * - 每个周期产生 4 个计数边沿
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
// Quadrature Encoder 模块
// ============================================================================

template <unsigned COUNTER_WIDTH = 16>
class QuadratureEncoder : public ch::Component {
public:
    __io(
        // 编码器输入
        ch_in<ch_bool> enc_a;       // A 相输入
        ch_in<ch_bool> enc_b;       // B 相输入
        ch_in<ch_bool> enc_z;       // 索引 (Z) 输入 - 清零信号
        
        // 计数输出
        ch_out<ch_uint<COUNTER_WIDTH>> position;  // 位置计数 (无符号)
        ch_out<ch_bool> direction;  // 方向 (1=CW, 0=CCW)
        
        // 调试输出
        ch_out<ch_bool> a_sync;     // 同步后的 A 相
        ch_out<ch_bool> b_sync;     // 同步后的 B 相
    )
    
    QuadratureEncoder(ch::Component* parent = nullptr, const std::string& name = "quad_enc")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 输入同步 (消除亚稳态)
        ch_reg<ch_bool> a_sync1(ch_bool(true));
        ch_reg<ch_bool> a_sync2(ch_bool(true));
        ch_reg<ch_bool> b_sync1(ch_bool(true));
        ch_reg<ch_bool> b_sync2(ch_bool(true));
        
        a_sync1->next = io().enc_a;
        a_sync2->next = a_sync1;
        b_sync1->next = io().enc_b;
        b_sync2->next = b_sync1;
        
        io().a_sync = a_sync2;
        io().b_sync = b_sync2;
        
        // 边沿检测
        auto a_rising = !a_sync2 && a_sync1;
        auto a_falling = a_sync2 && !a_sync1;
        auto b_rising = !b_sync2 && b_sync1;
        auto b_falling = b_sync2 && !b_sync1;
        
        // 方向检测 (基于 A 相和 B 相的相位关系)
        // A 领先 B = CW (顺时针), B 领先 A = CCW (逆时针)
        auto is_cw = (a_sync2 && !b_sync2) || (!a_sync2 && b_sync2);
        
        // 4 倍频计数逻辑
        // 在 A 相和 B 相的每个边沿都计数
        auto count_up = (a_rising && !b_sync2) || (a_falling && b_sync2) ||
                        (b_rising && a_sync2) || (b_falling && !a_sync2);
        auto count_down = (a_rising && b_sync2) || (a_falling && !b_sync2) ||
                          (b_rising && !a_sync2) || (b_falling && a_sync2);
        
        // 位置计数器 (使用无符号数，支持双向计数)
        ch_reg<ch_uint<COUNTER_WIDTH>> pos_reg(0_d);
        
        // Z 相信号清零 (检测 Z 相上升沿)
        ch_reg<ch_bool> z_sync1(ch_bool(false));
        ch_reg<ch_bool> z_sync2(ch_bool(false));
        z_sync1->next = io().enc_z;
        z_sync2->next = z_sync1;
        auto z_rising = !z_sync2 && z_sync1;
        
        auto new_pos_up = pos_reg + ch_uint<COUNTER_WIDTH>(1_d);
        auto new_pos_down = pos_reg - ch_uint<COUNTER_WIDTH>(1_d);
        
        pos_reg->next = select(z_rising, ch_uint<COUNTER_WIDTH>(0_d),
                               select(count_up, new_pos_up,
                                      select(count_down, new_pos_down, pos_reg)));
        
        io().position = pos_reg;
        io().direction = is_cw;
    }
};

// ============================================================================
// 顶层模块
// ============================================================================

class QuadEncTop : public ch::Component {
public:
    __io(
        ch_in<ch_bool> enc_a;
        ch_in<ch_bool> enc_b;
        ch_in<ch_bool> enc_z;
        ch_out<ch_uint<16>> position;
        ch_out<ch_bool> direction;
        ch_out<ch_bool> a_sync;
        ch_out<ch_bool> b_sync;
    )
    
    QuadEncTop(ch::Component* parent = nullptr, const std::string& name = "quad_enc_top")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        ch::ch_module<QuadratureEncoder<16>> enc{"enc"};
        
        enc.io().enc_a <<= io().enc_a;
        enc.io().enc_b <<= io().enc_b;
        enc.io().enc_z <<= io().enc_z;
        io().position <<= enc.io().position;
        io().direction <<= enc.io().direction;
        io().a_sync <<= enc.io().a_sync;
        io().b_sync <<= enc.io().b_sync;
    }
};

// ============================================================================
// 主函数 - 仿真测试
// ============================================================================

int main() {
    std::cout << "CppHDL vs SpinalHDL - Quadrature Encoder" << std::endl;
    std::cout << "=========================================" << std::endl;
    std::cout << "4x quadrature decoding with Z-index reset" << std::endl;
    
    ch::ch_device<QuadEncTop> top_device;
    ch::Simulator sim(top_device.context());
    
    // 初始化输入
    sim.set_input_value(top_device.instance().io().enc_a, false);
    sim.set_input_value(top_device.instance().io().enc_b, false);
    sim.set_input_value(top_device.instance().io().enc_z, false);
    
    std::cout << "\n=== Test 1: CW Rotation (A leads B) ===" << std::endl;
    
    // 模拟顺时针旋转 (A 相领先 B 相 90 度)
    // 状态序列：00 -> 10 -> 11 -> 01 -> 00 (4 个边沿)
    for (int cycle = 0; cycle < 20; ++cycle) {
        // 状态 0: A=0, B=0
        if (cycle == 0) {
            sim.set_input_value(top_device.instance().io().enc_a, false);
            sim.set_input_value(top_device.instance().io().enc_b, false);
        }
        // 状态 1: A=1, B=0 (A 上升沿)
        else if (cycle == 2) {
            sim.set_input_value(top_device.instance().io().enc_a, true);
            sim.set_input_value(top_device.instance().io().enc_b, false);
        }
        // 状态 2: A=1, B=1 (B 上升沿)
        else if (cycle == 4) {
            sim.set_input_value(top_device.instance().io().enc_a, true);
            sim.set_input_value(top_device.instance().io().enc_b, true);
        }
        // 状态 3: A=0, B=1 (A 下降沿)
        else if (cycle == 6) {
            sim.set_input_value(top_device.instance().io().enc_a, false);
            sim.set_input_value(top_device.instance().io().enc_b, true);
        }
        // 状态 4: A=0, B=0 (B 下降沿)
        else if (cycle == 8) {
            sim.set_input_value(top_device.instance().io().enc_a, false);
            sim.set_input_value(top_device.instance().io().enc_b, false);
        }
        
        sim.tick();
        
        auto pos = sim.get_port_value(top_device.instance().io().position);
        auto dir = sim.get_port_value(top_device.instance().io().direction);
        
        if (cycle < 15) {
            std::cout << "Cyc " << cycle << ": pos=" << static_cast<uint64_t>(pos)
                      << ", dir=" << static_cast<uint64_t>(dir) << std::endl;
        }
    }
    
    std::cout << "\n=== Test 2: CCW Rotation (B leads A) ===" << std::endl;
    
    // 模拟逆时针旋转 (B 相领先 A 相 90 度)
    for (int cycle = 0; cycle < 20; ++cycle) {
        // 状态 0: A=0, B=0
        if (cycle == 0) {
            sim.set_input_value(top_device.instance().io().enc_a, false);
            sim.set_input_value(top_device.instance().io().enc_b, false);
        }
        // 状态 1: A=0, B=1 (B 上升沿)
        else if (cycle == 2) {
            sim.set_input_value(top_device.instance().io().enc_a, false);
            sim.set_input_value(top_device.instance().io().enc_b, true);
        }
        // 状态 2: A=1, B=1 (A 上升沿)
        else if (cycle == 4) {
            sim.set_input_value(top_device.instance().io().enc_a, true);
            sim.set_input_value(top_device.instance().io().enc_b, true);
        }
        // 状态 3: A=1, B=0 (B 下降沿)
        else if (cycle == 6) {
            sim.set_input_value(top_device.instance().io().enc_a, true);
            sim.set_input_value(top_device.instance().io().enc_b, false);
        }
        // 状态 4: A=0, B=0 (A 下降沿)
        else if (cycle == 8) {
            sim.set_input_value(top_device.instance().io().enc_a, false);
            sim.set_input_value(top_device.instance().io().enc_b, false);
        }
        
        sim.tick();
        
        auto pos = sim.get_port_value(top_device.instance().io().position);
        auto dir = sim.get_port_value(top_device.instance().io().direction);
        
        if (cycle < 15) {
            std::cout << "Cyc " << cycle << ": pos=" << static_cast<uint64_t>(pos)
                      << ", dir=" << static_cast<uint64_t>(dir) << std::endl;
        }
    }
    
    std::cout << "\n=== Test 3: Z-index Reset ===" << std::endl;
    
    // 设置一个非零位置
    sim.set_input_value(top_device.instance().io().enc_a, true);
    sim.set_input_value(top_device.instance().io().enc_b, false);
    sim.tick();
    sim.tick();
    
    // 触发 Z 相清零
    sim.set_input_value(top_device.instance().io().enc_z, true);
    sim.tick();
    sim.set_input_value(top_device.instance().io().enc_z, false);
    sim.tick();
    
    auto pos = sim.get_port_value(top_device.instance().io().position);
    std::cout << "After Z-reset: pos=" << static_cast<uint64_t>(pos) << std::endl;
    
    std::cout << "\n=== Generating Verilog ===" << std::endl;
    toVerilog("quadrature_encoder.v", top_device.context());
    std::cout << "Verilog generated: quadrature_encoder.v" << std::endl;
    
    std::cout << "\n✅ Quadrature Encoder test completed!" << std::endl;
    return 0;
}
