// examples/dual_clock_module.cpp
#include "../core/min_cash.h"
#include <iostream>

using namespace ch::core;

struct DualClockModule {
    __io(
        __in(ch_bool) clk_a;
        __in(ch_bool) clk_b;
        __in(ch_bool) rst;
        __out(ch_uint<4>) count_a;
        __out(ch_uint<4>) count_b;
    );

    void describe() {
        // 时钟域 A: 上升沿触发
        ch_pushcd(io.clk_a, io.rst, true);
        static ch_reg<ch_uint<4>> reg_a(0);
        reg_a.next() = *reg_a + 1;
        io.count_a = *reg_a;
        ch_popcd();

        // 时钟域 B: 下降沿触发
        ch_pushcd(io.clk_b, io.rst, false);
        static ch_reg<ch_uint<4>> reg_b(0);
        reg_b.next() = *reg_b + 1;
        io.count_b = *reg_b;
        ch_popcd();
    }
};

int main() {
    std::cout << "=== Starting Simulation: Dual Clock Module ===" << std::endl;

    ch_device<DualClockModule> device;

    // 初始化
    device.instance().io.clk_a = 0;
    device.instance().io.clk_b = 1; // 初始为高，下降沿触发
    device.instance().io.rst = 1;
    device.describe();

    // 运行 10 个仿真周期
    for (int cycle = 0; cycle < 10; cycle++) {
        std::cout << "\n--- Cycle " << cycle << " ---" << std::endl;

        // 生成时钟信号
        device.instance().io.clk_a = (cycle % 2); // 0,1,0,1... (上升沿在奇数周期)
        device.instance().io.clk_b = (cycle % 2) ? 0 : 1; // 1,0,1,0... (下降沿在奇数周期)
        device.instance().io.rst = (cycle < 2); // 前两个周期复位

        // 1. 计算组合逻辑
        device.describe();

        // 2. 更新寄存器
        device.tick();

        // 3. 读取输出
        std::cout << "Count A: " << (unsigned)device.instance().io.count_a << std::endl;
        std::cout << "Count B: " << (unsigned)device.instance().io.count_b << std::endl;
    }

    std::cout << "\n=== Simulation Complete ===" << std::endl;
    return 0;
}
