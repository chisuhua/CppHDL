// examples/counter_module.cpp
#include "../core/min_cash.h"
#include <iostream>

using namespace ch::core;

struct CounterModule {
    __io(
        __in(ch_bool) clk;
        __in(ch_bool) rst;
        __out(ch_uint<4>) count;
    );

    ch_reg<ch_uint<4>>* reg = nullptr;

    CounterModule() : reg(0) {
        std::cout << "  [CounterModule] Constructor called" << std::endl;
    }

    void describe() {
        ch_pushcd(io.clk, io.rst, true);
        if (reg == nullptr) {
            reg = new ch_reg<ch_uint<4>>(0);
        }

        if (io.rst) {
            reg->next() = 0; // 设置下一周期的值
        } else {
            reg->next() = reg->value() + 1; // 获取当前值，加1，设置为下一周期的值
        }

        // 输出当前值（组合逻辑）
        io.count = reg->value();

        ch_popcd();
    }

    ~CounterModule() {
        delete reg;
    }
};

int main() {
    std::cout << "=== Starting Simulation: Counter Module ===" << std::endl;

    ch_device<CounterModule> device;

    device.instance().io.clk = 0;
    device.instance().io.rst = 1; // 初始复位
    // 运行 5 个时钟周期
    for (int cycle = 0; cycle < 15; cycle++) {
        std::cout << "\n--- Cycle " << cycle << " ---" << std::endl;

        // 设置输入信号
        device.instance().io.clk = (cycle % 2) ? 0 : 1; // 0, 1, 0, 1, 0 (模拟时钟)
        device.instance().io.rst = (cycle == 0); // 仅在第0周期复位

        // 1. 调用 describe()：计算组合逻辑和 next-value
        device.describe();

        device.tick();

        // 3. 读取输出
        std::cout << "Count: " << (unsigned)device.instance().io.count << std::endl;
    }

    std::cout << "\n=== Simulation Complete ===" << std::endl;
    return 0;
}
