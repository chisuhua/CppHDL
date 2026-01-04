#include "ch.hpp"
#include "component.h"
#include "core/context.h"
#include "simulator.h"
#include <iostream>

using namespace ch::core;
using namespace chlib;

// 使用表达式风格的条件语句
class ConditionalALU : public ch::Component {
public:
    __io(
        ch_in<ch_uint<8>> a, b;
        ch_in<ch_uint<2>> op;  // 00=add, 01=sub, 10=and, 11=or
        ch_out<ch_uint<8>> result;
    )

    ConditionalALU(ch::Component *parent = nullptr,
                   const std::string &name = "alu")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // 使用表达式风格
        io().result = if_then(io().op == 0_d, io().a + io().b)
                     .elif(io().op == 1_d, io().a - io().b)
                     .elif(io().op == 2_d, io().a & io().b)
                     .else_(io().a | io().b);
        
        // 使用优先级风格（适用于互斥条件）
        ch_bool is_zero = (io().a == 0_d) && (io().b == 0_d);
        ch_bool is_max = (io().a == 255_d) && (io().b == 255_d);
        
        // 这些条件互斥，可以优化
        auto special_result = priority_if_then(is_zero, 0_d, true)
                             .elif(is_max, 255_d, true)
                             .else_(io().result);
    }
};

// 使用语句块风格的条件语句
class ConditionalCounter : public ch::Component {
public:
    __io(
        ch_in<ch_bool> clk, rst, en;
        ch_out<ch_uint<8>> count;
    )

    ConditionalCounter(ch::Component *parent = nullptr,
                       const std::string &name = "counter")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_reg<ch_uint<8>> reg_count(0_d);
        
        // 时序逻辑中的条件 - 使用默认时钟
        seq_if(io().rst)
            .then([&]{
                reg_count->next = 0_d;
            })
            .elif(io().en, [&]{
                _if(reg_count == 255_d)
                    .then([&]{
                        reg_count->next = 0_d;
                    })
                    .else_([&]{
                        reg_count->next = reg_count + 1_d;
                    })
                    .endif();
            })
            .else_([&]{
                // 保持当前值
            })
            .endif();
        
        io().count = reg_count;
    }
};

// 使用寄存器特定的条件语句
class RegConditionalCounter : public ch::Component {
public:
    __io(
        ch_in<ch_bool> clk, rst, en;
        ch_out<ch_uint<8>> count;
    )

    RegConditionalCounter(ch::Component *parent = nullptr,
                          const std::string &name = "reg_counter")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_reg<ch_uint<8>> reg_count(0_d);
        
        // 使用寄存器特定的条件语句
        reg_if(io().rst, reg_count)
            .then([&]{
                reg_count->next = 0_d;
            })
            .elif(io().en, [&]{
                reg_if(reg_count == 255_d, reg_count)
                    .then([&]{
                        reg_count->next = 0_d;
                    })
                    .else_([&]{
                        reg_count->next = reg_count + 1_d;
                    })
                    .endif();
            })
            .else_([&]{
                // 保持当前值
            })
            .endif();
        
        io().count = reg_count;
    }
};

// 复杂条件逻辑示例
class ComplexConditionalLogic : public ch::Component {
public:
    __io(
        ch_in<ch_bool> clk, rst;
        ch_in<ch_bool> enable;
        ch_in<ch_uint<4>> sel;
        ch_in<ch_uint<8>> in_a, in_b, in_c, in_d;
        ch_out<ch_uint<8>> out;
    )

    ComplexConditionalLogic(ch::Component *parent = nullptr,
                           const std::string &name = "complex_cond")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_reg<ch_uint<8>> reg_out(0_d);
        
        // 使用嵌套条件和默认时钟
        seq_if(io().rst)
            .then([&]{
                reg_out->next = 0_d;
            })
            .elif(io().enable, [&]{
                // 多路选择器
                auto selected = if_then(io().sel[3], io().in_a)
                              .elif(io().sel[2], io().in_b)
                              .elif(io().sel[1], io().in_c)
                              .else_(io().in_d);
                
                reg_out->next = selected;
            })
            .endif(); // 使用默认时钟
            
        io().out = reg_out;
    }
};

int main() {
    std::cout << "CppHDL Conditional Statement Demo" << std::endl;
    std::cout << "=================================" << std::endl;

    // 创建ALU组件实例
    ConditionalALU alu;

    // 创建计数器组件实例
    ConditionalCounter counter;

    // 创建寄存器条件计数器实例
    RegConditionalCounter reg_counter;

    // 创建复杂条件逻辑实例
    ComplexConditionalLogic complex_logic;

    // 创建仿真器
    ch::core::context ctx("test_ctx");
    ch::core::ctx_swap swap(&ctx);
    ch::Simulator sim(&ctx);
    
    std::cout << "Conditional components created successfully!" << std::endl;

    return 0;
}