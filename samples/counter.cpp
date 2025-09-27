#include "../include/ch.hpp" // Include the main CppHDL header
#include <iostream>

using namespace ch::core;

template <unsigned N>
class Counter : public ch::Component {
public:
    //__io((__out(ch_uint<4>) out));
    __io(ch_logic_out<ch_uint<N>> out); // Use the expanded type directly

    Counter(ch::Component* parent = nullptr, const std::string& name = "counter")
        : ch::Component(parent, name)
    {}

    void describe() override {
        ch_reg<ch_uint<4>> reg(0);
        reg->next = reg + 1;
        io.out = reg;
    }
};

// 顶层模块
class Top : public ch::Component {
public:
    //__io((__out(ch_uint<4>) out));
    __io(ch_logic_out<ch_uint<4>> out); // Use the expanded type directly

    Top(ch::Component* parent = nullptr, const std::string& name = "top")
        : ch::Component(parent, name)
    {}

    void describe() override {
        ch::ch_module<Counter<4>> counter; // 自动归属到 Top
        io.out = counter.io.out;
    }
};

int main() {
    // 直接构造顶层模块（无父）
    ch::ch_device<Top> top; // 语义清晰：这是顶层设备
    /*
    ch::Simulator sim(top.context());
    sim.run(10);
    */
    // 后续：Simulator sim(top.context()); ...
    return 0;
}
