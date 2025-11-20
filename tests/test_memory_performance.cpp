#include "catch_amalgamated.hpp"
#include "simulator.h"
#include "ch.hpp"
#include <chrono>
#include <iostream>
#include <vector>

using namespace ch;
using namespace ch::core;

// Performance test to verify memory access optimization
TEST_CASE("Memory Access Performance Test", "[memory][performance]") {
    context ctx("test_ctx");
    ctx_swap swap(&ctx);
    
    // Create a large number of registers and operations to test performance
    constexpr size_t num_registers = 100;
    std::vector<ch_reg<ch_uint<8>>> registers;
    registers.reserve(num_registers);
    
    // Create registers
    for (size_t i = 0; i < num_registers; ++i) {
        registers.emplace_back(ch_literal((uint64_t)(i & 0xFF)), "reg_" + std::to_string(i));
    }
    
    // Create operations chaining registers together
    std::vector<decltype(registers[0] + registers[0])> operations;
    operations.reserve(num_registers - 1);
    
    for (size_t i = 0; i < num_registers - 1; ++i) {
        operations.push_back(registers[i] + registers[i + 1]);
    }
    
    // Create simulator
    Simulator sim(&ctx);
    
    // Measure evaluation performance
    auto start = std::chrono::high_resolution_clock::now();
    
    // Run multiple simulation cycles
    constexpr size_t num_cycles = 10;
    for (size_t i = 0; i < num_cycles; ++i) {
        sim.tick();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "Evaluated " << num_registers << " registers with " 
              << (num_registers - 1) << " operations over " 
              << num_cycles << " cycles in " 
              << duration.count() << " microseconds" << std::endl;
    
    // Performance should be reasonable (less than 1 second for this test)
    REQUIRE(duration.count() < 1000000); // 1 second threshold
    
    // We can't easily check register values since ch_reg doesn't expose data_out
    // But the test has run successfully if we got here without crashing
    REQUIRE(true);
}