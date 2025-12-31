#include <ch.hpp>
#include <chlib/switch.h>
#include <iostream>

using namespace ch::core;
using namespace chlib;

int main() {
    // 创建一些测试信号
    ch_uint<4> input = make_uint<4>(0);
    ch_uint<8> output = make_uint<8>(0);

    // 使用case_函数创建case分支
    auto case1 = case_(make_uint<4>(1), make_uint<8>(10));
    auto case2 = case_(make_uint<4>(2), make_uint<8>(20));
    auto case3 = case_(make_uint<4>(3), make_uint<8>(30));

    // 使用switch_函数实现多路分支
    output = switch_(input, make_uint<8>(0), case1, case2, case3);

    // 使用switch_case便捷函数，直接传入值对
    ch_uint<8> output2 = switch_case(
        input, make_uint<8>(0), make_uint<4>(1), make_uint<8>(10),
        make_uint<4>(2), make_uint<8>(20), make_uint<4>(3), make_uint<8>(30));

    // 并行实现示例
    ch_uint<8> output3 =
        switch_parallel(input, make_uint<8>(0), case1, case2, case3);

    std::cout << "Switch demo completed" << std::endl;

    return 0;
}