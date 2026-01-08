#include "../include/core/operators.h"
#include "../include/core/literal.h"
#include "../include/core/uint.h"
#include <iostream>

using namespace ch::core;

int main() {
    // 测试当右操作数是字面量8时（值为8，结果范围是0-7，需要3位）
    constexpr unsigned width_result_8 = get_binary_result_width<mod_op, ch_uint<16>, decltype(ch_lit<8>)>();
    std::cout << "Width result for mod 8: " << width_result_8 << " (expected: 3)" << std::endl;
    
    // 测试当右操作数是字面量16时（值为16，结果范围是0-15，需要4位）
    constexpr unsigned width_result_16 = get_binary_result_width<mod_op, ch_uint<32>, decltype(ch_lit<16>)>();
    std::cout << "Width result for mod 16: " << width_result_16 << " (expected: 4)" << std::endl;
    
    // 测试当右操作数是字面量7时（值为7，结果范围是0-6，需要3位）
    constexpr unsigned width_result_7 = get_binary_result_width<mod_op, ch_uint<16>, decltype(ch_lit<7>)>();
    std::cout << "Width result for mod 7: " << width_result_7 << " (expected: 3)" << std::endl;
    
    if (width_result_8 == 3 && width_result_16 == 4 && width_result_7 == 3) {
        std::cout << "All tests passed!" << std::endl;
        return 0;
    } else {
        std::cout << "Some tests failed!" << std::endl;
        return 1;
    }
}