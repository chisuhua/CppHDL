#include "ch.hpp"
// #include "chlib.h"  // 包含chlib的所有组件
#include "chlib/if.h"      // 包含chlib的所有组件
#include "chlib/if_stmt.h" // 包含chlib的所有组件
#include "component.h"
#include "core/context.h"
#include "simulator.h"
#include <iostream>

using namespace ch::core;
using namespace chlib;

// 使用表达式风格的条件语句
class ConditionalALU : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> a, b;
         ch_in<ch_uint<2>> op; // 00=add, 01=sub, 10=and, 11=or
         ch_out<ch_uint<8>> result;)

        ConditionalALU(ch::Component *parent = nullptr,
                       const std::string &name = "alu")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // 使用表达式风格
        io().result = if_then(io().op == 0_d, io().a + io().b)
                          .elif (io().op == 1_d, io().a - io().b)
                          .elif (io().op == 2_d, io().a & io().b)
                          .else_(io().a | io().b);

        // 使用优先级风格（适用于互斥条件）
        ch_bool is_zero = (io().a == 0_d) && (io().b == 0_d);
        ch_bool is_max = (io().a == 255_d) && (io().b == 255_d);

        // 这些条件互斥，可以优化
        auto special_result = priority_if_then(is_zero, 0_d)
                                  .elif (is_max, 255_d)
                                  .else_(io().result);
    }
};

// 使用语句块风格的条件语句
class ConditionalCounter : public ch::Component {
public:
    __io(ch_in<ch_bool> clk, rst, en; ch_out<ch_uint<8>> count;)

        ConditionalCounter(ch::Component *parent = nullptr,
                           const std::string &name = "counter")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_reg<ch_uint<8>> reg_count(0_d);

        // 使用组合逻辑中的条件语句
        _if(io().en)
            .then([&] { reg_count->next = reg_count + 1_d; })
            .else_([&] { reg_count->next = reg_count; })
            .endif();

        // 在时序逻辑中使用
        always_ff(io().clk.posedge(),
                  [&] { reg_count->next = reg_count->value(); });

        io().count = reg_count;
    }
};

int main() {
    std::cout << "CppHDL Conditional Statement Demo" << std::endl;
    std::cout << "=================================" << std::endl;

    // 创建ALU组件实例
    ConditionalALU alu;
    ConditionalCounter counter;

    // 创建仿真器
    ch::core::context ctx("test_ctx");
    ch::core::ctx_swap swap(&ctx);
    ch::Simulator sim(&ctx);

    std::cout << "Conditional components created successfully!" << std::endl;

    return 0;
}