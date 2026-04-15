/**
 * @file test_riscv_tests_pipeline.cpp
 * @brief RISC-V RV32UI tests through actual Rv32iPipeline hardware (Phase F Wave 3)
 * 
 * Tests:
 * 1. Rv32iPipeline hardware module instantiation
 * 2. riscv-tests .bin file loading into I-TCM
 * 3. Execution through actual 5-stage pipeline (not interpreter)
 * 4. tohost@0x80001000 monitoring for pass/fail
 * 
 * riscv-tests spec:
 * - Entry point: 0x80000000
 * - tohost: 0x80001000 (1=pass, 0=fail)
 * - Binary files: /workspace/project/riscv-tests/build/rv32ui/*.bin
 */

#include "../../../tests/catch_amalgamated.hpp"
#include "ch.hpp"
#include "simulator.h"
#include "../../include/cpu/pipeline/rv32i_pipeline.h"
#include "../../include/cpu/pipeline/rv32i_tcm.h"

#include <cstdint>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>

using namespace ch::core;
using namespace riscv;

// ============================================================================
// Constants
// ============================================================================

constexpr uint32_t RV32I_ENTRY_POINT = 0x80000000;
constexpr uint32_t RV32I_TOHOST_ADDR = 0x80001000;
constexpr uint32_t RV32I_TOHOST_WORD = RV32I_TOHOST_ADDR / 4;
constexpr uint32_t RV32I_MEM_SIZE = 65536;
constexpr uint32_t MAX_TICKS = 100000;

// ============================================================================
// Helper: Load binary file into memory
// ============================================================================

std::vector<uint32_t> load_binary_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    REQUIRE(file.is_open());
    
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<char> buffer(size);
    file.read(buffer.data(), size);
    
    // Pad to word boundary
    while (buffer.size() % 4 != 0) {
        buffer.push_back(0);
    }
    
    std::vector<uint32_t> words;
    for (size_t i = 0; i < buffer.size(); i += 4) {
        uint32_t word = static_cast<uint8_t>(buffer[i]) |
                       (static_cast<uint8_t>(buffer[i+1]) << 8) |
                       (static_cast<uint8_t>(buffer[i+2]) << 16) |
                       (static_cast<uint8_t>(buffer[i+3]) << 24);
        words.push_back(word);
    }
    
    return words;
}

// ============================================================================
// Test 1: Pipeline Instantiation
// ============================================================================

TEST_CASE("Rv32iPipeline: Hardware module instantiation", "[pipeline][instantiation]") {
    ch::ch_device<Rv32iPipeline> pipeline;
    REQUIRE(pipeline.context()->name() == "rv32i_pipeline");
}

// ============================================================================
// Test 2: Pipeline with TCM integration
// ============================================================================

TEST_CASE("Rv32iPipeline + TCM: Basic execution", "[pipeline][tcm]") {
    ch::core::context ctx("pipeline_tcm_test");
    ch::core::ctx_swap swap(&ctx);
    
    // Create pipeline and TCM modules
    ch::ch_device<Rv32iPipeline> pipeline;
    ch::ch_device<InstrTCM<20, 32>> itcm;
    ch::ch_device<DataTCM<20, 32>> dtcm;
    
    ch::Simulator sim(ctx);
    
    // Initialize I-TCM with simple test program:
    // 0x80000000: addi x1, x0, 42  (0x00000293)
    // 0x80000004: addi x2, x1, 8   (0x00800113)
    // 0x80000008: sw x2, 0(x0)     (0x00200023) - writes to DTCM
    std::vector<uint32_t> program = {
        0x00000293,  // addi x1, x0, 42
        0x00800113,  // addi x2, x1, 8
        0x00200023,  // sw x2, 0(x0)
        0x00000063,  // beq x0, x0, 0 (infinite loop)
    };
    
    // Load program into I-TCM
    for (size_t i = 0; i < program.size(); i++) {
        sim.set_mem_value(itcm.instance(), i, program[i]);
    }
    
    // Set reset
    sim.set_input_value(pipeline.instance().io().rst, 1);
    sim.tick();
    sim.set_input_value(pipeline.instance().io().rst, 0);
    
    for (int i = 0; i < 20; i++) {
        sim.tick();
    }

    SUCCEED("Pipeline executes without crashing");
}

// ============================================================================
// Test 3: riscv-tests execution through pipeline
// ============================================================================

