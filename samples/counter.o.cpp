// samples/counter.cpp
#include "../include/ch.hpp"
#include "component.h"
#include "module.h"
#include <iostream>

using namespace ch::core;

template <unsigned N>
class Counter : public ch::Component {
public:
    //__io(ch_out<ch_uint<N>> out);  // 输出端口
    ch_out<ch_uint<4>> out; // 成员变量，但不创建节点

    Counter(ch::Component* parent = nullptr, const std::string& name = "counter")
        : ch::Component(parent, name)
    {
        // 构造函数中不创建节点，不调用 describe()
    }

    void describe() override {
        ch_reg<ch_uint<N>> reg(0);
        reg->next = reg + 1;
        io.out = reg;  // ✅ ch_logic_out::operator= 已实现
    }
};

// 顶层模块
class Top : public ch::Component {
public:
    __io(ch_out<ch_uint<4>> out);

    Top(ch::Component* parent = nullptr, const std::string& name = "top")
        : ch::Component(parent, name)
    {}

    void describe() override {
        ch::ch_module<Counter<4>> counter;  // 自动归属到 Top
        io.out = counter.io.out;
    }
};

// --- 主函数 ---
int main() {
    // ✅ 使用 ch_device 作为顶层入口
    ch::ch_device<Top> top_device;

    // 后续可接仿真器
    // ch::Simulator sim(top_device.context());
    // sim.run(10);

    return 0;
}
