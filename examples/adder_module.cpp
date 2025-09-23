// examples/adder_module.cpp
#include "../core/min_cash.h"
#include <iostream>

using namespace ch::core;
    
// 用户定义的带端口模块
struct MyAdderModule {
    __io(
        __in(ch_uint<4>) a;
        __in(ch_uint<4>) b;
        __out(ch_uint<5>) sum;
    );

    void describe() {
        std::cout << "  [MyAdderModule] Calculating sum of " << (unsigned)io.a << " and " << (unsigned)io.b << std::endl;
        io.sum = io.a + io.b; // 直接赋值，无 read()/write()
    }
};

int main() {
    std::cout << "=== Starting Minimal Cash Framework with I/O ===" << std::endl;

    ch::core::ch_device<MyAdderModule> device;

    // 1. 为输入端口赋值
    device.instance().io.a = 5;
    device.instance().io.b = 3;

    // 2. 调用 describe() 进行硬件逻辑计算
    device.describe();

    // 3. 读取输出端口
    std::cout << "=== Result: " << (unsigned)device.instance().io.sum << " ===" << std::endl;

    return 0;
}
