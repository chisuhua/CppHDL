// examples/counter_module.cpp
#include "../core/min_cash.h"
#include "../core/ch_tracer.h"
#include "../core/ch_verilog_gen.h"
#include "../core/component.h"
#include <iostream>

using namespace ch::core;

struct CounterModule : public Component {
    __io(
        __in(ch_bool) clk;
        __in(ch_bool) rst;
        __out(ch_uint<4>) count;
    );

    void describe() {
        ch_pushcd(io.clk, io.rst, true);
        static ch_reg<ch_uint<4>> reg(this, "CounterModule", 0);

        reg.next() = ch_nextEn(*reg + 1, !io.rst, 0);
        io.count = *reg + 1;
        ch_popcd();
    }
};

int main() {
    std::cout << "=== Starting Simulation: Counter Module ===" << std::endl;

    ch_device<CounterModule> device;
    ch_tracer tracer(device, "counter_wave.vcd"); // ✅ 创建 tracer
    //
    tracer.add_signal(device.instance().io.clk, "clk", 1);
    tracer.add_signal(device.instance().io.rst, "rst", 1);
    tracer.add_signal(device.instance().io.count, "count", 4);

    //device.instance().io.clk = 0;
    //device.instance().io.rst = 1; // 初始复位
    // 运行 5 个时钟周期
    for (int cycle = 0; cycle < 15; cycle++) {
        std::cout << "\n--- Cycle " << cycle << " ---" << std::endl;

        // 设置输入信号
        device.instance().io.clk = (cycle % 2) ? 0 : 1; // 0, 1, 0, 1, 0 (模拟时钟)
        device.instance().io.rst = (cycle == 0); // 仅在第0周期复位

        // 1. 调用 describe()：计算组合逻辑和 next-value
        device.describe();

        device.tick();
        tracer.tick();

        // 3. 读取输出
        std::cout << "Count: " << (unsigned)device.instance().io.count << std::endl;
    }


    ch_toVerilog("counter_generated.v", device);

    std::cout << "\n=== Simulation Complete ===" << std::endl;
    return 0;
}
