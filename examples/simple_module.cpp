// examples/simple_module.cpp
#include "../core/min_cash.h"
#include <iostream>

struct MySimpleModule {
private:
    int internal_counter_ = 0; // 内部状态

public:
    void describe() {
        std::cout << "  [MySimpleModule] Counter before: " << internal_counter_ << std::endl;
        internal_counter_ += 1; // 模拟一个状态更新
        std::cout << "  [MySimpleModule] Counter after: " << internal_counter_ << std::endl;
    }
};

int main() {
    std::cout << "=== Starting Minimal Cash Framework ===" << std::endl;

    // 实例化设备 —— 这是用户唯一的入口
    ch::core::ch_device<MySimpleModule> device;

    // 调用 describe 方法 —— 这是硬件描述的入口点
    device.describe();

    std::cout << "=== Simulation Complete ===" << std::endl;
    return 0;
}