TEST_CASE("rv32ui-p-simple through pipeline", "[pipeline][riscv-tests][rv32ui]") {
    ch::core::context ctx("rv32ui_simple_pipeline");
    ch::core::ctx_swap swap(&ctx);
    
    ch::ch_device<Rv32iPipeline> pipeline;
    ch::ch_device<InstrTCM<20, 32>> itcm;
    ch::ch_device<DataTCM<20, 32>> dtcm;
    
    ch::Simulator sim(ctx);
    
    // Load test binary
    std::string bin_path = "/workspace/project/riscv-tests/build/rv32ui-p-simple.bin";
    std::vector<uint32_t> program;
    
    std::ifstream file(bin_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        SKIP("riscv-tests binary not found at " + bin_path);
    }
    
    program = load_binary_file(bin_path);
    
    // Load into I-TCM
    for (size_t i = 0; i < program.size(); i++) {
        sim.set_mem_value(itcm.instance(), i, program[i]);
    }
    
    // Initialize tohost to 0
    sim.set_mem_value(dtcm.instance(), RV32I_TOHOST_WORD, 0);
    
    // Release reset
    sim.set_input_value(pipeline.instance().io().rst, 1);
    sim.tick();
    sim.set_input_value(pipeline.instance().io().rst, 0);
    
    // Run until tohost changes or max ticks
    uint32_t tohost_value = 0;
    bool test_complete = false;
    
    for (uint32_t tick = 0; tick < MAX_TICKS && !test_complete; tick++) {
        sim.tick();
        
        // Check tohost
        tohost_value = static_cast<uint32_t>(sim.get_mem_value(dtcm.instance(), RV32I_TOHOST_WORD));
        if (tohost_value != 0) {
            test_complete = true;
        }
    }
    
    // Verify result
    REQUIRE(test_complete);
    REQUIRE(tohost_value == 1);  // 1 = pass, 0 = fail
}

// Helper to run a single riscv-tests test through pipeline
bool run_riscv_test_pipeline(const std::string& test_name, const std::string& bin_filename) {
    ch::core::context ctx("rv32ui_" + test_name + "_pipeline");
    ch::core::ctx_swap swap(&ctx);
    
    ch::ch_device<Rv32iPipeline> pipeline;
    ch::ch_device<InstrTCM<20, 32>> itcm;
    ch::ch_device<DataTCM<20, 32>> dtcm;
    
    ch::Simulator sim(ctx);
    
    // Find binary in build directory
    std::string bin_path = "/workspace/project/riscv-tests/build/rv32ui-" + bin_filename;
    std::vector<uint32_t> program;
    
    std::ifstream file(bin_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "SKIP: Binary not found: " << bin_path << std::endl;
        return false;  // Skip, not fail
    }
    
    program = load_binary_file(bin_path);
    
    // Load into I-TCM
    for (size_t i = 0; i < program.size(); i++) {
        sim.set_mem_value(itcm.instance(), i, program[i]);
    }
    
    // Initialize tohost to 0
    sim.set_mem_value(dtcm.instance(), RV32I_TOHOST_WORD, 0);
    
    // Release reset
    sim.set_input_value(pipeline.instance().io().rst, 1);
    sim.tick();
    sim.set_input_value(pipeline.instance().io().rst, 0);
    
    // Run
    uint32_t tohost_value = 0;
    bool test_complete = false;
    
    for (uint32_t tick = 0; tick < MAX_TICKS && !test_complete; tick++) {
        sim.tick();
        
        tohost_value = static_cast<uint32_t>(sim.get_mem_value(dtcm.instance(), RV32I_TOHOST_WORD));
        if (tohost_value != 0) {
            test_complete = true;
        }
    }
    
    if (!test_complete) {
        std::cerr << "FAIL: Test " << test_name << " did not complete in " << MAX_TICKS << " ticks" << std::endl;
        return false;
    }
    
    if (tohost_value != 1) {
        std::cerr << "FAIL: Test " << test_name << " returned " << tohost_value << " (expected 1)" << std::endl;
        return false;
    }
    
    return true;
}

// ============================================================================
// Individual RV32UI tests through pipeline
// ============================================================================

TEST_CASE("rv32ui-p-add through pipeline", "[pipeline][rv32ui][add]") {
    bool passed = run_riscv_test_pipeline("add", "rv32ui-p-add.bin");
    if (passed) {
        SUCCEED("Test passed");
    } else {
        // Check if binary exists
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui-p-add.bin");
        if (!f.is_open()) {
            SKIP("Binary not available");
        }
        FAIL("Test failed");
    }
}

TEST_CASE("rv32ui-p-addi through pipeline", "[pipeline][rv32ui][addi]") {
    bool passed = run_riscv_test_pipeline("addi", "rv32ui-p-addi.bin");
    if (passed) {
        SUCCEED("Test passed");
    } else {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui-p-addi.bin");
        if (!f.is_open()) {
            SKIP("Binary not available");
        }
        FAIL("Test failed");
    }
}

TEST_CASE("rv32ui-p-and through pipeline", "[pipeline][rv32ui][and]") {
    bool passed = run_riscv_test_pipeline("and", "rv32ui-p-and.bin");
    if (passed) {
        SUCCEED("Test passed");
    } else {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui-p-and.bin");
        if (!f.is_open()) {
            SKIP("Binary not available");
        }
        FAIL("Test failed");
    }
}
