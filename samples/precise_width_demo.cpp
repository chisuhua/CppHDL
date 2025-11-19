// precise_width_demo.cpp - Demonstrate precise width support
#include "core/operators.h"
#include "core/uint.h"
#include "core/bool.h"
#include "core/literal.h"
#include "core/context.h"
#include <iostream>

using namespace ch::core;

int main() {
    // Create test context
    context ctx("precise_width_demo");
    ctx_swap swap(&ctx);
    
    std::cout << "=== Precise Width Demo ===" << std::endl;
    
    // Create different width ch_uint values
    ch_uint<3> a(0b101, "a");   // 3-bit value
    ch_uint<5> b(0b11010, "b"); // 5-bit value
    
    std::cout << "a width: " << ch_width_v<decltype(a)> << " bits" << std::endl;
    std::cout << "b width: " << ch_width_v<decltype(b)> << " bits" << std::endl;
    
    // Test concat operation with precise widths
    auto concat_result = concat(a, b);
    std::cout << "concat(a,b) width: " << ch_width_v<decltype(concat_result)> << " bits" << std::endl;
    std::cout << "Expected concat width: " << (ch_width_v<decltype(a)> + ch_width_v<decltype(b)>) << " bits" << std::endl;
    
    // Test bit slice operations
    ch_uint<12> data(0b101101011100, "data");
    auto slice = bits<decltype(data), 7, 4>(data);
    std::cout << "bits<7,4>(data) width: " << ch_width_v<decltype(slice)> << " bits" << std::endl;
    std::cout << "Expected slice width: " << (7-4+1) << " bits" << std::endl;
    
    // Test arithmetic operations
    ch_uint<7> x(0b1010101, "x");
    ch_uint<5> y(0b11010, "y");
    auto sum = x + y;
    std::cout << "x + y width: " << ch_width_v<decltype(sum)> << " bits" << std::endl;
    std::cout << "Expected sum width: " << (std::max(7u, 5u) + 1) << " bits" << std::endl;
    
    // Test nested operations
    ch_uint<2> p(0b11, "p");
    ch_uint<3> q(0b101, "q");
    ch_uint<4> r(0b1110, "r");
    auto nested = concat(p, concat(q, r));
    std::cout << "concat(p, concat(q, r)) width: " << ch_width_v<decltype(nested)> << " bits" << std::endl;
    std::cout << "Expected nested width: " << (2 + 3 + 4) << " bits" << std::endl;
    
    std::cout << "=== Demo Complete ===" << std::endl;
    
    return 0;
}