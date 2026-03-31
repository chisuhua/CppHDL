/**
 * VGA Controller Example - SpinalHDL Port
 * 
 * 功能:
 * - 640x480 @ 60Hz VGA 时序生成
 * - HSync/VSync 信号生成
 * - 屏幕坐标计数器
 * - 简单颜色输出 (测试图案)
 * 
 * VGA 640x480@60Hz 时序:
 * - 像素时钟：25.175 MHz
 * - 水平：640 显示 + 16 前肩 + 96 同步 + 48 后肩 = 800 像素
 * - 垂直：480 显示 + 10 前肩 + 2 同步 + 33 后肩 = 525 行
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
// VGA Controller 模块
// ============================================================================

class VgaController : public ch::Component {
public:
    __io(
        // VGA 输出
        ch_out<ch_bool> hsync;       // 水平同步
        ch_out<ch_bool> vsync;       // 垂直同步
        ch_out<ch_bool> red;         // 红色 (1 位)
        ch_out<ch_bool> green;       // 绿色 (1 位)
        ch_out<ch_bool> blue;        // 蓝色 (1 位)
        
        // 调试输出
        ch_out<ch_uint<10>> hcount;  // 水平计数器
        ch_out<ch_uint<10>> vcount;  // 垂直计数器
    )
    
    VgaController(ch::Component* parent = nullptr, const std::string& name = "vga_ctrl")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 水平计数器 (0-799)
        ch_reg<ch_uint<10>> hcnt(0_d);
        
        // 垂直计数器 (0-524)
        ch_reg<ch_uint<10>> vcnt(0_d);
        
        // 水平时序参数
        constexpr unsigned H_DISPLAY = 640;
        constexpr unsigned H_FRONT = 16;
        constexpr unsigned H_SYNC = 96;
        constexpr unsigned H_BACK = 48;
        constexpr unsigned H_TOTAL = H_DISPLAY + H_FRONT + H_SYNC + H_BACK;  // 800
        
        // 垂直时序参数
        constexpr unsigned V_DISPLAY = 480;
        constexpr unsigned V_FRONT = 10;
        constexpr unsigned V_SYNC = 2;
        constexpr unsigned V_BACK = 33;
        constexpr unsigned V_TOTAL = V_DISPLAY + V_FRONT + V_SYNC + V_BACK;  // 525
        
        // 水平同步脉冲 (低电平有效)
        // HSync 在 hcnt >= H_DISPLAY + H_FRONT 时拉低，持续 H_SYNC 周期
        auto hsync_start = ch_uint<10>(H_DISPLAY + H_FRONT);
        auto hsync_end = ch_uint<10>(H_DISPLAY + H_FRONT + H_SYNC);
        auto hsync_active = (hcnt >= hsync_start) && (hcnt < hsync_end);
        io().hsync = !hsync_active;  // 低电平有效
        
        // 垂直同步脉冲 (低电平有效)
        auto vsync_start = ch_uint<10>(V_DISPLAY + V_FRONT);
        auto vsync_end = ch_uint<10>(V_DISPLAY + V_FRONT + V_SYNC);
        auto vsync_active = (vcnt >= vsync_start) && (vcnt < vsync_end);
        io().vsync = !vsync_active;  // 低电平有效
        
        // 显示区域检测
        auto h_display = (hcnt < ch_uint<10>(H_DISPLAY));
        auto v_display = (vcnt < ch_uint<10>(V_DISPLAY));
        auto in_display = h_display && v_display;
        
        // 简单测试图案：棋盘格
        // 每 32 像素交替颜色
        auto h_pattern = (bits<9, 5>(hcnt) == ch_uint<5>(0_d));  // 每 32 像素
        auto v_pattern = (bits<9, 5>(vcnt) == ch_uint<5>(0_d));
        auto checker = h_pattern ^ v_pattern;
        
        // RGB 输出
        io().red = select(in_display, checker, ch_bool(false));
        io().green = select(in_display, !checker, ch_bool(false));
        io().blue = ch_bool(false);
        
        // 计数器更新
        auto h_reset = (hcnt == ch_uint<10>(H_TOTAL - 1));
        hcnt->next = select(h_reset, ch_uint<10>(0_d), hcnt + ch_uint<10>(1_d));
        
        auto v_reset = (vcnt == ch_uint<10>(V_TOTAL - 1));
        vcnt->next = select(v_reset && h_reset, ch_uint<10>(0_d),
                            select(h_reset, vcnt + ch_uint<10>(1_d), vcnt));
        
        io().hcount = hcnt;
        io().vcount = vcnt;
    }
};

// ============================================================================
// 顶层模块
// ============================================================================

class VgaTop : public ch::Component {
public:
    __io(
        ch_out<ch_bool> hsync;
        ch_out<ch_bool> vsync;
        ch_out<ch_bool> red;
        ch_out<ch_bool> green;
        ch_out<ch_bool> blue;
        ch_out<ch_uint<10>> hcount;
        ch_out<ch_uint<10>> vcount;
    )
    
    VgaTop(ch::Component* parent = nullptr, const std::string& name = "vga_top")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        ch::ch_module<VgaController> vga{"vga"};
        
        io().hsync <<= vga.io().hsync;
        io().vsync <<= vga.io().vsync;
        io().red <<= vga.io().red;
        io().green <<= vga.io().green;
        io().blue <<= vga.io().blue;
        io().hcount <<= vga.io().hcount;
        io().vcount <<= vga.io().vcount;
    }
};

// ============================================================================
// 主函数 - 仿真测试
// ============================================================================

int main() {
    std::cout << "CppHDL vs SpinalHDL - VGA Controller Example" << std::endl;
    std::cout << "============================================" << std::endl;
    std::cout << "VGA 640x480 @ 60Hz" << std::endl;
    std::cout << "H: 640 + 16 + 96 + 48 = 800 pixels" << std::endl;
    std::cout << "V: 480 + 10 + 2 + 33 = 525 lines" << std::endl;
    
    ch::ch_device<VgaTop> top_device;
    ch::Simulator sim(top_device.context());
    
    std::cout << "\n=== Test 1: HSync Timing ===" << std::endl;
    
    // 运行几个水平周期
    bool hsync_seen = false;
    unsigned hsync_count = 0;
    for (int cycle = 0; cycle < 2000; ++cycle) {
        sim.tick();
        
        auto hsync = sim.get_port_value(top_device.instance().io().hsync);
        auto hcount = sim.get_port_value(top_device.instance().io().hcount);
        
        // 检测 HSync 下降沿
        static uint64_t prev_hsync = 1;
        if (prev_hsync && !static_cast<uint64_t>(hsync)) {
            hsync_seen = true;
            std::cout << "HSync pulse at hcount=" << static_cast<uint64_t>(hcount) << std::endl;
        }
        prev_hsync = static_cast<uint64_t>(hsync);
        
        if (static_cast<uint64_t>(hsync) && hsync_seen) {
            hsync_count++;
            if (hsync_count >= 3) break;
        }
        
        // 打印前几个周期的状态
        if (cycle < 20) {
            std::cout << "Cyc " << cycle << ": hcount=" << static_cast<uint64_t>(hcount)
                      << ", hsync=" << static_cast<uint64_t>(hsync) << std::endl;
        }
    }
    
    std::cout << "\n=== Test 2: Frame Timing ===" << std::endl;
    
    // 简单验证 vcount 递增
    auto vcount1 = sim.get_port_value(top_device.instance().io().vcount);
    for (int i = 0; i < 1000; ++i) sim.tick();
    auto vcount2 = sim.get_port_value(top_device.instance().io().vcount);
    
    std::cout << "vcount: " << static_cast<uint64_t>(vcount1) 
              << " -> " << static_cast<uint64_t>(vcount2) << std::endl;
    
    bool vsync_seen = (static_cast<uint64_t>(vcount2) > static_cast<uint64_t>(vcount1));
    
    std::cout << "\n=== Test 3: Display Output ===" << std::endl;
    
    auto red = sim.get_port_value(top_device.instance().io().red);
    auto green = sim.get_port_value(top_device.instance().io().green);
    auto hcount = sim.get_port_value(top_device.instance().io().hcount);
    auto vcount = sim.get_port_value(top_device.instance().io().vcount);
    
    std::cout << "At hcount=" << static_cast<uint64_t>(hcount)
              << ", vcount=" << static_cast<uint64_t>(vcount) << ":" << std::endl;
    std::cout << "  red=" << static_cast<uint64_t>(red)
              << ", green=" << static_cast<uint64_t>(green) << std::endl;
    
    std::cout << "\n=== Generating Verilog ===" << std::endl;
    toVerilog("vga_controller.v", top_device.context());
    std::cout << "Verilog generated" << std::endl;
    
    if (hsync_seen && vsync_seen) {
        std::cout << "\n✅ VGA Controller test PASSED!" << std::endl;
    } else {
        std::cout << "\n❌ VGA Controller test FAILED!" << std::endl;
    }
    
    return 0;
}
