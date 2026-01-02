#include "ch.hpp"
#include "chlib/if.h"
#include <iostream>

void simple_if_demo() {
    auto ctx = ch::core::context::instance();
    
    // 创建一些信号
    ch::core::ch_bool condition = 1_b;
    ch::core::ch_uint<8> a = 10_d;
    ch::core::ch_uint<8> b = 20_d;
    
    // 使用链式调用风格的if_函数
    auto result1 = ch::chlib::if_(condition, a).else_(b);
    
    // 创建一个输出信号
    ch::core::ch_out<ch::core::ch_uint<8>> output("output", result1);
    
    std::cout << "Simple if_ demo completed" << std::endl;
}

void functional_if_demo() {
    auto ctx = ch::core::context::instance();
    
    // 创建条件信号
    ch::core::ch_bool condition = 0_b;
    
    // 使用函数式风格的if_else函数
    ch::core::ch_uint<8> a = 5_d;
    ch::core::ch_uint<8> b = 3_d;
    auto result = ch::chlib::if_else(
        condition,
        [&a]() { 
            // 在then分支中执行更复杂的操作
            auto temp = a + 10_d;
            return temp * 2_d;
        },
        [&b]() {
            // 在else分支中执行更复杂的操作
            auto temp = b + 5_d;
            return temp * 3_d;
        }
    );
    
    ch::core::ch_out<ch::core::ch_uint<8>> output("output", result);
    
    std::cout << "Functional if_else demo completed" << std::endl;
}

void if_then_demo() {
    auto ctx = ch::core::context::instance();
    
    // 使用if_then函数，仅在条件为真时执行操作
    ch::core::ch_bool condition = 1_b;
    auto result = ch::chlib::if_then(
        condition,
        []() {
            // 在分支中执行更复杂的操作
            ch::core::ch_uint<10> temp = 15_d;
            return temp + 5_d;
        }
    );
    
    ch::core::ch_out<ch::core::ch_uint<10>> output("output", result);
    
    std::cout << "If_then demo completed" << std::endl;
}

void component_if_demo() {
    auto ctx = ch::core::context::instance();
    
    // 创建一个简单的组件，演示如何在组件中使用if_函数
    ch::ch_uint<8> input1 = 100_d;
    ch::core::ch_bool enable = 1_b;
    
    auto output = ch::chlib::if_(
        enable,
        input1 + 50_d  // 如果enable为真，则执行更复杂的操作
    ).else_(
        input1        // 否则使用原值
    );
    
    ch::core::ch_out<ch::core::ch_uint<8>> out("output", output);
    
    std::cout << "Component if demo completed" << std::endl;
}

int main() {
    std::cout << "CppHDL if_() function demo" << std::endl;
    
    simple_if_demo();
    functional_if_demo();
    if_then_demo();
    component_if_demo();
    
    std::cout << "All demos completed successfully!" << std::endl;
    return 0;
}